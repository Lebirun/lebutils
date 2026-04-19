#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

#define O_RDONLY 0
#define TAIL_BUFSZ 512
#define TAIL_DEFAULT_LINES 10
#define TAIL_MAX_STORE (TAIL_DEFAULT_LINES * 2 + 10)

int cmd_tail(int argc, char **argv) {
    int nlines;
    int i;
    int fd;
    int rd;
    int j;
    char path[256];
    char buf[TAIL_BUFSZ];
    char *lines[1024];
    int line_count;
    char *store;
    int store_pos;
    int store_cap;
    int start;

    nlines = TAIL_DEFAULT_LINES;
    store_cap = 65536;
    store_pos = 0;
    line_count = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: tail [-n NUM] [file]");
            puts("Print the last NUM lines of a file (default 10).");
            return 0;
        }
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            nlines = atoi(argv[i + 1]);
            if (nlines <= 0) nlines = TAIL_DEFAULT_LINES;
            i++;
        } else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9') {
            nlines = atoi(argv[i] + 1);
            if (nlines <= 0) nlines = TAIL_DEFAULT_LINES;
        }
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-n") == 0) i++;
            continue;
        }

        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            fprintf(stderr, "tail: invalid path '%s'\n", argv[i]);
            return 1;
        }

        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "tail: cannot open '%s'\n", path);
            return 1;
        }

        store = malloc(store_cap);
        if (!store) {
            fprintf(stderr, "tail: out of memory\n");
            vfs_close_fd(fd);
            return 1;
        }

        while ((rd = vfs_read_fd(fd, buf, TAIL_BUFSZ)) > 0) {
            for (j = 0; j < rd; j++) {
                if (store_pos < store_cap - 1) {
                    store[store_pos++] = buf[j];
                }
            }
        }
        store[store_pos] = '\0';
        vfs_close_fd(fd);

        lines[0] = store;
        line_count = 1;
        for (j = 0; j < store_pos && line_count < 1023; j++) {
            if (store[j] == '\n') {
                store[j] = '\0';
                if (j + 1 < store_pos) {
                    lines[line_count++] = &store[j + 1];
                }
            }
        }

        start = line_count - nlines;
        if (start < 0) start = 0;

        for (j = start; j < line_count; j++) {
            puts(lines[j]);
        }

        free(store);
        return 0;
    }

    fprintf(stderr, "tail: missing file operand\n");
    return 1;
}
