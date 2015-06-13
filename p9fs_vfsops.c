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

#include <sys/mount.h>
#include <sys/kernel.h>

static int
p9fs_mount(struct mount *mp)
{
	return (EINVAL);
}

static int
p9fs_unmount(struct mount *mp, int mntflags)
{
	return (EINVAL);
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
