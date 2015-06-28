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

#include "p9fs_proto.h"
#include "p9fs_subr.h"

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

/*
 * NB: consumes the p9 message; caller should never destroy if it sends.
 */
int
p9fs_msg_send(struct p9fs_session *p9s, void *mp)
{
	int error;
	int flags = 0;
	struct uio *uio = NULL;
	struct mbuf *m = mp;
	struct mbuf *control = NULL;
	struct thread *td = curthread;

	/* Prepend the packet size. */
	m->m_pkthdr.len = m_length(m, NULL);
	M_PREPEND(m, sizeof (uint32_t), M_WAITOK);
	*mtod(m, uint32_t *) = m->m_pkthdr.len;

	error = sosend(p9s->p9s_sock, &p9s->p9s_sockaddr, uio, m, control,
	    flags, td);

	if (error == EMSGSIZE) {
		SOCKBUF_LOCK(&p9s->p9s_sock->so_snd);
		sbwait(&p9s->p9s_sock->so_snd);
		SOCKBUF_UNLOCK(&p9s->p9s_sock->so_snd);
	}

	return (error);
}

void
p9fs_msg_destroy(void *mp)
{
	struct mbuf *m = mp;

	m_freem(m);
}
