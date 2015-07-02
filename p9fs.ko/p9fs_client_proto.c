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
 * Plan9 filesystem (9P2000.u) client implementation.
 *
 * Functions intended to map to client handling of protocol operations are
 * to be defined as p9fs_client_<operation>.
 *
 * See p9fs_proto.h for more details on the protocol specification.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include "p9fs_proto.h"
#include "p9fs_subr.h"

/*
 * version - negotiate protocol version
 *
 *   size[4] Tversion tag[2] msize[4] version[s]
 *   size[4] Rversion tag[2] msize[4] version[s]
 *
 ******** PROTOCOL NOTES
 * Tversion must be the first message sent on the 9P connection; the client
 * may not send any other requests until it is complete.
 *
 * tag[2]: Must always be NOTAG.
 *
 * Tmsize[4]: Suggested maximum size the client will ever generate/receive.
 * Rmsize[4]: Server value, which must be <= Tmsize.
 ********
 *
 * This implementation only handles 9P2000.u, so if any other version is
 * returned, the call will simply bail.
 */
int
p9fs_client_version(struct p9fs_session *p9s)
{
	void *m;
	int error = 0;
	uint32_t max_size = P9_MSG_MAX;

retry:
	m = p9fs_msg_create(Tversion, NOTAG);
	if (m == NULL)
		return (ENOBUFS);

	if (error == 0) /* max_size[4] */
		error = p9fs_msg_add(m, sizeof (uint32_t), &max_size);
	if (error == 0) /* version[s] */
		error = p9fs_msg_add_string(m, UN_VERS, strlen(UN_VERS));
	if (error != 0) {
		p9fs_msg_destroy(m);
		return (error);
	}

	error = p9fs_msg_send(p9s, &m);
	if (error == EMSGSIZE)
		goto retry;

	if (m != NULL) {
		struct p9fs_str p9str;

		error = p9fs_client_error(&m, Rversion);
		if (error != 0)
			return (error);

		p9fs_msg_get_str(m, sizeof (struct p9fs_msg_Rversion), &p9str);
		if (strncmp(p9str.p9str_str, UN_VERS, p9str.p9str_size) != 0) {
			printf("Remote offered incompatible version '%.*s'\n",
			    p9str.p9str_size, p9str.p9str_str);
			error = EINVAL;
		}

		p9fs_msg_destroy(m);
	}

	return (error);
}

/*
 * attach, auth - messages to establish a connection
 *
 *   size[4] Tauth tag[2] afid[4] uname[s] aname[s]
 *   size[4] Rauth tag[2] aqid[13]
 *
 *   size[4] Tattach tag[2] fid[4] afid[4] uname[s] aname[s]
 *   size[4] Rattach tag[2] qid[13]
 *
 * 9P2000.u modifies, according to py9p but not the spec:
 *
 *   size[4] Tattach tag[2] fid[4] afid[4] uname[s] aname[s] uid[4]
 *
 ******** PROTOCOL NOTES
 * Tuname[s]: User identification
 * Taname[s]: File tree being attached
 ******** PROTOCOL NOTES (auth)
 * Tafid[4]: Proposed afid to be used for authentication.
 * Raqid[13]: File of type QTAUTH to execute an authentication protocol.
 *
 * XXX This needs more explanation.
 ******** PROTOCOL NOTES (attach)
 * Tafid[4]: Successful afid from auth, or NOFID if no auth required.
 ********
 *
 * This implementation only supports authentication-free connections for now.
 */
int
p9fs_client_auth(struct p9fs_session *p9s)
{
	return (EINVAL);
}

int
p9fs_client_attach(struct p9fs_session *p9s)
{
	void *m;
	int error = 0;

retry:
	m = p9fs_msg_create(Tattach, 0);
	if (m == NULL)
		return (ENOBUFS);

	if (error == 0) /* fid[4] */
		error = p9fs_msg_add(m, sizeof (uint32_t), &p9s->p9s_fid);
	if (error == 0) /* afid[4] */
		error = p9fs_msg_add(m, sizeof (uint32_t), &p9s->p9s_afid);
	if (error == 0) /* uname[s] */
		error = p9fs_msg_add_string(m, p9s->p9s_uname,
		    strlen(p9s->p9s_uname));
	if (error == 0) /* aname[s] */
		error = p9fs_msg_add_string(m, p9s->p9s_path,
		    strlen(p9s->p9s_path));
	if (error == 0) /* uid[4] */
		error = p9fs_msg_add(m, sizeof (uint32_t), &p9s->p9s_uid);
	if (error != 0) {
		p9fs_msg_destroy(m);
		return (error);
	}

	error = p9fs_msg_send(p9s, &m);
	if (error == EMSGSIZE)
		goto retry;

	if (m != NULL) {
		struct p9fs_qid *qid;

		error = p9fs_client_error(&m, Rattach);
		if (error != 0)
			return (error);

		qid = p9fs_msg_get(m, sizeof (struct p9fs_msg_hdr));
		bcopy(qid, (void *)&p9s->p9s_qid, sizeof (struct p9fs_qid));

		p9fs_msg_destroy(m);
	}

	return (error);
}

/*
 * clunk - forget about a fid
 *
 *   size[4] Tclunk tag[2] fid[4]
 *   size[4] Rclunk tag[2]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 */
int
p9fs_client_clunk(void)
{
	return (EINVAL);
}

/*
 * error - return an error
 *
 *   size[4] Rerror tag[2] ename[s] errno[4]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 * This is primarily used by other functions as a means of checking for
 * error conditions, so it also checks whether the expected R command is
 * being transmitted.
 *
 * Return codes:
 * >0: Error return from the server.  May be EINVAL if the wrong R command
 *     was returned.
 *  0: No error; the expected R command was returned.
 *
 * NB: m is consumed if an error is returned, regardless of type.
 */
int
p9fs_client_error(void **mp, enum p9fs_msg_type expected_type)
{
	void *m = *mp;
	struct p9fs_msg_hdr *hdr = p9fs_msg_get(m, 0);
	int errcode = EINVAL;

	if (hdr->hdr_type == expected_type)
		return (0);

	if (hdr->hdr_type == Rerror) {
		size_t off = sizeof (struct p9fs_msg_hdr);
		struct p9fs_str p9str;
		uint32_t errcode;

		p9fs_msg_get_str(m, off, &p9str);
		off += sizeof (p9str.p9str_size);
		errcode = *(uint32_t *)p9fs_msg_get(m, off + p9str.p9str_size);
	}
	p9fs_msg_destroy(m);
	*mp = NULL;
	return (errcode);
}

/*
 * flush - abort a message
 *
 *   size[4] Tflush tag[2] oldtag[2]
 *   size[4] Rflush tag[2]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 */
int
p9fs_client_flush(void)
{
	return (EINVAL);
}

/*
 * open, create - prepare a fid for I/O on an existing or new file
 *
 *   size[4] Topen tag[2] fid[4] mode[1]
 *   size[4] Ropen tag[2] qid[13] iounit[4]
 *
 *   size[4] Tcreate tag[2] fid[4] name[s] perm[4] mode[1]
 *   size[4] Rcreate tag[2] qid[13] iounit[4]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 */
int
p9fs_client_open(void)
{
	return (EINVAL);
}

int
p9fs_client_create(void)
{
	return (EINVAL);
}

/*
 * read, write - transfer data to and from a file
 *
 *   size[4] Tread tag[2] fid[4] offset[8] count[4]
 *   size[4] Rread tag[2] count[4] data[count]
 *
 *   size[4] Twrite tag[2] fid[4] offset[8] count[4] data[count]
 *   size[4] Rwrite tag[2] count[4]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 */
int
p9fs_client_read(void)
{
	return (EINVAL);
}

int
p9fs_client_write(void)
{
	return (EINVAL);
}

/*
 * remove - remove a file from a server
 *
 *   size[4] Tremove tag[2] fid[4]
 *   size[4] Rremove tag[2]
 *
 ******** PROTOCOL NOTES
 *
 ********
 *
 */
int
p9fs_client_remove(void)
{
	return (EINVAL);
}

/* Parse the 9P2000-only portion of Rstat from the message. */
static void
p9fs_client_parse_std_stat(void *m, struct p9fs_stat_payload *pay, size_t *off)
{
	*off = sizeof (struct p9fs_msg_hdr);

	pay->pay_stat = p9fs_msg_get(m, *off);

	*off += sizeof (struct p9fs_stat);
	p9fs_msg_get_str(m, *off, &pay->pay_name);

	*off += sizeof (uint16_t) + pay->pay_name.p9str_size;
	p9fs_msg_get_str(m, *off, &pay->pay_uid);

	*off += sizeof (uint16_t) + pay->pay_uid.p9str_size;
	p9fs_msg_get_str(m, *off, &pay->pay_gid);

	*off += sizeof (uint16_t) + pay->pay_gid.p9str_size;
	p9fs_msg_get_str(m, *off, &pay->pay_muid);

	*off += sizeof (uint16_t) + pay->pay_muid.p9str_size;
}

/*
 * stat, wstat - inquire or change file attributes
 *
 *   size[4] Tstat tag[2] fid[4]
 *   size[4] Rstat tag[2] stat[n]
 *
 *   size[4] Twstat tag[2] fid[4] stat[n]
 *   size[4] Rwstat tag[2]
 *
 ******** PROTOCOL NOTES
 * Tfid[4]: Fid to perform the stat call on.
 ********
 *
 * This function is used for both VOP_GETATTR(9) and VOP_READDIR(9), which
 * have very different output requirements.  As such, its primary job is to
 * perform the stat call and parse the response payload.
 *
 * For each Rstat[n] entry returned, cb will be called with pointers to:
 * - struct p9fs_stat, for the main body of the stat response
 * - struct p9fs_str pointers, for the name/uid/gid/muid/extension strings.
 * - struct p9fs_stat_u_footer, for the tail of the 9P2000.u stat response.
 *
 * In order to parse the entire payload, each of these must be teased from
 * the payload in turn, until the payload is exhausted.
 *
 * If cb returns non-zero, parsing will end immediately and the error return
 * will be forwarded to the caller.
 */
int
p9fs_client_stat(struct p9fs_session *p9s, uint32_t fid, Rstat_callback cb,
    void *cbarg)
{
	void *m;
	int error = 0;

retry:
	m = p9fs_msg_create(Tstat, 0);
	if (m == NULL)
		return (ENOBUFS);

	if (error == 0) /* fid[4] */
		error = p9fs_msg_add(m, sizeof (uint32_t), &fid);
	if (error != 0) {
		p9fs_msg_destroy(m);
		return (error);
	}

	error = p9fs_msg_send(p9s, &m);
	if (error == EMSGSIZE)
		goto retry;

	if (m != NULL) {
		int32_t len = p9fs_msg_payload_len(m);
		size_t off = 0;
		struct p9fs_stat_u_payload upay = {};

		error = p9fs_client_error(&m, Rstat);
		if (error != 0)
			return (error);

		while (error == 0 && len > off) {
			struct p9fs_stat_payload *pay = &upay.upay_std;

			p9fs_client_parse_std_stat(m, pay, &off);
			p9fs_msg_get_str(m, off, &upay.upay_extension);

			off += sizeof (uint16_t) + upay.upay_extension.p9str_size;
			upay.upay_footer = p9fs_msg_get(m, off);
			off += sizeof (upay.upay_footer);

			error = cb(&upay, cbarg);
		}
		p9fs_msg_destroy(m);
	}

	return (error);
}

int
p9fs_client_wstat(void)
{
	return (EINVAL);
}

/*
 * walk - descend a directory hierarchy
 *
 *   size[4] Twalk tag[2] fid[4] newfid[4] nwname[2] nwname*(wname[s])
 *   size[4] Rwalk tag[2] nwqid[2] nwqid*(qid[13])
 *
 ******** PROTOCOL NOTES
 * Tfid[4] must be a fid for a directory
 * Tnewfid[4] is the proposed fid for the thing being walked to.
 * Tnwname[2] is the number of things to walk down; newfid is for the last.
 * T*wname[s] are the names of those things.
 ********
 *
 * For the purposes of p9fs, this call will only ever be used for a single
 * walk at a time, due to the nature of POSIX VFS.
 */
int
p9fs_client_walk(struct p9fs_session *p9s, uint32_t fid, uint32_t *newfid,
    size_t namelen, const char *namestr)
{
	return (EINVAL);
}
