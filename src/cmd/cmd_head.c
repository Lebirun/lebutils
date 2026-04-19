#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

#define O_RDONLY 0
#define HEAD_BUFSZ 512
#define HEAD_DEFAULT_LINES 10

int cmd_head(int argc, char **argv) {
    int nlines;
    int i;
    int fd;
    int rd;
    int lines_printed;
    int j;
    char path[256];
    char buf[HEAD_BUFSZ];

    nlines = HEAD_DEFAULT_LINES;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: head [-n NUM] [file]");
            puts("Print the first NUM lines of a file (default 10).");
            return 0;
        }
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            nlines = atoi(argv[i + 1]);
            if (nlines <= 0) nlines = HEAD_DEFAULT_LINES;
            i++;
        } else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9') {
            nlines = atoi(argv[i] + 1);
            if (nlines <= 0) nlines = HEAD_DEFAULT_LINES;
        }
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-n") == 0) {
                i++;
            }
            continue;
        }

        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            fprintf(stderr, "head: invalid path '%s'\n", argv[i]);
            return 1;
        }

        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "head: cannot open '%s'\n", path);
            return 1;
        }

        lines_printed = 0;
        while (lines_printed < nlines && (rd = vfs_read_fd(fd, buf, HEAD_BUFSZ)) > 0) {
            for (j = 0; j < rd && lines_printed < nlines; j++) {
                putchar(buf[j]);
                if (buf[j] == '\n') lines_printed++;
            }
        }

        vfs_close_fd(fd);
        return 0;
    }

    fprintf(stderr, "head: missing file operand\n");
    return 1;
}
