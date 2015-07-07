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
 * Plan9 filesystem (9P2000.u) subroutines.
 */

#ifndef	__P9FS_SUBR_H__
#define	__P9FS_SUBR_H__

/* Plan9 message handling routines using opaque handles */
void *p9fs_msg_create(enum p9fs_msg_type, uint16_t);
int p9fs_msg_add(void *, size_t, void *);
int p9fs_msg_add_string(void *, const char *, uint16_t);
int p9fs_msg_send(struct p9fs_session *, void **);
void p9fs_msg_recv(struct p9fs_session *);
void p9fs_msg_get(void *, size_t *, void **, size_t);
void p9fs_msg_get_str(void *, size_t *, struct p9fs_str *);
void p9fs_msg_destroy(struct p9fs_session *, void *);
int32_t p9fs_msg_payload_len(void *);
void p9fs_init_session(struct p9fs_session *);
void p9fs_close_session(struct p9fs_session *);
uint32_t p9fs_getfid(struct p9fs_session *);
void p9fs_relfid(struct p9fs_session *, uint32_t);
uint16_t p9fs_gettag(struct p9fs_session *);
void p9fs_reltag(struct p9fs_session *, uint16_t);

#endif
