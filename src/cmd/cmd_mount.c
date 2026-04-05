#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include "cu.h"

static void print_mount_usage(void)
{
    printf("Usage: mount [-t fstype] [-o ro|rw] <device> <mountpoint>\n");
    printf("       mount\n");
    printf("\n");
    printf("With no arguments, list all mounted filesystems.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -t <type>    Specify the filesystem type\n");
    printf("  -o ro|rw     Mount read-only or read-write\n");
}

int cmd_mount(int argc, char **argv) {
    const char *source;
    const char *target;
    const char *fstype;
    const char *opts;
    unsigned long flags;
    int i;
    int ret;
    char abs_source[256];
    char abs_target[256];

    source = NULL;
    target = NULL;
    fstype = NULL;
    opts = NULL;
    flags = 0;

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_mount_usage();
        return 0;
    }

    if (argc < 2) {
        vfs_mounts();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "mount: -t requires an argument\n");
                return 1;
            }
            fstype = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "mount: -o requires an argument\n");
                return 1;
            }
            opts = argv[++i];
        } else if (!source) {
            source = argv[i];
        } else if (!target) {
            target = argv[i];
        } else {
            fprintf(stderr, "mount: too many arguments\n");
            return 1;
        }
    }

    if (!source || !target) {
        fprintf(stderr, "Usage: mount [-t fstype] [-o ro|rw] <device> <mountpoint>\n");
        return 1;
    }

    if (!fstype) {
        fprintf(stderr, "mount: filesystem type required (-t)\n");
        return 1;
    }

    if (cu_path_abs(source, abs_source, sizeof(abs_source)) != 0) {
        fprintf(stderr, "mount: invalid source path\n");
        return 1;
    }

    if (cu_path_abs(target, abs_target, sizeof(abs_target)) != 0) {
        fprintf(stderr, "mount: invalid target path\n");
        return 1;
    }

    if (opts) {
        if (strcmp(opts, "ro") == 0)
            flags |= LEB_MS_RDONLY;
    }

    ret = (int)leb_syscall4(LEB_SYSCALL_VFS_MOUNT,
                            (long)abs_source, (long)abs_target, (long)fstype, (long)flags);
    if (ret < 0) {
        if (ret == -2)
            fprintf(stderr, "mount: %s: no such file or directory\n", abs_target);
        else
            fprintf(stderr, "mount: failed to mount %s on %s (%d)\n",
                    abs_source, abs_target, ret);
        return 1;
    }

    return 0;
}
