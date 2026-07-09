#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cu.h"

#define O_RDONLY 0
#define CP_BUFSZ 4096

int cmd_cp(int argc, char **argv) {
    char src[256];
    char dst[256];
    int sfd;
    int dfd;
    char buf[CP_BUFSZ];
    int rd;
    uint64_t size;
    uint64_t type;

    if (argc < 3) {
        fprintf(stderr, "Usage: cp <source> <destination>\n");
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: cp <source> <destination>");
        puts("Copy a file from source to destination.");
        return 0;
    }

    if (cu_path_abs(argv[1], src, sizeof(src)) < 0) {
        fprintf(stderr, "cp: invalid source path\n");
        return 1;
    }

    if (cu_path_abs(argv[2], dst, sizeof(dst)) < 0) {
        fprintf(stderr, "cp: invalid destination path\n");
        return 1;
    }

    sfd = vfs_open(src, O_RDONLY);
    if (sfd < 0) {
        fprintf(stderr, "cp: cannot open '%s'\n", src);
        return 1;
    }

    size = 0;
    type = 0;
    if (vfs_stat(sfd, &size, &type) < 0 || (type & 0x07) == 0x02) {
        fprintf(stderr, "cp: '%s' is not a regular file\n", src);
        vfs_close_fd(sfd);
        return 1;
    }

    if (vfs_create(dst, 0644) < 0) {
        fprintf(stderr, "cp: cannot create '%s'\n", dst);
        vfs_close_fd(sfd);
        return 1;
    }

    dfd = vfs_open(dst, 1);
    if (dfd < 0) {
        fprintf(stderr, "cp: cannot open '%s' for writing\n", dst);
        vfs_close_fd(sfd);
        return 1;
    }

    while ((rd = vfs_read_fd(sfd, buf, CP_BUFSZ)) > 0) {
        if (vfs_write_fd(dfd, buf, rd) < 0) {
            fprintf(stderr, "cp: write error\n");
            vfs_close_fd(sfd);
            vfs_close_fd(dfd);
            return 1;
        }
    }

    vfs_close_fd(sfd);
    vfs_close_fd(dfd);
    return 0;
}
