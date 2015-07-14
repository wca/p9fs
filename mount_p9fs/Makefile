# $FreeBSD$

# This should be removed if/when this is imported into FreeBSD.
BINDIR=	/sbin

PROG=	mount_p9fs
SRCS=	mount_p9fs.c getmntopts.c
MAN=	mount_p9fs.8

# For "mntopts.h"
MOUNT=	${.CURDIR}/../mount
.if !exists(${.CURDIR}/../mount) && defined(WORLDDIR)
MOUNT=	${WORLDDIR}/sbin/mount
.endif
.if !exists(${MOUNT}) && exists(/usr/src/Makefile.inc1)
MOUNT=	/usr/src/sbin/mount
.endif
.if !exists(${MOUNT})
.error "Unable to find sbin/mount source tree, specify WORLDDIR"
.endif
CFLAGS+=-I${MOUNT}

.PATH: ${MOUNT}

.include <bsd.prog.mk>
