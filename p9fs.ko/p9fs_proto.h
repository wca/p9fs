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
 * Plan9 filesystem (9P2000.u) protocol definitions.
 */

/**************************************************************************
 * Plan9 protocol documentation section
 **************************************************************************/

/*
 * 2 Introduction / 2.1 Messages
 *
 * - A client transmits requests (T-messages) to a server, which subsequently
 *   returns replies (R-messages) to the client.  The combined acts of
 *   transmitting (receiving) a request of a particular type, and receiving
 *   (transmitting) its reply is called a transaction of that type.
 *
 * - Each message consists of a sequence of bytes. Two-, four-, and
 *   eight-byte fields hold unsigned integers represented in little-endian
 *   order (least significant byte first).  Data items of larger or variable
 *   lengths are represented by a two-byte field specifying a count, n,
 *   followed by n bytes of data. Text strings are represented this way, with
 *   the text itself stored as a UTF-8 encoded sequence of Unicode characters.
 *   Text strings in 9P messages are not NUL-terminated: n counts the bytes
 *   of UTF-8 data, which include no final zero byte.  The NUL character is
 *   illegal in all text strings in 9P, and is therefore excluded from file
 *   names, user names, and so on.
 *
 * - Each 9P message begins with a four-byte size field specifying the length
 *   in bytes of the complete message including the four bytes of the size
 *   field itself.  The next byte is the message type, one of the constants
 *   in the enumeration in the include file fcall.h.  The next two bytes are
 *   an identifying tag, described below. The remaining bytes are parameters
 *   of different sizes. In the message descriptions, the number of bytes in a
 *   field is given in brackets after the field name.  The notation
 *   parameter[n] where n is not a constant represents a variable-length
 *   parameter: n[2] followed by n bytes of data forming the parameter.
 *   The notation string[s] (using a literal s character) is shorthand for
 *   s[2] followed by s bytes of UTF-8 text.  (Systems may choose to reduce
 *   the set of legal characters to reduce syntactic problems, for example
 *   to remove slashes from name components, but the protocol has no such
 *   restriction.  Plan 9 names may contain any printable character (that
 *   is, any character outside hexadecimal 00-1F and 80-9F) except slash.)
 *   Messages are transported in byte form to allow for machine independence;
 *   fcall(2) describes routines that convert to and from this form into a
 *   machine-dependent C structure.
 *
 * - Each T-message has a tag field, chosen and used by the client to
 *   identify the message.  The reply to the message will have the same tag.
 *   Clients must arrange that no two outstanding messages on the same
 *   connection have the same tag.  An exception is the tag NOTAG, defined
 *   as (ushort)~0 in fcall.h: the client can use it, when establishing a
 *   connection, to override tag matching in version messages.
 *
 * - The type of an R-message will either be one greater than the type of
 *   the corresponding T-message or Rerror, indicating that the request failed.
 *   In the latter case, the ename field contains a string describing the
 *   reason for failure.
 *
 * - The version message identifies the version of the protocol and indicates
 *   the maximum message size the system is prepared to handle.  It also
 *   initializes the connection and aborts all outstanding I/O on the
 *   connection.  The set of messages between version requests is called
 *   a session.
 *
 * - Most T-messages contain a fid, a 32-bit unsigned integer that the
 *   client uses to identify a ``current file'' on the server.  Fids are
 *   somewhat like file descriptors in a user process, but they are not
 *   restricted to files open for I/O: directories being examined, files
 *   being accessed by stat(2) calls, and so on -- all files being
 *   manipulated by the operating system -- are identified by fids.  Fids are
 *   chosen by the client.  All requests on a connection share the same fid
 *   space; when several clients share a connection, the agent managing the
 *   sharing must arrange that no two clients choose the same fid.
 *
 * - The fid supplied in an attach message will be taken by the server to
 *   refer to the root of the served file tree.  The attach identifies the
 *   user to the server and may specify a particular file tree served by the
 *   server (for those that supply more than one).
 *
 * - Permission to attach to the service is proven by providing a special
 *   fid, called afid, in the attach message.  This afid is established by
 *   exchanging auth messages and subsequently manipulated using read and
 *   write messages to exchange authentication information not defined
 *   explicitly by 9P [P9SEC].  Once the authentication protocol is
 *   complete, the afid is presented in the attach to permit the user to
 *   access the service.
 *
 * - A walk message causes the server to change the current file associated
 *   with a fid to be a file in the directory that is the old current file,
 *   or one of its subdirectories.  Walk returns a new fid that refers to
 *   the resulting file.  Usually, a client maintains a fid for the root,
 *   and navigates by walks from the root fid.
 *
 * - A client can send multiple T-messages without waiting for the
 *   corresponding R-messages, but all outstanding T-messages must specify
 *   different tags.  The server may delay the response to a request and
 *   respond to later ones; this is sometimes necessary, for example when
 *   the client reads from a file that the server synthesizes from external
 *   events such as keyboard characters.
 *
 * - Replies (R-messages) to auth, attach, walk, open, and create requests
 *   convey a qid field back to the client.  The qid represents the server's
 *   unique identification for the file being accessed: two files on the
 *   same server hierarchy are the same if and only if their qids are the
 *   same. (The client may have multiple fids pointing to a single file on
 *   a server and hence having a single qid.)  The thirteen-byte qid fields
 *   hold a one-byte type, specifying whether the file is a directory,
 *   append-only file, etc., and two unsigned integers: first the four-byte
 *   qid version, then the eight-byte qid path.  The path is an integer unique
 *   among all files in the hierarchy.  If a file is deleted and recreated
 *   with the same name in the same directory, the old and new path
 *   components of the qids should be different.  The version is a version
 *   number for a file; typically, it is incremented every time the file
 *   is modified.
 *
 * - An existing file can be opened, or a new file may be created in the
 *   current (directory) file.  I/O of a given number of bytes at a given
 *   offset on an open file is done by read and write.
 *
 * - A client should clunk any fid that is no longer needed. The remove
 *   transaction deletes files.
 *
 * - The stat transaction retrieves information about the file.  The stat
 *   field in the reply includes the file's name, access permissions (read,
 *   write and execute for owner, group and public), access and modification
 *   times, and owner and group identifications (see stat(2)).  The owner
 *   and group identifications are textual names.  The wstat transaction
 *   allows some of a file's properties to be changed.  A request can be
 *   aborted with a flush request.  When a server receives a Tflush, it
 *   should not reply to the message with tag oldtag (unless it has already
 *   replied), and it should immediately send an Rflush.  The client must
 *   wait until it gets the Rflush (even if the reply to the original message
 *   arrives in the interim), at which point oldtag may be reused.
 *
 * - Because the message size is negotiable and some elements of the
 *   protocol are variable length, it is possible (although unlikely) to
 *   have a situation where a valid message is too large to fit within the
 *   negotiated size.  For example, a very long file name may cause a Rstat
 *   of the file or Rread of its directory entry to be too large to send.
 *   In most such cases, the server should generate an error rather than
 *   modify the data to fit, such as by truncating the file name.  The
 *   exception is that a long error string in an Rerror message should be
 *   truncated if necessary, since the string is only advisory and in
 *   some sense arbitrary.
 *
 * - 
 */

/*
 * 2.2 Directories
 *
 * - Directories are created by create with DMDIR set in the permissions
 *   argument.  The members of a directory can be found with
 *   read(5).  All directories must support walks to the directory ..
 *   (dot-dot) meaning parent directory, although by convention directories
 *   contain no explicit entry for .. or . (dot).  The parent of the root
 *   directory of a server's tree is itself.
 *
 * - Each file server maintains a set of user and group names.  Each user
 *   can be a member of any number of groups.  Each group has a group leader
 *   who has special privileges.  Every file request has an implicit user id
 *   (copied from the original attach) and an implicit set of groups (every
 *   group of which the user is a member).
 */

/*
 * 2.3 Access Permissions
 *
 * - Each file has an associated owner and group id and three sets of
 *   permissions: those of the owner, those of the group, and those of
 *   ``other'' users.  When the owner attempts to do something to a file,
 *   the owner, group, and other permissions are consulted, and if any of
 *   them grant the requested permission, the operation is allowed.  For
 *   someone who is not the owner, but is a member of the file's group, the
 *   group and other permissions are consulted.  For everyone else, the
 *   other permissions are used.  Each set of permissions says whether
 *   reading is allowed, whether writing is allowed, and whether executing
 *   is allowed.  A walk in a directory is regarded as executing the
 *   directory, not reading it.  Permissions are kept in the low-order bits
 *   of the file mode: owner read/write/execute permission represented as
 *   1 in bits 8, 7, and 6 respectively (using 0 to number the low order).
 *   The group permissions are in bits 5, 4, and 3, and the other
 *   permissions are in bits 2, 1, and 0.
 *
 * - The file mode contains some additional attributes besides the
 *   permissions.  If bit 31 (DMDIR) is set, the file is a directory; if
 *   bit 30 (DMAPPEND) is set, the file is append-only (offset is ignored
 *   in writes); if bit 29 (DMEXCL) is set, the file is exclusive- use
 *   (only one client may have it open at a time); if bit 27 (DMAUTH) is set,
 *   the file is an authentication file established by auth messages; if
 *   bit 26 (DMTMP) is set, the contents of the file (or directory) are not
 *   included in nightly archives.  (Bit 28 is skipped for historical
 *   reasons.)  These bits are reproduced, from the top bit down, in the
 *   type byte of the Qid: QTDIR, QTAPPEND, QTEXCL, (skipping one bit)
 *   QTAUTH, and QTTMP.  The name QTFILE, defined to be zero, identifies
 *   the value of the type for a plain file.
 */

/**************************************************************************
 * Plan9 protocol definitions section
 **************************************************************************/

#ifndef	__P9FS_PROTO_H__
#define	__P9FS_PROTO_H__

/*
 * The message type used as the fifth byte for all 9P2000 messages.
 */
enum p9fs_msg_type {
	Tversion =	100,
	Rversion,
	Tauth,
	Rauth,
	Tattach,
	Rattach,
	/* Terror is illegal */
	Rerror =	107,
	Tflush,
	Rflush,
	Twalk,
	Rwalk,
	Topen,
	Ropen,
	Tcreate,
	Rcreate,
	Tread,
	Rread,
	Twrite,
	Rwrite,
	Tclunk,
	Rclunk,
	Tremove,
	Rremove,
	Tstat,
	Rstat,
	Twstat,
	Rwstat,
};

/* All 9P2000* messages are prefixed with: size[4] <Type> tag[2] */
struct p9fs_msg_hdr {
	uint32_t	hdr_size;
	uint8_t		hdr_type;
	uint16_t	hdr_tag;
} __attribute__((packed));

/*
 * Common structures for 9P2000 message payload items.
 */

/* QID: Unique identification for the file being accessed */
struct p9fs_qid {
	uint8_t qid_mode;
	uint32_t qid_version;
	uint64_t qid_path;
} __attribute__((packed));

/* Plan9-specific stat structure */
struct p9fs_stat {
	uint16_t stat_size;
	uint16_t stat_type;
	uint32_t stat_dev;
	struct p9fs_qid stat_qid;
	uint32_t stat_mode;
	uint32_t stat_atime;
	uint32_t stat_mtime;
	uint64_t stat_length;
	/* stat_name[s] */
	/* stat_uid[s] */
	/* stat_gid[s] */
	/* stat_muid[s] */
} __attribute__((packed));

/* This is the stat addendum for 9P2000.u vs 9P2000 */
struct p9fs_stat_u {
	struct p9fs_stat u_stat;
	/* extension[s] */
	/* p9fs_stat_u_footer */
} __attribute__((packed));

struct p9fs_stat_u_footer {
	uint32_t n_uid;
	uint32_t n_gid;
	uint32_t n_muid;
} __attribute__((packed));

/*
 * Basic structures for 9P2000 message types.
 *
 * Aside from Rerror and Tcreate, all variable length fields follow fixed
 * length fields.
 */

struct p9fs_msg_Tversion {
	struct p9fs_msg_hdr Tversion_hdr;
	uint32_t Tversion_max_size;
	/* Tversion_version[s] */
} __attribute__((packed));

struct p9fs_msg_Rversion {
	struct p9fs_msg_hdr Rversion_hdr;
	uint32_t Rversion_max_size;
	/* Rversion_version[s] */
} __attribute__((packed));

struct p9fs_msg_Tauth {
	struct p9fs_msg_hdr Tauth_hdr;
	uint32_t Tauth_afid;
	/* Tauth_uname[s] */
	/* Tauth_aname[s] */
} __attribute__((packed));

struct p9fs_msg_Rauth {
	struct p9fs_msg_hdr Rauth_hdr;
	struct p9fs_qid Rauth_aqid;
} __attribute__((packed));

struct p9fs_msg_Tattach {
	struct p9fs_msg_hdr Tattach_hdr;
	uint32_t Tattach_fid;
	uint32_t Tattach_afid;
	/* Tattach_uname[s] */
	/* Tattach_aname[s] */
} __attribute__((packed));

struct p9fs_msg_Rattach {
	struct p9fs_msg_hdr Rattach_hdr;
	struct p9fs_qid Rattach_qid;
} __attribute__((packed));

struct p9fs_msg_Rerror {
	struct p9fs_msg_hdr Rerror_hdr;
	/* Rerror_ename[s] */
	uint32_t Rerror_errno;
} __attribute__((packed));

struct p9fs_msg_Tflush {
	struct p9fs_msg_hdr Tflush_hdr;
	uint16_t Tflush_oldtag;
} __attribute__((packed));

struct p9fs_msg_Rflush {
	struct p9fs_msg_hdr Rflush_hdr;
} __attribute__((packed));

struct p9fs_msg_Twalk {
	struct p9fs_msg_hdr Twalk_hdr;
	uint32_t Twalk_fid;
	uint32_t Twalk_newfid;
	uint16_t Twalk_nwname;
	/* Twalk_wname[s][] */
} __attribute__((packed));

struct p9fs_msg_Rwalk {
	struct p9fs_msg_hdr Rwalk_hdr;
	uint16_t Rwalk_nwqid;
	/* struct p9fs_qid Rwalk_nwqid[] */
} __attribute__((packed));

struct p9fs_msg_Topen {
	struct p9fs_msg_hdr Topen_hdr;
	uint32_t Topen_fid;
	uint8_t Topen_mode;
} __attribute__((packed));

struct p9fs_msg_Ropen {
	struct p9fs_msg_hdr Ropen_hdr;
	struct p9fs_qid Ropen_qid;
	uint32_t Ropen_iounit;
} __attribute__((packed));

struct p9fs_msg_Tcreate {
	struct p9fs_msg_hdr Tcreate_hdr;
	uint32_t Tcreate_fid;
	/* Tcreate_name[s] */
	uint32_t Tcreate_perm;
	uint8_t Tcreate_mode;
} __attribute__((packed));

struct p9fs_msg_Rcreate {
	struct p9fs_msg_hdr Rcreate_hdr;
	struct p9fs_qid Rcreate_qid;
	uint32_t Rcreate_iounit;
} __attribute__((packed));

struct p9fs_msg_Tread {
	struct p9fs_msg_hdr Tread_hdr;
	uint32_t Tread_fid;
	uint64_t Tread_offset;
	uint32_t Tread_count;
} __attribute__((packed));

struct p9fs_msg_Rread {
	struct p9fs_msg_hdr Rread_hdr;
	uint32_t Rread_count;
	/* uint8_t data[count] */
} __attribute__((packed));

struct p9fs_msg_Twrite {
	struct p9fs_msg_hdr Twrite_hdr;
	uint32_t Twrite_fid;
	uint64_t Twrite_offset;
	uint32_t Twrite_count;
	/* uint8_t data[count] */
} __attribute__((packed));

struct p9fs_msg_Rwrite {
	struct p9fs_msg_hdr Rwrite_hdr;
	uint32_t Rwrite_count;
} __attribute__((packed));

struct p9fs_msg_Tclunk {
	struct p9fs_msg_hdr Tclunk_hdr;
	uint32_t Tclunk_fid;
} __attribute__((packed));

struct p9fs_msg_Rclunk {
	struct p9fs_msg_hdr Rclunk_hdr;
} __attribute__((packed));

struct p9fs_msg_Tremove {
	struct p9fs_msg_hdr Tremove_hdr;
	uint32_t Tremove_fid;
} __attribute__((packed));

struct p9fs_msg_Rremove {
	struct p9fs_msg_hdr Rremove_hdr;
} __attribute__((packed));

struct p9fs_msg_Tstat {
	struct p9fs_msg_hdr Tstat_hdr;
	uint32_t Tstat_fid;
} __attribute__((packed));

struct p9fs_msg_Rstat {
	struct p9fs_msg_hdr Rstat_hdr;
	struct p9fs_stat Rstat_stat;
} __attribute__((packed));

struct p9fs_msg_Twstat {
	struct p9fs_msg_hdr Twstat_hdr;
	uint32_t Twstat_fid;
	struct p9fs_stat Twstat_stat;
} __attribute__((packed));

struct p9fs_msg_Rwstat {
	struct p9fs_msg_hdr Rwstat_hdr;
} __attribute__((packed));

/*
 * The main 9P message management structure.
 */
union p9fs_msg {
	struct p9fs_msg_hdr p9msg_hdr;
	struct p9fs_msg_Tversion p9msg_Tversion;
	struct p9fs_msg_Rversion p9msg_Rversion;
	struct p9fs_msg_Tauth p9msg_Tauth;
	struct p9fs_msg_Rauth p9msg_Rauth;
	struct p9fs_msg_Tattach p9msg_Tattach;
	struct p9fs_msg_Rattach p9msg_Rattach;
	struct p9fs_msg_Rerror p9msg_Rerror;
	struct p9fs_msg_Tflush p9msg_Tflush;
	struct p9fs_msg_Rflush p9msg_Rflush;
	struct p9fs_msg_Twalk p9msg_Twalk;
	struct p9fs_msg_Rwalk p9msg_Rwalk;
	struct p9fs_msg_Topen p9msg_Topen;
	struct p9fs_msg_Ropen p9msg_Ropen;
	struct p9fs_msg_Tcreate p9msg_Tcreate;
	struct p9fs_msg_Rcreate p9msg_Rcreate;
	struct p9fs_msg_Tread p9msg_Tread;
	struct p9fs_msg_Rread p9msg_Rread;
	struct p9fs_msg_Twrite p9msg_Twrite;
	struct p9fs_msg_Rwrite p9msg_Rwrite;
	struct p9fs_msg_Tclunk p9msg_Tclunk;
	struct p9fs_msg_Rclunk p9msg_Rclunk;
	struct p9fs_msg_Tremove p9msg_Tremove;
	struct p9fs_msg_Rremove p9msg_Rremove;
	struct p9fs_msg_Tstat p9msg_Tstat;
	struct p9fs_msg_Rstat p9msg_Rstat;
	struct p9fs_msg_Twstat p9msg_Twstat;
	struct p9fs_msg_Rwstat p9msg_Rwstat;
} __attribute__((packed));

#define	NOTAG		(unsigned short)~0
#define	NOFID		(uint32_t)~0

#define	P9_VERS		"9P2000"
#define	UN_VERS		P9_VERS ".u"
#define	P9_MSG_MAX	MAXPHYS + sizeof (struct p9fs_msg_hdr)

/**************************************************************************
 * Plan9 session details section
 **************************************************************************/

struct p9fs_req {
	TAILQ_ENTRY(p9fs_req) req_link;
	uint16_t req_tag;
	struct mbuf *req_msg;
	int req_error;
};
TAILQ_HEAD(p9fs_req_list, p9fs_req);

/* NB: This is used for in-core, not wire format. */
struct p9fs_str {
	uint16_t p9str_size;
	char *p9str_str;
};

/* Payload structures passed to requesters via callback.  In-core only. */
struct p9fs_stat_payload {
	struct p9fs_stat *pay_stat;
	struct p9fs_str pay_name;
	struct p9fs_str pay_uid;
	struct p9fs_str pay_gid;
	struct p9fs_str pay_muid;
};
struct p9fs_stat_u_payload {
	struct p9fs_stat_payload upay_std;
	struct p9fs_str upay_extension;
	struct p9fs_stat_u_footer *upay_footer;
};

struct p9fs_recv {
	uint32_t p9r_resid;
	uint32_t p9r_size;
	int p9r_error;
	int p9r_soupcalls;
	struct mbuf *p9r_msg;
	struct p9fs_req_list p9r_reqs;
};

enum p9s_state {
	P9S_INIT,
	P9S_RUNNING,
	P9S_CLOSING,
	P9S_CLOSED,
};

#define	MAXUNAMELEN	32
struct p9fs_session {
	enum p9s_state p9s_state;
	struct sockaddr p9s_sockaddr;
	struct mtx p9s_lock;
	struct p9fs_recv p9s_recv;
	int p9s_sockaddr_len;
	struct socket *p9s_sock;
	int p9s_socktype;
	int p9s_proto;
	int p9s_threads;
	char p9s_uname[MAXUNAMELEN];
	char p9s_path[MAXPATHLEN];
	struct p9fs_qid p9s_qid;
	uint32_t p9s_fid;
	uint32_t p9s_afid;
	uint32_t p9s_uid;
};

typedef int (*Rstat_callback)(struct p9fs_stat_u_payload *, void *);

int p9fs_client_version(struct p9fs_session *);
int p9fs_client_auth(struct p9fs_session *);
int p9fs_client_attach(struct p9fs_session *);
int p9fs_client_clunk(void);
int p9fs_client_error(void **, enum p9fs_msg_type);
int p9fs_client_flush(void);
int p9fs_client_open(void);
int p9fs_client_create(void);
int p9fs_client_read(void);
int p9fs_client_write(void);
int p9fs_client_remove(void);
int p9fs_client_stat(struct p9fs_session *, uint32_t, Rstat_callback, void *);
int p9fs_client_wstat(void);
int p9fs_client_walk(struct p9fs_session *, uint32_t, uint32_t *, size_t,
    const char *);

#endif /* __P9FS_PROTO_H__ */
