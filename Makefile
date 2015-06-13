# $FreeBSD$

#.PATH=	${.CURDIR}/../../fs/p9fs

KMOD=	p9fs

SRCS+=	p9fs_client_proto.c
SRCS+=	p9fs_subr.c
SRCS+=	p9fs_vfsops.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
