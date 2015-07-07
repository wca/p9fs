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
 * Plan9 filesystem (9P2000.u) node operations implementation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include "p9fs_proto.h"
#include "p9fs_subr.h"

struct vop_vector p9fs_vnops;
static MALLOC_DEFINE(M_P9NODE, "p9fs_node", "p9fs node structures");

/*
 * Get a p9node.  Nodes are represented by (fid, qid) tuples in 9P2000.
 * Fids are assigned by the client, while qids are assigned by the server.
 *
 * The caller is expected to have generated the FID via p9fs_getfid() and
 * obtained the QID from the server via p9fs_client_walk() and friends.
 */
int
p9fs_nget(struct mount *mp, struct p9fs_session *p9s,
    uint32_t fid, struct p9fs_qid *qid,
    int lkflags, struct p9fs_node **npp)
{
	int error = 0;
	struct p9fs_node *np;
	struct vnode *vp, *nvp;
	struct thread *td = curthread;

	*npp = NULL;
	error = vfs_hash_get(mp, fid, lkflags, td, &vp, NULL, NULL);
	if (error != 0)
		return (error);
	if (vp != NULL) {
		*npp = vp->v_data;
		return (0);
	}

	np = malloc(sizeof (struct p9fs_node), M_P9NODE, M_WAITOK);
	getnewvnode_reserve(1);

	error = getnewvnode("p9fs", mp, &p9fs_vnops, &nvp);
	if (error != 0) {
		getnewvnode_drop_reserve();
		free(np, M_P9NODE);
		return (error);
	}
	vp = nvp;
	vn_lock(vp, LK_EXCLUSIVE);

	error = insmntque(nvp, mp);
	if (error != 0) {
		/* vp was vput()'d by insmntque() */
		return (error);
	}
	error = vfs_hash_insert(nvp, fid, lkflags, td, &nvp, NULL, NULL);
	if (error != 0)
		return (error);
	if (nvp != NULL) {
		*npp = nvp->v_data;
		/* vp was vput()'d by vfs_hash_insert() */
		return (0);
	}

	/* Our vnode is the winner.  Set up the new p9node for it. */
	np->p9n_fid = fid;
	np->p9n_session = p9s;
	np->p9n_vnode = vp;
	bcopy(qid, &np->p9n_qid, sizeof (*qid));
	*npp = np;

	return (error);
}

static int
p9fs_lookup(struct vop_cachedlookup_args *ap)
{
	return (EINVAL);
}

static int
p9fs_create(struct vop_create_args *ap)
{
	return (EINVAL);
}

static int
p9fs_mknod(struct vop_mknod_args *ap)
{
	return (EINVAL);
}

static int
p9fs_open(struct vop_open_args *ap)
{
	return (EINVAL);
}

static int
p9fs_close(struct vop_close_args *ap)
{
	return (EINVAL);
}

static int
p9fs_access(struct vop_access_args *ap)
{
	return (EINVAL);
}

static int
p9fs_getattr(struct vop_getattr_args *ap)
{
	return (EINVAL);
}

static int
p9fs_setattr(struct vop_setattr_args *ap)
{
	return (EINVAL);
}

static int
p9fs_read(struct vop_read_args *ap)
{
	return (EINVAL);
}

static int
p9fs_write(struct vop_write_args *ap)
{
	return (EINVAL);
}

static int
p9fs_fsync(struct vop_fsync_args *ap)
{
	return (EINVAL);
}

static int
p9fs_remove(struct vop_remove_args *ap)
{
	return (EINVAL);
}

static int
p9fs_link(struct vop_link_args *ap)
{
	return (EINVAL);
}

static int
p9fs_rename(struct vop_rename_args *ap)
{
	return (EINVAL);
}

static int
p9fs_mkdir(struct vop_mkdir_args *ap)
{
	return (EINVAL);
}

static int
p9fs_rmdir(struct vop_rmdir_args *ap)
{
	return (EINVAL);
}

static int
p9fs_symlink(struct vop_symlink_args *ap)
{
	return (EINVAL);
}

static int
p9fs_readdir(struct vop_readdir_args *ap)
{
	return (EINVAL);
}

static int
p9fs_readlink(struct vop_readlink_args *ap)
{
	return (EINVAL);
}

static int
p9fs_inactive(struct vop_inactive_args *ap)
{
	return (0);
}

static int
p9fs_reclaim(struct vop_reclaim_args *ap)
{
	struct p9fs_node *np = ap->a_vp->v_data;
	int error;

	/* Failure should never happen here. */
	error = p9fs_client_clunk(np->p9n_session, np->p9n_fid);
	if (error != 0) {
		printf("%s: error %d\n", __func__, error);
		//return (error);
	}
	p9fs_relfid(np->p9n_session, np->p9n_fid);

	return (0);
}

static int
p9fs_print(struct vop_print_args *ap)
{
	return (EINVAL);
}

static int
p9fs_pathconf(struct vop_pathconf_args *ap)
{
	return (EINVAL);
}

static int
p9fs_vptofh(struct vop_vptofh_args *ap)
{
	return (EINVAL);
}


struct vop_vector p9fs_vnops = {
	.vop_default =		&default_vnodeops,
	.vop_lookup =		vfs_cache_lookup,
	.vop_cachedlookup =	p9fs_lookup,
	.vop_create =		p9fs_create,
	.vop_mknod =		p9fs_mknod,
	.vop_open =		p9fs_open,
	.vop_close =		p9fs_close,
	.vop_access =		p9fs_access,
	.vop_getattr =		p9fs_getattr,
	.vop_setattr =		p9fs_setattr,
	.vop_read =		p9fs_read,
	.vop_write =		p9fs_write,
	.vop_fsync =		p9fs_fsync,
	.vop_remove =		p9fs_remove,
	.vop_link =		p9fs_link,
	.vop_rename =		p9fs_rename,
	.vop_mkdir =		p9fs_mkdir,
	.vop_rmdir =		p9fs_rmdir,
	.vop_symlink =		p9fs_symlink,
	.vop_readdir =		p9fs_readdir,
	.vop_readlink =		p9fs_readlink,
	.vop_inactive =		p9fs_inactive,
	.vop_reclaim =		p9fs_reclaim,
	.vop_print =		p9fs_print,
	.vop_pathconf =		p9fs_pathconf,
	.vop_vptofh =		p9fs_vptofh,
#ifdef NOT_NEEDED
	.vop_bmap =		p9fs_bmap,
	.vop_bypass =		p9fs_bypass,
	.vop_islocked =		p9fs_islocked,
	.vop_whiteout =		p9fs_whiteout,
	.vop_accessx =		p9fs_accessx,
	.vop_markatime =	p9fs_markatime,
	.vop_poll =		p9fs_poll,
	.vop_kqfilter =		p9fs_kqfilter,
	.vop_revoke =		p9fs_revoke,
	.vop_lock1 =		p9fs_lock1,
	.vop_unlock =		p9fs_unlock,
	.vop_strategy =		p9fs_strategy,
	.vop_getwritemount =	p9fs_getwritemount,
	.vop_advlock =		p9fs_advlock,
	.vop_advlockasync =	p9fs_advlockasync,
	.vop_advlockpurge =	p9fs_advlockpurge,
	.vop_reallocblks =	p9fs_reallocblks,
	.vop_getpages =		p9fs_getpages,
	.vop_putpages =		p9fs_putpages,
	.vop_getacl =		p9fs_getacl,
	.vop_setacl =		p9fs_setacl,
	.vop_aclcheck =		p9fs_aclcheck,
	/*
	 * 9P2000.u specifically doesn't support extended attributes,
	 * although they could be as an extension.
	 */
	.vop_closeextattr =	p9fs_closeextattr,
	.vop_getextattr =	p9fs_getextattr,
	.vop_listextattr =	p9fs_listextattr,
	.vop_openextattr =	p9fs_openextattr,
	.vop_deleteextattr =	p9fs_deleteextattr,
	.vop_setextattr =	p9fs_setextattr,
	.vop_setlabel =		p9fs_setlabel,
	.vop_vptocnp =		p9fs_vptocnp,
	.vop_allocate =		p9fs_allocate,
	.vop_advise =		p9fs_advise,
	.vop_unp_bind =		p9fs_unp_bind,
	.vop_unp_connect =	p9fs_unp_connect,
	.vop_unp_detach =	p9fs_unp_detach,
	.vop_is_text =		p9fs_is_text,
	.vop_set_text =		p9fs_set_text,
	.vop_unset_text =	p9fs_unset_text,
	.vop_get_writecount =	p9fs_get_writecount,
	.vop_add_writecount =	p9fs_add_writecount,
#endif
};
