#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cu.h"

static int parse_octal_mode(const char *s, int *mode) {
    int val;
    const char *p;

    val = 0;
    for (p = s; *p; p++) {
        if (*p < '0' || *p > '7') return -1;
        val = (val << 3) | (*p - '0');
    }
    if (val > 07777) return -1;
    *mode = val;
    return 0;
}

int cmd_chmod(int argc, char **argv) {
    int i, mode, rc;
    char path[256];

    if (argc < 3) {
        fprintf(stderr, "Usage: chmod MODE FILE...\n");
        return 1;
    }

    if (parse_octal_mode(argv[1], &mode) != 0) {
        fprintf(stderr, "chmod: invalid mode '%s'\n", argv[1]);
        return 1;
    }

    rc = 0;
    for (i = 2; i < argc; i++) {
        if (cu_path_abs(argv[i], path, sizeof(path)) != 0) {
            fprintf(stderr, "chmod: path too long: %s\n", argv[i]);
            rc = 1;
            continue;
        }
        if (chmod(path, mode) != 0) {
            fprintf(stderr, "chmod: cannot change mode of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
