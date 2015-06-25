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
static int
p9fs_client_version(void)
{
	return (EINVAL);
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
static int
p9fs_client_auth(void)
{
	return (EINVAL);
}

static int
p9fs_client_attach(void)
{
	return (EINVAL);
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
static int
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
 */
static int
p9fs_client_error(void)
{
	return (EINVAL);
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
static int
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
static int
p9fs_client_open(void)
{
	return (EINVAL);
}

static int
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
static int
p9fs_client_read(void)
{
	return (EINVAL);
}

static int
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
static int
p9fs_client_remove(void)
{
	return (EINVAL);
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
 *
 ********
 *
 */
static int
p9fs_client_stat(void)
{
	return (EINVAL);
}

static int
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
 *
 ********
 *
 */
static int
p9fs_client_walk(void)
{
	return (EINVAL);
}
