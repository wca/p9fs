# $FreeBSD$

#.PATH=	${.CURDIR}/../../fs/p9fs

KMOD=	p9fs
SRCS=	vnode_if.h \
	p9fs_vfsops.c

.include <bsd.kmod.mk>
