#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include "cu.h"

static void print_umount_usage(void)
{
    printf("Usage: umount <mountpoint>\n");
    printf("\n");
    printf("Unmount a filesystem at the given mountpoint.\n");
}

int cmd_umount(int argc, char **argv) {
    int ret;
    char abs_path[256];

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_umount_usage();
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: umount <mountpoint>\n");
        return 1;
    }

    if (cu_path_abs(argv[1], abs_path, sizeof(abs_path)) != 0) {
        fprintf(stderr, "umount: invalid path\n");
        return 1;
    }

    ret = (int)leb_syscall1(LEB_SYSCALL_VFS_UMOUNT, (long)abs_path);
    if (ret < 0) {
        fprintf(stderr, "umount: failed to unmount %s (%d)\n", abs_path, ret);
        return 1;
    }

    return 0;
}
