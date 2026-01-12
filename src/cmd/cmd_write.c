#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define O_RDONLY 0

int cmd_write(int argc, char **argv) {
    int append, opt_idx, fd, ret, written;
    char path[256], text[1024];
    int text_pos;
    unsigned int size, type;
    char buf[256];
    int rd;

    append = 0;
    opt_idx = 1;
    
    while (opt_idx < argc && argv[opt_idx][0] == '-') {
        for (const char *p = argv[opt_idx] + 1; *p; p++) {
            if (*p == 'a') append = 1;
        }
        opt_idx++;
    }
    
    if (argc - opt_idx < 2) {
        printf("write: usage: write [-a] <path> <text>\n");
        return 1;
    }

    if (cu_path_abs(argv[opt_idx], path, sizeof(path)) < 0) return 1;

    text_pos = 0;
    for (int i = opt_idx + 1; i < argc && text_pos < 1020; i++) {
        if (i > opt_idx + 1 && text_pos < 1020) text[text_pos++] = ' ';
        const char *p = argv[i];
        while (*p && text_pos < 1020) text[text_pos++] = *p++;
    }
    text[text_pos] = '\0';

    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        ret = vfs_create(path, 0x06);
        if (ret < 0) {
            printf("write: cannot create '%s'\n", path);
            return 1;
        }
        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            printf("write: cannot open '%s' after create\n", path);
            return 1;
        }
    }

    if (append) {
        size = 0;
        type = 0;
        vfs_stat(fd, &size, &type);
        while (size > 0) {
            rd = vfs_read_fd(fd, buf, size > 256 ? 256 : size);
            if (rd <= 0) break;
            size -= (unsigned int)rd;
        }
    }

    written = vfs_write_fd(fd, text, (unsigned int)text_pos);
    vfs_close_fd(fd);

    if (written < 0) {
        printf("write: failed to write to '%s'\n", path);
        return 1;
    }

    return 0;
}
