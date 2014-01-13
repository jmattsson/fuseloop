/* fuseloop.c - loopback via fuse
 *
 * See the LICENSE.txt file for license details.
 *
 * Mainly intended to allow creating of virtual disk images by giving
 * specific offset/size access so that mke2fs can be run on less than the
 * entire file.
 *
 * Once a virtual disk image has been partitioned and had the filesystems
 * created, "mountlo" can be used to mount them.
 */
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse_common.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define VERSION "1.0"

#define DBG(fmt, ...) do { \
    fprintf(stderr, "debug: ");\
    fprintf(stderr, fmt, __VA_ARGS__);\
    fprintf(stderr, "\n"); }\
while (0)

/**
 * Configuration options & state
 */
struct cfg{
    char *src_file;
    char *mount_file;
    off_t offset_in_bytes;
    off_t size_in_bytes;
    int open_as_readonly;
    int src_fd;
    int debug;
};

#define LOCAL_OPT(opt, var, val) { opt, offsetof(struct cfg, var), val }

enum {
    KEY_RO,
    KEY_RW,
    KEY_HELP,
    KEY_VERSION,
};


static struct fuse_opt all_opts[] = {
    LOCAL_OPT("-d",             debug,           1),
    LOCAL_OPT("-O %lu",         offset_in_bytes, 0),
    LOCAL_OPT("-S %lu",         size_in_bytes,   0),
    FUSE_OPT_KEY("-r",          KEY_RO),
    FUSE_OPT_KEY("-w",          KEY_RW),
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};

static void syntax (const char *argv0)
{
    fprintf (stderr,
            "syntax: %s [options] <src_file> <mount_file>\n"
            "options:\n"
            "    -h  --help      print help\n"
            "    -V  --version   print version\n"
            "    -O <bytes>      src_file offset in bytes\n"
            "    -S <bytes>      size in bytes available from offset\n"
            "    -r              read only\n"
            "    -w              read write (default)\n"
            "    -o <option(s)>  mount options\n"
            "    -d              enable debug output\n"
            "\n",
            argv0);
    exit (1);
}

static int process_opts (void *data, const char *arg, int key, struct fuse_args *outargs)
{
    struct cfg *conf = data;
    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            if (!conf->src_file)
            {
                conf->src_file = strdup (arg);
                return 0;
            }
            else if (!conf->mount_file)
            {
                conf->mount_file = strdup (arg);
                return 0;
            }
            break;
        case FUSE_OPT_KEY_OPT:
            // Keep -o options
            return 1;
        case KEY_RO:
            conf->open_as_readonly = 1;
            return fuse_opt_add_arg (outargs, "-oro");
        case KEY_RW:
            return fuse_opt_add_arg (outargs, "-orw");
        case KEY_HELP:
            syntax (outargs->argv[0]);
            // does not return
        case KEY_VERSION:
            fprintf (stderr, "fuseloop %s\n", VERSION);
            exit (0);
        default:
            break;
    }

    fprintf (stderr, "unknown option: '%s'\n", arg);
    return -1;
}

#define ISROOT(path) (path[0] == '/' && path[1] == '\0')

static int fuseloop_open (const char *path, struct fuse_file_info *fi)
{
    if (!ISROOT(path))
        return -ENOENT;

    return 0;
}

static int fuseloop_read (const char *path, char *buf, size_t size, off_t offs, struct fuse_file_info *fi)
{
    int res = 0;
    struct fuse_context *ctx = fuse_get_context ();
    struct cfg *conf = ctx->private_data;

    if (!ISROOT(path))
        return -ENOENT;

    // Truncate read request if outside of file bounds
    if (conf->size_in_bytes >= 0 && (offs + size) > conf->size_in_bytes)
    {
        // size_t is unsigned, so make sure we don't by accident wrap it
        if (offs > conf->size_in_bytes)
            return -EINVAL; // Most suitable error code, I think?
        size = conf->size_in_bytes - offs;
    }

    res = pread (conf->src_fd, buf, size, offs + conf->offset_in_bytes);
    if (res == -1)
        res = -errno;

    return res;
}

static int fuseloop_write (const char *path, const char *buf, size_t size, off_t offs, struct fuse_file_info *fi)
{
    int res = 0;
    struct fuse_context *ctx = fuse_get_context ();
    struct cfg *conf = ctx->private_data;

    if (!ISROOT(path))
        return -ENOENT;

    if (conf->size_in_bytes >= 0 && (offs + size) > conf->size_in_bytes)
    {
        // size_t is unsigned, so make sure we don't by accident wrap it
        if (offs > conf->size_in_bytes)
            return -ENOSPC;
        size = conf->size_in_bytes - offs;
    }

    res = pwrite (conf->src_fd, buf, size, offs + conf->offset_in_bytes);
    if (res == -1)
        res = -errno;

    return res;
}

static int fuseloop_getattr (const char *path, struct stat *stbuf)
{
    struct fuse_context *ctx = fuse_get_context ();
    struct cfg *conf = ctx->private_data;

    if (!ISROOT(path))
        return -ENOENT;

    if (fstat (conf->src_fd, stbuf) == -1)
        return -errno;

    stbuf->st_size = conf->size_in_bytes;
    stbuf->st_mode = 0100600;

    return 0;
}

static int fuseloop_fsync (const char *path, int unknown, struct fuse_file_info *fi)
{
    int ret = 0;
    struct fuse_context *ctx = fuse_get_context ();
    struct cfg *conf = ctx->private_data;

    if (!ISROOT(path))
        return -ENOENT;

    ret = fsync (conf->src_fd);
    if (ret == -1)
        ret = -errno;

    return ret;
}


struct fuse_operations ops = {
    .open    = fuseloop_open,
    .read    = fuseloop_read,
    .write   = fuseloop_write,
    .getattr = fuseloop_getattr,
    .fsync   = fuseloop_fsync,
};


enum ERR_CODES {
    BAD_OPTS = 1,
    FAILED_OPEN,
    NO_SIZE,
    FAILED_MOUNT,
    FAILED_FUSENEW,
    FAILED_SIGNALS,
    FAILED_DAEMONIZE,
};


int main (int argc, char *argv[])
{
    int open_mode = O_RDWR;
    struct fuse_chan *chan = 0;
    struct fuse *fs = 0;

    struct cfg conf = { .size_in_bytes = -1, };
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    // FIXME - need to set -odirect_io ?

    // Parse the options
    if (fuse_opt_parse (&args, &conf, all_opts, process_opts) == -1)
        return BAD_OPTS;

    // Make sure we have at least srcfile + mountfile
    if (!conf.src_file || !conf.mount_file)
        syntax (argv[0]);

    // Check if we need to open in read-only mode
    if (conf.open_as_readonly)
        open_mode = O_RDONLY;

    // Show what we've got
    if (conf.debug)
    {
        DBG("size set to: %ld", conf.size_in_bytes);
        DBG("offset set to: %ld", conf.offset_in_bytes);
        DBG("using open mode: %x", open_mode);
    }

    conf.src_fd = open (conf.src_file, open_mode);
    if (conf.src_fd < 0)
    {
        perror ("Unable to open source file");
        return FAILED_OPEN;
    }

    // Obtain the size, if necessary
    if (conf.size_in_bytes == -1)
    {
        // Use actual file size
        struct stat st;
        if (fstat (conf.src_fd, &st) == -1)
        {
            perror ("Unable to obtain file size of source file");
            return NO_SIZE;
        }
        DBG("looked up file size: %ld", st.st_size);
        conf.size_in_bytes = st.st_size;
    }
    if (conf.debug)
        DBG("file size is: %ld", conf.size_in_bytes);

    // Mount a fuse on the specified file
    chan = fuse_mount (conf.mount_file, &args);
    if (!chan)
    {
        perror ("Unable to attach fuse to mount_file");
        return FAILED_MOUNT;
    }

    // Register us as the handler of said fuse
    fs = fuse_new (chan, &args, &ops, sizeof (ops), &conf);
    if (!fs)
    {
        perror ("Failed to create fuse fs");
        return FAILED_FUSENEW;
    }

    // Pop into the background unless we're debugging
    if (fuse_daemonize (conf.debug) != 0)
    {
        perror ("Failed to daemonize process");
        return FAILED_DAEMONIZE;
    }

    // Set the signal handlers (should this be done before daemonize?)
    if (fuse_set_signal_handlers (fuse_get_session (fs)) != 0)
    {
        perror ("Unable to install signal handlers");
        return FAILED_SIGNALS;
    }

    // Main loop, and cleanup
    fuse_loop_mt (fs);
    fuse_unmount (conf.mount_file, chan);
    fuse_remove_signal_handlers (fuse_get_session (fs));
    fuse_destroy (fs);

    close (conf.src_fd);

    return 0;
}
