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
 * Plan9 filesystem (9P2000.u) implementation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/sockopt.h>
#include <sys/socketvar.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/vnode.h>

static const char *p9_opts[] = {
	"addr",
	"debug",
	"hostname",
	"path",
	"proto",
};

struct p9fsmount {
	struct sockaddr p9_sockaddr;
	int p9_sockaddr_len;
	int p9_socktype;
	int p9_proto;
	int p9_debuglevel;
	struct socket *p9_socket;
	struct mount *p9_mountp;
	char p9_path[MAXPATHLEN];
	char p9_hostname[256];
};
#define	VFSTOP9(mp) ((mp)->mnt_data)

static MALLOC_DEFINE(M_P9MNT, "p9fsmount", "Mount structures for p9fs");

static int
p9fs_mount_parse_opts(struct mount *mp)
{
	struct p9fsmount *p9mp = VFSTOP9(mp);
	char *opt;
	int error = EINVAL;
	int fromnamelen, ret;

	if (vfs_getopt(mp->mnt_optnew, "debug", (void **)&opt, NULL) == 0) {
		if (opt == NULL) {
			vfs_mount_error(mp, "must specify value for debug");
			goto out;
		}
		ret = sscanf(opt, "%d", &p9mp->p9_debuglevel);
		if (ret != 1 || p9mp->p9_debuglevel < 0) {
			vfs_mount_error(mp, "illegal debug value: %s", opt);
			goto out;
		}
	}

	/* Flags beyond here are not supported for updates. */
	if (mp->mnt_flag & MNT_UPDATE)
		return (0);

	ret = vfs_getopt(mp->mnt_optnew, "addr",
	    (void **)&p9mp->p9_sockaddr, &p9mp->p9_sockaddr_len);
	if (ret != 0) {
		vfs_mount_error(mp, "No server address");
		goto out;
	}
	if (p9mp->p9_sockaddr_len > SOCK_MAXADDRLEN) {
		error = ENAMETOOLONG;
		goto out;
	}

	ret = vfs_getopt(mp->mnt_optnew, "hostname", (void **)&opt, NULL);
	if (ret != 0) {
		vfs_mount_error(mp, "No remote host");
		goto out;
	}
	ret = strlcpy(p9mp->p9_hostname, opt, sizeof (p9mp->p9_hostname));
	if (ret >= sizeof (p9mp->p9_hostname)) {
		error = ENAMETOOLONG;
		goto out;
	}

	ret = vfs_getopt(mp->mnt_optnew, "path", (void **)&opt, NULL);
	if (ret != 0) {
		vfs_mount_error(mp, "No remote path");
		goto out;
	}
	ret = strlcpy(p9mp->p9_path, opt, sizeof (p9mp->p9_path));
	if (ret >= sizeof (p9mp->p9_path)) {
		error = ENAMETOOLONG;
		goto out;
	}

	fromnamelen = sizeof (mp->mnt_stat.f_mntfromname);
	ret = snprintf(mp->mnt_stat.f_mntfromname, fromnamelen,
	    "%s:%s", p9mp->p9_hostname, p9mp->p9_path);
	if (ret >= fromnamelen) {
		error = ENAMETOOLONG;
		goto out;
	}

	if (vfs_getopt(mp->mnt_optnew, "proto", (void **)&opt, NULL) == 0) {
		if (strcasecmp(opt, "tcp") == 0) {
			p9mp->p9_socktype = SOCK_STREAM;
			p9mp->p9_proto = IPPROTO_TCP;
		} else if (strcasecmp(opt, "udp") == 0) {
			p9mp->p9_socktype = SOCK_DGRAM;
			p9mp->p9_proto = IPPROTO_UDP;
		} else {
			vfs_mount_error(mp, "illegal proto: %s", opt);
			goto out;
		}
	}

	error = 0;

out:
	return (error);
}

static void
p9fs_setsockopt(struct socket *so, int sopt_name)
{
	struct sockopt sopt = { 0 };
	int one = 1;

	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = sopt_name;
	sopt.sopt_val = &one;
	sopt.sopt_valsize = sizeof(one);
	sosetopt(so, &sopt);
}

/*
 * XXX Need to implement reconnecting as necessary.  If that were to be
 *     needed, most likely all current vnodes would have to be renegotiated
 *     or otherwise invalidated (a la NFS "stale file handle").
 */
static int
p9fs_connect(struct mount *mp)
{
	struct p9fsmount *p9mp = VFSTOP9(mp);
	struct socket *so;
	int error;

	error = socreate(p9mp->p9_sockaddr.sa_family, &p9mp->p9_socket,
	    p9mp->p9_socktype, p9mp->p9_proto, curthread->td_ucred, curthread);
	if (error != 0) {
		vfs_mount_error(mp, "socreate");
		goto out;
	}

	so = p9mp->p9_socket;
	error = soconnect(so, &p9mp->p9_sockaddr, curthread);
	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = msleep(&so->so_timeo, SOCK_MTX(so), PSOCK | PCATCH,
		    "connec", 0);
		if (error)
			break;
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	SOCK_UNLOCK(so);
	if (error) {
		vfs_mount_error(mp, "soconnect");
		if (error == EINTR)
			so->so_state &= ~SS_ISCONNECTING;
		goto out;
	}

	if (so->so_proto->pr_flags & PR_CONNREQUIRED)
		p9fs_setsockopt(so, SO_KEEPALIVE);
	if (so->so_proto->pr_protocol == IPPROTO_TCP)
		p9fs_setsockopt(so, TCP_NODELAY);

	/* XXX Do we need this?  Probably?  See clnt_vc_soupcall(). */
#if 0
	SOCKBUF_LOCK(&so->so_rcv);
	soupcall_set(so, SO_RCV, p9fs_sock_upcall, p9mp);
	SOCKBUF_UNLOCK(&so->so_rcv);
#endif

	error = 0;

out:
	return (error);
}

static int
p9fs_negotiate(struct p9fsmount *p9mp)
{
	int error = EINVAL;

	/* Call into p9fs_client_version/auth/attach */

	return (error);
}

static void
p9fs_mount_alloc(struct p9fsmount **p9mpp, struct mount *mp)
{
	*p9mpp = malloc(sizeof (struct p9fsmount), M_P9MNT, M_WAITOK | M_ZERO);
	mp->mnt_data = *p9mpp;
	(*p9mpp)->p9_mountp = mp;

	/* Default values. */
	(*p9mpp)->p9_socktype = SOCK_STREAM;
	(*p9mpp)->p9_proto = IPPROTO_TCP;
}

static int
p9fs_unmount(struct mount *mp, int mntflags)
{
	struct p9fsmount *p9mp = VFSTOP9(mp);
	int error, flags, i;

	error = 0;
	flags = 0;
	if (p9mp == NULL)
		return (0);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	for (i = 0; i < 10; i++) {
		error = vflush(mp, 0, flags, curthread);
		if (error != 0 && (mntflags & MNT_FORCE) == 0)
			break;
		/* Sleep until interrupted or 1 second expires. */
		error = tsleep(&error, PSOCK, "p9unmnt", 1);
		if (error == EINTR)
			break;
	}
	if (error != 0)
		goto out;

	if (p9mp->p9_socket != NULL)
		(void) soclose(p9mp->p9_socket);

	free(p9mp, M_P9MNT);
	mp->mnt_data = NULL;

out:
	return (error);
}

static int
p9fs_mount(struct mount *mp)
{
	struct p9fsmount *p9mp;
	int error;

	error = EINVAL;
	if (vfs_filteropt(mp->mnt_optnew, p9_opts))
		goto out;

	if (mp->mnt_flag & MNT_UPDATE)
		return (p9fs_mount_parse_opts(mp));

	p9fs_mount_alloc(&p9mp, mp);

	error = p9fs_mount_parse_opts(mp);
	if (error != 0)
		goto out;

	error = p9fs_connect(mp);
	if (error != 0)
		goto out;

	error = p9fs_negotiate(p9mp);
	if (error != 0)
		goto out;

out:
	if (error != 0)
		(void) p9fs_unmount(mp, 0);
	return (error);
}

static int
p9fs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	return (EINVAL);
}

static int
p9fs_statfs(struct mount *mp, struct statfs *sbp)
{
	return (EINVAL);
}

static int
p9fs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	return (EINVAL);
}

static int
p9fs_sync(struct mount *mp, int waitfor)
{
	return (EINVAL);
}

struct vfsops p9fs_vfsops = {
	.vfs_mount =	p9fs_mount,
	.vfs_unmount =	p9fs_unmount,
	.vfs_root =	p9fs_root,
	.vfs_statfs =	p9fs_statfs,
	.vfs_fhtovp =	p9fs_fhtovp,
	.vfs_sync =	p9fs_sync,
};
VFS_SET(p9fs_vfsops, p9fs, VFCF_JAIL);
