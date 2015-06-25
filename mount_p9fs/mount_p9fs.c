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
 * Plan9 filesystem mount helper.
 *
 * This is used primarily to perform DNS lookups for the kernel.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/errno.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdarg.h>

#include "mntopts.h"

static void
usage(int exitcode, const char *errfmt, ...)
{
	if (exitcode > 1)
		fprintf(stderr, "%s\n", strerror(exitcode));

	if (errfmt != NULL) {
		va_list ap;

		va_start(ap, errfmt);
		vfprintf(stderr, errfmt, ap);
		va_end(ap);
		fprintf(stderr, "\n");
	}

	fprintf(stderr, "Usage: %s [-o option=value] pathspec mntpt\n",
	    getprogname());
	exit(exitcode);
}

struct mnt_context {
	struct iovec *iov;
	int iovlen;
	int socktype;
	int found_addr;
};

static void
parse_opt_o(struct mnt_context *ctx)
{
	char *opt, *val;

	opt = strchr(optarg, '=');
	if (opt == NULL)
		usage(1, "Invalid -o");
	*opt = '\0';
	val = opt + 1;
	opt = optarg;
	build_iovec(&ctx->iov, &ctx->iovlen, opt,
	    __DECONST(void *, val), strlen(val) + 1);

	if (strcmp(opt, "proto") == 0) {
		if (strcasecmp(val, "tcp") == 0)
			ctx->socktype = SOCK_STREAM;
		else
			ctx->socktype = SOCK_DGRAM;
	}
}

/*
 * Try the given addrinfo.  Returns:
 *   -1: if the attempt failed for a reason not caused by local machine issues.
 *   0:  on success.
 *   errno: any local machine failure.
 */
static int
try_addrinfo(struct mnt_context *ctx, struct addrinfo *ai)
{
	int error, s;

	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s == -1) {
		/* XXX: Not sure if EACCES should be considered non-fatal. */
		switch (errno) {
		case EACCES:
		case EAFNOSUPPORT:
			return (-1);
		default:
			return (errno);
		}
	}
	error = connect(s, ai->ai_addr, ai->ai_addrlen);
	close(s);
	if (error == -1) {
		switch (errno) {
		case ECONNREFUSED:
		case ECONNRESET:
		case ENETUNREACH:
		case EHOSTUNREACH:
		case ETIMEDOUT:
			return (-1);
		default:
			return (errno);
		}
	}

	build_iovec(&ctx->iov, &ctx->iovlen, "addr",
	    &ai->ai_addr, ai->ai_addrlen);
	ctx->found_addr = 1;
	return (0);
}

static void
parse_required_args(struct mnt_context *ctx, char **argv)
{
	char *path;
	int addrlen, error;
	struct addrinfo *ai, *res;
	struct addrinfo hints;
	struct sockaddr *addr;

	/* Parse pathspec */
	path = strchr(argv[0], ':');
	if (path == NULL)
		usage(1, "Pathspec does not follow host:path format");
	*path++ = '\0';

	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = ctx->socktype == 0 ? SOCK_STREAM : ctx->socktype;
	error = getaddrinfo(argv[0], NULL, &hints, &res);
	if (error != 0) {
		/* Try again, with name lookups. */
		hints.ai_flags = AI_CANONNAME;
		error = getaddrinfo(argv[0], NULL, &hints, &res);
	}
	if (error != 0)
		errx(error, "Unable to lookup %s: %s", argv[0],
		    gai_strerror(error));

	/* Try each addrinfo returned to see if one connects OK. */
	error = -1;
	for (ai = res; error == -1 && ai != NULL; ai = ai->ai_next)
		error = try_addrinfo(ctx, ai);
	freeaddrinfo(res);
	if (error != 0)
		errx(error, "Unable to connect to %s", argv[0]);
	if (ctx->found_addr == 0)
		errx(1, "No valid address found for %s", argv[0]);

	build_iovec(&ctx->iov, &ctx->iovlen, "hostname", argv[0], (size_t)-1);
	build_iovec(&ctx->iov, &ctx->iovlen, "path", path, (size_t)-1);
}

int
main(int argc, char **argv)
{
	int ch, error;
	struct mnt_context ctx = { 0 };
	const char *optstr = "o:";

	while ((ch = getopt(argc, argv, optstr)) != -1) {
		switch (ch) {
		case 'o':
			parse_opt_o(&ctx);
			break;
		default:
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage(1, "Must specify required arguments");

	parse_required_args(&ctx, argv);
	if (modfind("p9fs") < 0) {
		if (kldload("p9fs") < 0)
			errx(1, "p9fs could not be loaded in the kernel");
		if (modfind("p9fs") < 0)
			errx(1, "p9fs is not in the kernel");
	}

	error = nmount(ctx.iov, ctx.iovlen, 0);

	return (error);
}
