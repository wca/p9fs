/*-
 * Copyright (c) 2015 Will Andrews.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS        
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR       
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS        
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF             
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS         
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN          
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)          
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       
 * POSSIBILITY OF SUCH DAMAGE.                                                      
 */

/*
 * Plan9 filesystem (9P2000.u) subroutines.  This file is intended primarily
 * for Plan9-specific details.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <netinet/in.h>
#include <sys/limits.h>
#include <sys/vnode.h>

#include "p9fs_proto.h"
#include "p9fs_subr.h"

static MALLOC_DEFINE(M_P9REQ, "p9fsreq", "Request structures for p9fs");

/*
 * Plan 9 message handling.  This is primarily intended as a means of
 * performing marshalling/unmarshalling.
 *
 * sosend()/sorecv() perform operations using struct uio or struct mbuf,
 * which manage I/O vectors.  Since each Plan9 message can be considered a
 * vector of arbitrary data, they fit the bill.
 *
 * mbufs perform their own internal memory management and therefore are
 * probably the better fit.
 *
 * XXX: Need to figure out how to enforce little endian ordering, since that
 *      is part of the spec.
 */

void *
p9fs_msg_create(enum p9fs_msg_type p9_type, uint16_t tag)
{
	struct mbuf *m;

	m = m_gethdr(M_WAITOK, MT_DATA);
	/* Leave space to prepend the size of the packet. */
	m->m_data += sizeof (uint32_t);
	if (m != NULL) {
		if (m_append(m, sizeof (uint8_t), (void *)&p9_type) == 0)
			goto fail;
		if (m_append(m, sizeof (tag), (void *)&tag) == 0)
			goto fail;
	}
	return (m);

fail:
	m_freem(m);
	return (NULL);
}

int
p9fs_msg_add(void *mp, size_t len, void *cp)
{
	struct mbuf *m = mp;

	if (m_append(m, len, cp) == 0)
		return (EINVAL);
	return (0);
}

int
p9fs_msg_add_string(void *mp, const char *str, uint16_t len)
{
	struct mbuf *m = mp;

	if (m_append(m, 2, (uint8_t *)&len) == 0)
		return (EINVAL);
	if (m_append(m, len, str) == 0)
		return (EINVAL);
	return (0);
}

int
p9fs_msg_add_uio(void *mp, struct uio *uio, uint32_t count)
{
	struct mbuf *m = mp;
	struct mbuf *uiom;

	uiom = m_uiotombuf(uio, M_WAITOK, count, /*align*/ 0, /*flags*/ 0);
	m_cat(m, uiom);

	return (0);
}

/*
 * mp is the Plan9 payload on input; on output it is the response payload.
 */
int
p9fs_msg_send(struct p9fs_session *p9s, void **mp)
{
	int error, flags;
	struct uio *uio = NULL;
	struct mbuf *m = *mp;
	struct mbuf *control = NULL;
	struct thread *td = curthread;
	struct p9fs_recv *p9r = &p9s->p9s_recv;
	struct p9fs_req *req;
	int timo = 30 * hz;
	uint16_t tag;

	req = malloc(sizeof (struct p9fs_req), M_P9REQ, M_WAITOK | M_ZERO);

	/* Prepend the packet size, then re-fetch the tag. */
	m->m_pkthdr.len = m_length(m, NULL);
	M_PREPEND(m, sizeof (uint32_t), M_WAITOK);
	*mtod(m, uint32_t *) = m->m_pkthdr.len;
	m_copydata(m, offsetof(struct p9fs_msg_hdr, hdr_tag),
	    sizeof (tag), (void *)&tag);
	req->req_tag = tag;

	mtx_lock(&p9s->p9s_lock);
	if (p9s->p9s_state >= P9S_CLOSING) {
		mtx_unlock(&p9s->p9s_lock);
		*mp = NULL;
		p9fs_msg_destroy(p9s, m);
		free(req, M_P9REQ);
		return (ECONNABORTED);
	}
	p9s->p9s_threads++;
	TAILQ_INSERT_TAIL(&p9r->p9r_reqs, req, req_link);
	mtx_unlock(&p9s->p9s_lock);

	flags = 0;
	error = sosend(p9s->p9s_sock, &p9s->p9s_sockaddr, uio, m, control,
	    flags, td);
	*mp = NULL;
	if (error == EMSGSIZE) {
		SOCKBUF_LOCK(&p9s->p9s_sock->so_snd);
		sbwait(&p9s->p9s_sock->so_snd);
		SOCKBUF_UNLOCK(&p9s->p9s_sock->so_snd);
	}

	mtx_lock(&p9s->p9s_lock);
	/*
	 * Check to see if a response was generated for this request while
	 * waiting for the lock.
	 */
	if (req->req_error != 0 && error == 0)
		error = req->req_error;

	if (error == 0 && req->req_msg == NULL)
		error = msleep(req, &p9s->p9s_lock, PCATCH, "p9reqsend", timo);

	TAILQ_REMOVE(&p9r->p9r_reqs, req, req_link);
	if (error == 0)
		error = req->req_error;

	/* Ensure any response is disposed of in case of a local error. */
	*mp = req->req_msg;
	if (error != 0 && *mp != NULL) {
		m_freem(*mp);
		*mp = NULL;
	}

	p9s->p9s_threads--;
	wakeup(p9s);
	mtx_unlock(&p9s->p9s_lock);

	free(req, M_P9REQ);

	return (error);
}

void
p9fs_msg_recv(struct p9fs_session *p9s)
{
	struct p9fs_recv *p9r = &p9s->p9s_recv;
	struct mbuf *control, *m;
	struct uio uio;
	int error, rcvflag;
	struct sockaddr **psa = NULL;

	p9r->p9r_soupcalls++;

again:
	/* Is the socket still waiting for a new record's size? */
	if (p9r->p9r_resid == 0) {
		if (sbavail(&p9s->p9s_sock->so_rcv) < sizeof (p9r->p9r_resid)
		 || (p9s->p9s_sock->so_rcv.sb_state & SBS_CANTRCVMORE) != 0
		 || p9s->p9s_sock->so_error != 0)
			goto out;

		uio.uio_resid = sizeof (p9r->p9r_resid);
	} else {
		uio.uio_resid = p9r->p9r_resid;
	}

	/* Drop the sockbuf lock and do the soreceive call. */
	SOCKBUF_UNLOCK(&p9s->p9s_sock->so_rcv);
	rcvflag = MSG_DONTWAIT | MSG_SOCALLBCK;
	error = soreceive(p9s->p9s_sock, psa, &uio, &m, &control, &rcvflag);
	SOCKBUF_LOCK(&p9s->p9s_sock->so_rcv);

	/* Process errors from soreceive(). */
	if (error == EWOULDBLOCK)
		goto out;
	if (error != 0 || uio.uio_resid > 0) {
		struct p9fs_req *req;

		if (error == 0)
			error = ECONNRESET;

		mtx_lock(&p9s->p9s_lock);
		p9r->p9r_error = error;
		TAILQ_FOREACH(req, &p9r->p9r_reqs, req_link) {
			req->req_error = error;
			wakeup(req);
		}
		mtx_unlock(&p9s->p9s_lock);
		goto out;
	}

	if (p9r->p9r_resid == 0) {
		/* Copy in the size, subtract itself, and reclaim the mbuf. */
		m_copydata(m, 0, sizeof (p9r->p9r_size),
		    (uint8_t *)&p9r->p9r_size);
		if (p9r->p9r_size < sizeof (struct p9fs_msg_hdr)) {
			/* XXX Reject the packet as illegal? */
		}
		p9r->p9r_resid = p9r->p9r_size - sizeof (p9r->p9r_size);
		p9r->p9r_msg = m;

		/* Record size is known now; retrieve the rest. */
		goto again;
	}

	/* Chain the message to the end. */
	m_last(p9r->p9r_msg)->m_next = m;

	/* If we have a complete record, match it to a request via tag. */
	p9r->p9r_resid = uio.uio_resid;
	if (p9r->p9r_resid == 0) {
		int found = 0;
		uint16_t tag;
		struct p9fs_req *req;

		m_copydata(p9r->p9r_msg, offsetof(struct p9fs_msg_hdr, hdr_tag),
		    sizeof (uint16_t), (void *)&tag);

		mtx_lock(&p9s->p9s_lock);
		TAILQ_FOREACH(req, &p9r->p9r_reqs, req_link) {
			if (req->req_tag == tag) {
				found = 1;
				req->req_msg = m_pullup(p9r->p9r_msg,
				    p9r->p9r_size);
				/* Zero tag to skip any duplicate replies. */
				req->req_tag = 0;
				wakeup(req);
				break;
			}
		}
		mtx_unlock(&p9s->p9s_lock);
		if (found == 0) {
			m_freem(p9r->p9r_msg);
		}
		p9r->p9r_msg = NULL;
	}

out:
	p9r->p9r_soupcalls--;
	return;
}

void
p9fs_msg_get(void *mp, size_t *offp, void **ptr, size_t data_sz)
{
	*ptr = mtod((struct mbuf *)mp, uint8_t *) + *offp;
	*offp += data_sz;
}

void
p9fs_msg_get_str(void *mp, size_t *offp, struct p9fs_str *str)
{
	uint8_t *buf;

	p9fs_msg_get(mp, offp, (void **)&buf, sizeof (uint16_t));
	str->p9str_size = *(uint16_t *)buf;
	str->p9str_str = (char *)(buf + sizeof (str->p9str_size));
	*offp += str->p9str_size;
}

int32_t
p9fs_msg_payload_len(void *mp)
{
	return (((struct mbuf *)mp)->m_len);
}

void
p9fs_msg_destroy(struct p9fs_session *p9s, void *mp)
{
	struct mbuf *m = mp;
	uint16_t *tag;
	size_t off = offsetof(struct p9fs_msg_hdr, hdr_tag);

	p9fs_msg_get(mp, &off, (void **)&tag, sizeof (*tag));
	if (*tag != NOTAG)
		p9fs_reltag(p9s, *tag);
	m_freem(m);
}

void
p9fs_init_session(struct p9fs_session *p9s)
{
	mtx_init(&p9s->p9s_lock, "p9s->p9s_lock", NULL, MTX_DEF);
	TAILQ_INIT(&p9s->p9s_recv.p9r_reqs);
	(void) strlcpy(p9s->p9s_uname, "root", sizeof ("root"));
	p9s->p9s_uid = 0;
	p9s->p9s_afid = NOFID;
	/*
	 * XXX Although there can be more FIDs, the unit accounting subroutines
	 *     flatten these values to int arguments rather than u_int.
	 *     This will limit the number of outstanding vnodes for a p9fs
	 *     mount to 64k.
	 */
	p9s->p9s_fids = new_unrhdr(1, UINT16_MAX, &p9s->p9s_lock);
	p9s->p9s_tags = new_unrhdr(1, UINT16_MAX - 1, &p9s->p9s_lock);
	p9s->p9s_socktype = SOCK_STREAM;
	p9s->p9s_proto = IPPROTO_TCP;
}

void
p9fs_close_session(struct p9fs_session *p9s)
{
	mtx_lock(&p9s->p9s_lock);
	if (p9s->p9s_sock != NULL) {
		struct p9fs_recv *p9r = &p9s->p9s_recv;
		struct sockbuf *rcv = &p9s->p9s_sock->so_rcv;

		p9s->p9s_state = P9S_CLOSING;
		mtx_unlock(&p9s->p9s_lock);

		SOCKBUF_LOCK(rcv);
		soupcall_clear(p9s->p9s_sock, SO_RCV);
		while (p9r->p9r_soupcalls > 0)
			(void) msleep(&p9r->p9r_soupcalls, SOCKBUF_MTX(rcv),
			    0, "p9rcvup", 0);
		SOCKBUF_UNLOCK(rcv);
		(void) soclose(p9s->p9s_sock);

		/*
		 * XXX Can there really be any such threads?  If vflush()
		 *     has completed, there shouldn't be.  See if we can
		 *     remove this and related code later.
		 */
		mtx_lock(&p9s->p9s_lock);
		while (p9s->p9s_threads > 0)
			msleep(p9s, &p9s->p9s_lock, 0, "p9sclose", 0);
		p9s->p9s_state = P9S_CLOSED;
	}
	mtx_unlock(&p9s->p9s_lock);

	/* Would like to explicitly clunk ROOTFID here, but soupcall gone. */
	delete_unrhdr(p9s->p9s_fids);
	delete_unrhdr(p9s->p9s_tags);
}

/* FID & tag management.  Makes use of subr_unit, since it's the best fit. */
uint32_t
p9fs_getfid(struct p9fs_session *p9s)
{
	return (alloc_unr(p9s->p9s_fids));
}
void
p9fs_relfid(struct p9fs_session *p9s, uint32_t fid)
{
	free_unr(p9s->p9s_fids, fid);
}
uint16_t
p9fs_gettag(struct p9fs_session *p9s)
{
	return (alloc_unr(p9s->p9s_tags));
}
void
p9fs_reltag(struct p9fs_session *p9s, uint16_t tag)
{
	free_unr(p9s->p9s_tags, tag);
}
