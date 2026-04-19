#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "cu.h"

int cmd_mv(int argc, char **argv) {
    char src[256];
    char dst[256];

    if (argc < 3) {
        fprintf(stderr, "Usage: mv <source> <destination>\n");
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: mv <source> <destination>");
        puts("Move or rename a file or directory.");
        return 0;
    }

    if (cu_path_abs(argv[1], src, sizeof(src)) < 0) {
        fprintf(stderr, "mv: invalid source path\n");
        return 1;
    }

    if (cu_path_abs(argv[2], dst, sizeof(dst)) < 0) {
        fprintf(stderr, "mv: invalid destination path\n");
        return 1;
    }

    if (rename(src, dst) < 0) {
        fprintf(stderr, "mv: cannot move '%s' to '%s'\n", src, dst);
        return 1;
    }

    return 0;
}
