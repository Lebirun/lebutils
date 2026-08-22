#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define O_RDONLY 0

static int show_line_numbers = 0;

static int cat_one(const char *arg) {
    char path[256];
    int fd, rd, line, at_line_start, i;
    uint64_t size, type;
    char buf[256];
    char last_char;

    if (cu_path_abs(arg, path, sizeof(path)) < 0) return 1;

    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printf("cat: cannot open '%s'\n", path);
        return 1;
    }

    size = 0;
    type = 0;
    if (vfs_stat(fd, &size, &type) < 0) {
        printf("cat: cannot stat '%s'\n", path);
        vfs_close_fd(fd);
        return 1;
    }
    if ((type & 0x07) == 0x02) {
        printf("cat: '%s' is a directory\n", path);
        vfs_close_fd(fd);
        return 1;
    }

    line = 1;
    at_line_start = 1;
    last_char = '\n';
    while ((rd = vfs_read_fd(fd, buf, sizeof(buf))) > 0) {
        if (show_line_numbers) {
            for (i = 0; i < rd; i++) {
                if (at_line_start) {
                    printf("%6d  ", line++);
                    at_line_start = 0;
                }
                putchar(buf[i]);
                if (buf[i] == '\n') at_line_start = 1;
            }
        } else {
            write(STDOUT_FILENO, buf, (unsigned int)rd);
        }
        last_char = buf[rd - 1];
    }

    if (rd < 0) {
        printf("cat: read error on '%s'\n", path);
        vfs_close_fd(fd);
        return 1;
    }

    if (last_char != '\n') {
        putchar('\n');
    }

    vfs_close_fd(fd);
    return 0;
}

int cmd_cat(int argc, char **argv) {
    int file_count, i, rc;
    const char *p;

    show_line_numbers = 0;
    file_count = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: cat [-n] FILE...");
            puts("Concatenate files and print to standard output.");
            puts("");
            puts("  -n         number all output lines");
            puts("  -h, --help display this help and exit");
            return 0;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'n') show_line_numbers = 1;
            }
        } else {
            file_count++;
        }
    }

    if (file_count == 0) {
        printf("cat: missing operand\n");
        return 1;
    }

    rc = 0;
    for (i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') {
            if (cat_one(argv[i]) != 0) rc = 1;
        }
    }
    return rc;
}
