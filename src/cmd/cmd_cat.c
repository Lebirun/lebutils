#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define O_RDONLY 0

static int show_line_numbers = 0;

static int cat_one(const char *arg) {
    char path[256];
    if (cu_path_abs(arg, path, sizeof(path)) < 0) return 1;

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printf("cat: cannot open '%s'\n", path);
        return 1;
    }

    unsigned int size = 0;
    unsigned int type = 0;
    if (vfs_stat(fd, &size, &type) < 0) {
        printf("cat: cannot stat '%s'\n", path);
        vfs_close_fd(fd);
        return 1;
    }
    if (type & 0x02) {
        printf("cat: '%s' is a directory\n", path);
        vfs_close_fd(fd);
        return 1;
    }

    char buf[256];
    int rd;
    int line = 1;
    int at_line_start = 1;
    while ((rd = vfs_read_fd(fd, buf, sizeof(buf))) > 0) {
        if (show_line_numbers) {
            for (int i = 0; i < rd; i++) {
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
    }

    if (rd < 0) {
        printf("cat: read error on '%s'\n", path);
        vfs_close_fd(fd);
        return 1;
    }

    vfs_close_fd(fd);
    return 0;
}

int cmd_cat(int argc, char **argv) {
    show_line_numbers = 0;
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i] + 1; *p; p++) {
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

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-') {
            if (cat_one(argv[i]) != 0) rc = 1;
        }
    }
    return rc;
}
