#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "cu.h"

static int ln_errno(int ret) {
    if (ret >= 0) return 0;
    if (errno) return errno;
    return -ret;
}

static int ln_one(const char *target, const char *linkpath, int symbolic, int force) {
    char tgt[256];
    char lnk[256];
    int ret;

    if (cu_path_abs(linkpath, lnk, sizeof(lnk)) < 0) {
        fprintf(stderr, "ln: invalid path '%s'\n", linkpath);
        return 1;
    }
    if (symbolic) {
        if (force) vfs_unlink(lnk);
        ret = symlink(target, lnk);
        if (ret < 0) {
            fprintf(stderr, "ln: cannot create symlink '%s': %s\n", lnk, strerror(ln_errno(ret)));
            return 1;
        }
        return 0;
    }
    if (cu_path_abs(target, tgt, sizeof(tgt)) < 0) {
        fprintf(stderr, "ln: invalid path '%s'\n", target);
        return 1;
    }
    if (force) vfs_unlink(lnk);
    ret = link(tgt, lnk);
    if (ret < 0) {
        fprintf(stderr, "ln: cannot create link '%s': %s\n", lnk, strerror(ln_errno(ret)));
        return 1;
    }
    return 0;
}

static int ln_is_dir(const char *path) {
    char abs[256];
    int fd;
    uint64_t size, type;

    if (cu_path_abs(path, abs, sizeof(abs)) < 0) return 0;
    fd = vfs_open(abs, 0);
    if (fd < 0) return 0;
    size = 0;
    type = 0;
    vfs_stat(fd, &size, &type);
    vfs_close_fd(fd);
    return ((type & 0x07) == 0x02) || (type & 0x08);
}

int cmd_ln(int argc, char **argv) {
    int symbolic, force, i, nsrc, rc;
    const char *dst;
    char dest[256];
    const char *base;

    symbolic = 0;
    force = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: ln [-s] [-f] <target> <link>");
            puts("       ln [-s] [-f] <target>... <dir>");
            puts("Create hard or symbolic links.");
            return 0;
        }
        if (strcmp(argv[i], "--symbolic") == 0) { symbolic = 1; continue; }
        if (strcmp(argv[i], "--force") == 0) { force = 1; continue; }
        if (argv[i][0] == '-') {
            const char *p;
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 's') symbolic = 1;
                else if (*p == 'f') force = 1;
            }
            continue;
        }
        break;
    }

    nsrc = 0;
    for (; i + nsrc < argc; nsrc++) ;
    if (nsrc < 2) {
        fprintf(stderr, "Usage: ln [-s] [-f] <target> <link>\n");
        return 1;
    }

    dst = argv[argc - 1];
    if (nsrc > 2 || ln_is_dir(dst)) {
        if (!ln_is_dir(dst)) {
            fprintf(stderr, "ln: '%s' is not a directory\n", dst);
            return 1;
        }
        rc = 0;
        for (; i < argc - 1; i++) {
            base = cu_basename(argv[i]);
            if (!*base) base = argv[i];
            snprintf(dest, sizeof(dest), "%s/%s", dst, base);
            if (ln_one(argv[i], dest, symbolic, force)) rc = 1;
        }
        return rc;
    }
    return ln_one(argv[i], dst, symbolic, force);
}
