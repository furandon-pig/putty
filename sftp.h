/*
 * sftp.h: definitions for SFTP and the sftp.c routines.
 */

#include "defs.h"
#include "int64.h"

#define SSH_FXP_INIT                              1	/* 0x1 */
#define SSH_FXP_VERSION                           2	/* 0x2 */
#define SSH_FXP_OPEN                              3	/* 0x3 */
#define SSH_FXP_CLOSE                             4	/* 0x4 */
#define SSH_FXP_READ                              5	/* 0x5 */
#define SSH_FXP_WRITE                             6	/* 0x6 */
#define SSH_FXP_LSTAT                             7	/* 0x7 */
#define SSH_FXP_FSTAT                             8	/* 0x8 */
#define SSH_FXP_SETSTAT                           9	/* 0x9 */
#define SSH_FXP_FSETSTAT                          10	/* 0xa */
#define SSH_FXP_OPENDIR                           11	/* 0xb */
#define SSH_FXP_READDIR                           12	/* 0xc */
#define SSH_FXP_REMOVE                            13	/* 0xd */
#define SSH_FXP_MKDIR                             14	/* 0xe */
#define SSH_FXP_RMDIR                             15	/* 0xf */
#define SSH_FXP_REALPATH                          16	/* 0x10 */
#define SSH_FXP_STAT                              17	/* 0x11 */
#define SSH_FXP_RENAME                            18	/* 0x12 */
#define SSH_FXP_STATUS                            101	/* 0x65 */
#define SSH_FXP_HANDLE                            102	/* 0x66 */
#define SSH_FXP_DATA                              103	/* 0x67 */
#define SSH_FXP_NAME                              104	/* 0x68 */
#define SSH_FXP_ATTRS                             105	/* 0x69 */
#define SSH_FXP_EXTENDED                          200	/* 0xc8 */
#define SSH_FXP_EXTENDED_REPLY                    201	/* 0xc9 */

#define SSH_FX_OK                                 0
#define SSH_FX_EOF                                1
#define SSH_FX_NO_SUCH_FILE                       2
#define SSH_FX_PERMISSION_DENIED                  3
#define SSH_FX_FAILURE                            4
#define SSH_FX_BAD_MESSAGE                        5
#define SSH_FX_NO_CONNECTION                      6
#define SSH_FX_CONNECTION_LOST                    7
#define SSH_FX_OP_UNSUPPORTED                     8

#define SSH_FILEXFER_ATTR_SIZE                    0x00000001
#define SSH_FILEXFER_ATTR_UIDGID                  0x00000002
#define SSH_FILEXFER_ATTR_PERMISSIONS             0x00000004
#define SSH_FILEXFER_ATTR_ACMODTIME               0x00000008
#define SSH_FILEXFER_ATTR_EXTENDED                0x80000000

#define SSH_FXF_READ                              0x00000001
#define SSH_FXF_WRITE                             0x00000002
#define SSH_FXF_APPEND                            0x00000004
#define SSH_FXF_CREAT                             0x00000008
#define SSH_FXF_TRUNC                             0x00000010
#define SSH_FXF_EXCL                              0x00000020

#define SFTP_PROTO_VERSION 3

/*
 * External references. The sftp client module sftp.c expects to be
 * able to get at these functions.
 * 
 * sftp_recvdata must never return less than len. It either blocks
 * until len is available, or it returns failure.
 * 
 * Both functions return 1 on success, 0 on failure.
 *
 * sftp_sendbuffer returns the size of the backlog of data in the
 * transmit queue.
 */
int sftp_senddata(char *data, int len);
int sftp_sendbuffer(void);
int sftp_recvdata(char *data, int len);

/*
 * Free sftp_requests
 */
void sftp_cleanup_request(void);

struct fxp_attrs {
    unsigned long flags;
    uint64 size;
    unsigned long uid;
    unsigned long gid;
    unsigned long permissions;
    unsigned long atime;
    unsigned long mtime;
};
extern const struct fxp_attrs no_attrs;

/*
 * Copy between the possibly-unused permissions field in an fxp_attrs
 * and a possibly-negative integer containing the same permissions.
 */
#define PUT_PERMISSIONS(attrs, perms)                   \
    ((perms) >= 0 ?                                     \
     ((attrs).flags |= SSH_FILEXFER_ATTR_PERMISSIONS,   \
      (attrs).permissions = (perms)) :                  \
     ((attrs).flags &= ~SSH_FILEXFER_ATTR_PERMISSIONS))
#define GET_PERMISSIONS(attrs, defaultperms)            \
    ((attrs).flags & SSH_FILEXFER_ATTR_PERMISSIONS ?    \
     (attrs).permissions : defaultperms)

struct fxp_handle {
    char *hstring;
    int hlen;
};

struct fxp_name {
    char *filename, *longname;
    struct fxp_attrs attrs;
};

struct fxp_names {
    int nnames;
    struct fxp_name *names;
};

struct sftp_request;

/*
 * Packet-manipulation functions.
 */

struct sftp_packet {
    char *data;
    unsigned length, maxlen;
    unsigned savedpos;
    int type;
    BinarySink_IMPLEMENTATION;
    BinarySource_IMPLEMENTATION;
};

/* When sending a packet, create it with sftp_pkt_init, then add
 * things to it by treating it as a BinarySink. When it's done, call
 * sftp_send_prepare, and then pkt->data and pkt->length describe its
 * wire format. */
struct sftp_packet *sftp_pkt_init(int pkt_type);
void sftp_send_prepare(struct sftp_packet *pkt);

/* When receiving a packet, create it with sftp_recv_prepare once you
 * decode its length from the first 4 bytes of wire data. Then write
 * that many bytes into pkt->data, and call sftp_recv_finish to set up
 * the type code and BinarySource. */
struct sftp_packet *sftp_recv_prepare(unsigned length);
int sftp_recv_finish(struct sftp_packet *pkt);

/* Either kind of packet can be freed afterwards with sftp_pkt_free. */
void sftp_pkt_free(struct sftp_packet *pkt);

void BinarySink_put_fxp_attrs(BinarySink *bs, struct fxp_attrs attrs);
int BinarySource_get_fxp_attrs(BinarySource *src, struct fxp_attrs *attrs);
#define put_fxp_attrs(bs, attrs) \
    BinarySink_put_fxp_attrs(BinarySink_UPCAST(bs), attrs)
#define get_fxp_attrs(bs, attrs) \
    BinarySource_get_fxp_attrs(BinarySource_UPCAST(bs), attrs)

/*
 * Error handling.
 */

const char *fxp_error(void);
int fxp_error_type(void);

/*
 * Perform exchange of init/version packets. Return 0 on failure.
 */
int fxp_init(void);

/*
 * Canonify a pathname. Concatenate the two given path elements
 * with a separating slash, unless the second is NULL.
 */
struct sftp_request *fxp_realpath_send(const char *path);
char *fxp_realpath_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Open a file. 'attrs' contains attributes to be applied to the file
 * if it's being created.
 */
struct sftp_request *fxp_open_send(const char *path, int type,
                                   const struct fxp_attrs *attrs);
struct fxp_handle *fxp_open_recv(struct sftp_packet *pktin,
				 struct sftp_request *req);

/*
 * Open a directory.
 */
struct sftp_request *fxp_opendir_send(const char *path);
struct fxp_handle *fxp_opendir_recv(struct sftp_packet *pktin,
				    struct sftp_request *req);

/*
 * Close a file/dir. Returns 1 on success, 0 on error.
 */
struct sftp_request *fxp_close_send(struct fxp_handle *handle);
int fxp_close_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Make a directory.
 */
struct sftp_request *fxp_mkdir_send(const char *path,
                                    const struct fxp_attrs *attrs);
int fxp_mkdir_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Remove a directory.
 */
struct sftp_request *fxp_rmdir_send(const char *path);
int fxp_rmdir_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Remove a file.
 */
struct sftp_request *fxp_remove_send(const char *fname);
int fxp_remove_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Rename a file.
 */
struct sftp_request *fxp_rename_send(const char *srcfname,
                                     const char *dstfname);
int fxp_rename_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Return file attributes.
 */
struct sftp_request *fxp_stat_send(const char *fname);
int fxp_stat_recv(struct sftp_packet *pktin, struct sftp_request *req,
		  struct fxp_attrs *attrs);
struct sftp_request *fxp_fstat_send(struct fxp_handle *handle);
int fxp_fstat_recv(struct sftp_packet *pktin, struct sftp_request *req,
		   struct fxp_attrs *attrs);

/*
 * Set file attributes.
 */
struct sftp_request *fxp_setstat_send(const char *fname,
                                      struct fxp_attrs attrs);
int fxp_setstat_recv(struct sftp_packet *pktin, struct sftp_request *req);
struct sftp_request *fxp_fsetstat_send(struct fxp_handle *handle,
				       struct fxp_attrs attrs);
int fxp_fsetstat_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Read from a file.
 */
struct sftp_request *fxp_read_send(struct fxp_handle *handle,
				   uint64 offset, int len);
int fxp_read_recv(struct sftp_packet *pktin, struct sftp_request *req,
		  char *buffer, int len);

/*
 * Write to a file. Returns 0 on error, 1 on OK.
 */
struct sftp_request *fxp_write_send(struct fxp_handle *handle,
				    void *buffer, uint64 offset, int len);
int fxp_write_recv(struct sftp_packet *pktin, struct sftp_request *req);

/*
 * Read from a directory.
 */
struct sftp_request *fxp_readdir_send(struct fxp_handle *handle);
struct fxp_names *fxp_readdir_recv(struct sftp_packet *pktin,
				   struct sftp_request *req);

/*
 * Free up an fxp_names structure.
 */
void fxp_free_names(struct fxp_names *names);

/*
 * Duplicate and free fxp_name structures.
 */
struct fxp_name *fxp_dup_name(struct fxp_name *name);
void fxp_free_name(struct fxp_name *name);

/*
 * Store user data in an sftp_request structure.
 */
void *fxp_get_userdata(struct sftp_request *req);
void fxp_set_userdata(struct sftp_request *req, void *data);

/*
 * These functions might well be temporary placeholders to be
 * replaced with more useful similar functions later. They form the
 * main dispatch loop for processing incoming SFTP responses.
 */
void sftp_register(struct sftp_request *req);
struct sftp_request *sftp_find_request(struct sftp_packet *pktin);
struct sftp_packet *sftp_recv(void);

/*
 * A wrapper to go round fxp_read_* and fxp_write_*, which manages
 * the queueing of multiple read/write requests.
 */

struct fxp_xfer;

struct fxp_xfer *xfer_download_init(struct fxp_handle *fh, uint64 offset);
void xfer_download_queue(struct fxp_xfer *xfer);
int xfer_download_gotpkt(struct fxp_xfer *xfer, struct sftp_packet *pktin);
int xfer_download_data(struct fxp_xfer *xfer, void **buf, int *len);

struct fxp_xfer *xfer_upload_init(struct fxp_handle *fh, uint64 offset);
int xfer_upload_ready(struct fxp_xfer *xfer);
void xfer_upload_data(struct fxp_xfer *xfer, char *buffer, int len);
int xfer_upload_gotpkt(struct fxp_xfer *xfer, struct sftp_packet *pktin);

int xfer_done(struct fxp_xfer *xfer);
void xfer_set_error(struct fxp_xfer *xfer);
void xfer_cleanup(struct fxp_xfer *xfer);

/*
 * Vtable for the platform-specific filesystem implementation that
 * answers requests in an SFTP server.
 */
typedef struct SftpReplyBuilder SftpReplyBuilder;
struct SftpServer {
    const SftpServerVtable *vt;
};
struct SftpServerVtable {
    SftpServer *(*new)(const SftpServerVtable *vt);
    void (*free)(SftpServer *srv);

    /*
     * Handle actual filesystem requests.
     *
     * Each of these functions replies by calling an appropiate
     * sftp_reply_foo() function on the given reply packet.
     */

    /* Should call fxp_reply_error or fxp_reply_simple_name */
    void (*realpath)(SftpServer *srv, SftpReplyBuilder *reply,
                     ptrlen path);

    /* Should call fxp_reply_error or fxp_reply_handle */
    void (*open)(SftpServer *srv, SftpReplyBuilder *reply,
                 ptrlen path, unsigned flags, struct fxp_attrs attrs);

    /* Should call fxp_reply_error or fxp_reply_handle */
    void (*opendir)(SftpServer *srv, SftpReplyBuilder *reply,
                    ptrlen path);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*close)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen handle);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*mkdir)(SftpServer *srv, SftpReplyBuilder *reply,
                  ptrlen path, struct fxp_attrs attrs);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*rmdir)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen path);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*remove)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen path);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*rename)(SftpServer *srv, SftpReplyBuilder *reply,
                   ptrlen srcpath, ptrlen dstpath);

    /* Should call fxp_reply_error or fxp_reply_attrs */
    void (*stat)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen path,
                 int follow_symlinks);

    /* Should call fxp_reply_error or fxp_reply_attrs */
    void (*fstat)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen handle);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*setstat)(SftpServer *srv, SftpReplyBuilder *reply,
                    ptrlen path, struct fxp_attrs attrs);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*fsetstat)(SftpServer *srv, SftpReplyBuilder *reply,
                     ptrlen handle, struct fxp_attrs attrs);

    /* Should call fxp_reply_error or fxp_reply_data */
    void (*read)(SftpServer *srv, SftpReplyBuilder *reply,
                 ptrlen handle, uint64 offset, unsigned length);

    /* Should call fxp_reply_error or fxp_reply_ok */
    void (*write)(SftpServer *srv, SftpReplyBuilder *reply,
                  ptrlen handle, uint64 offset, ptrlen data);

    /* Should call fxp_reply_error, or fxp_reply_name_count once and
     * then fxp_reply_full_name that many times */
    void (*readdir)(SftpServer *srv, SftpReplyBuilder *reply, ptrlen handle,
                    int max_entries, int omit_longname);
};

#define sftpsrv_new(vt) \
    ((vt)->new(vt))
#define sftpsrv_free(srv) \
    ((srv)->vt->free(srv))
#define sftpsrv_realpath(srv, reply, path) \
    ((srv)->vt->realpath(srv, reply, path))
#define sftpsrv_open(srv, reply, path, flags, attrs) \
    ((srv)->vt->open(srv, reply, path, flags, attrs))
#define sftpsrv_opendir(srv, reply, path) \
    ((srv)->vt->opendir(srv, reply, path))
#define sftpsrv_close(srv, reply, handle) \
    ((srv)->vt->close(srv, reply, handle))
#define sftpsrv_mkdir(srv, reply, path, attrs) \
    ((srv)->vt->mkdir(srv, reply, path, attrs))
#define sftpsrv_rmdir(srv, reply, path) \
    ((srv)->vt->rmdir(srv, reply, path))
#define sftpsrv_remove(srv, reply, path) \
    ((srv)->vt->remove(srv, reply, path))
#define sftpsrv_rename(srv, reply, srcpath, dstpath) \
    ((srv)->vt->rename(srv, reply, srcpath, dstpath))
#define sftpsrv_stat(srv, reply, path, follow) \
    ((srv)->vt->stat(srv, reply, path, follow))
#define sftpsrv_fstat(srv, reply, handle) \
    ((srv)->vt->fstat(srv, reply, handle))
#define sftpsrv_setstat(srv, reply, path, attrs) \
    ((srv)->vt->setstat(srv, reply, path, attrs))
#define sftpsrv_fsetstat(srv, reply, handle, attrs) \
    ((srv)->vt->fsetstat(srv, reply, handle, attrs))
#define sftpsrv_read(srv, reply, handle, offset, length) \
    ((srv)->vt->read(srv, reply, handle, offset, length))
#define sftpsrv_write(srv, reply, handle, offset, data) \
    ((srv)->vt->write(srv, reply, handle, offset, data))
#define sftpsrv_readdir(srv, reply, handle, max, nolongname) \
    ((srv)->vt->readdir(srv, reply, handle, max, nolongname))

typedef struct SftpReplyBuilderVtable SftpReplyBuilderVtable;
struct SftpReplyBuilder {
    const SftpReplyBuilderVtable *vt;
};
struct SftpReplyBuilderVtable {
    void (*reply_ok)(SftpReplyBuilder *reply);
    void (*reply_error)(SftpReplyBuilder *reply, unsigned code,
                        const char *msg);
    void (*reply_simple_name)(SftpReplyBuilder *reply, ptrlen name);
    void (*reply_name_count)(SftpReplyBuilder *reply, unsigned count);
    void (*reply_full_name)(SftpReplyBuilder *reply, ptrlen name,
                            ptrlen longname, struct fxp_attrs attrs);
    void (*reply_handle)(SftpReplyBuilder *reply, ptrlen handle);
    void (*reply_data)(SftpReplyBuilder *reply, ptrlen data);
    void (*reply_attrs)(SftpReplyBuilder *reply, struct fxp_attrs attrs);
};

#define fxp_reply_ok(reply) \
    ((reply)->vt->reply_ok(reply))
#define fxp_reply_error(reply, code, msg) \
    ((reply)->vt->reply_error(reply, code, msg))
#define fxp_reply_simple_name(reply, name) \
    ((reply)->vt->reply_simple_name(reply, name))
#define fxp_reply_name_count(reply, count) \
    ((reply)->vt->reply_name_count(reply, count))
#define fxp_reply_full_name(reply, name, longname, attrs) \
    ((reply)->vt->reply_full_name(reply, name, longname, attrs))
#define fxp_reply_handle(reply, handle) \
    ((reply)->vt->reply_handle(reply, handle))
#define fxp_reply_data(reply, data) \
    ((reply)->vt->reply_data(reply, data))
#define fxp_reply_attrs(reply, attrs) \
    ((reply)->vt->reply_attrs(reply, attrs))

/*
 * The usual implementation of an SftpReplyBuilder, containing a
 * 'struct sftp_packet' which is assumed to be already initialised
 * before one of the above request methods is called.
 */
extern const struct SftpReplyBuilderVtable DefaultSftpReplyBuilder_vt;
typedef struct DefaultSftpReplyBuilder DefaultSftpReplyBuilder;
struct DefaultSftpReplyBuilder {
    SftpReplyBuilder rb;
    struct sftp_packet *pkt;
};

/*
 * The top-level function that handles an SFTP request, given an
 * implementation of the above SftpServer abstraction to do the actual
 * filesystem work. It handles all the marshalling and unmarshalling
 * of packets, and the copying of request ids into the responses.
 */
struct sftp_packet *sftp_handle_request(
    SftpServer *srv, struct sftp_packet *request);
