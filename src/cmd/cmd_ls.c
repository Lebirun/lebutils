#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define O_RDONLY 0
#define O_DIRECTORY 0200000

int cmd_ls(int argc, char **argv) {
    int show_all, long_format, i;
    const char *arg, *p;
    char path[256];
    int fd;
    uint64_t size, type;
    unsigned int entry_type, col, idx;
    char name[64];
    const char *type_str;

    show_all = 0;
    long_format = 0;
    arg = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            puts("Usage: ls [-al] [DIRECTORY]");
            puts("List directory contents.");
            puts("");
            puts("  -a   show all entries including hidden");
            puts("  -l   use long listing format");
            puts("  --help  display this help and exit");
            return 0;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'a') show_all = 1;
                else if (*p == 'l') long_format = 1;
            }
        } else {
            arg = argv[i];
        }
    }

    if (arg && *arg) {
        if (cu_path_abs(arg, path, sizeof(path)) < 0) return 1;
    } else {
        if (!getcwd(path, sizeof(path))) return 1;
    }

    fd = vfs_open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        fd = vfs_open(path, O_RDONLY);
    }
    if (fd < 0) {
        printf("ls: cannot access '%s'\n", path);
        return 1;
    }

    size = 0;
    type = 0;
    if (vfs_stat(fd, &size, &type) == 0) {
        if ((type & 0x07) != 0x02 && (type & 0x08) == 0) {
            vfs_close_fd(fd);
            puts(path);
            return 0;
        }
    }

    entry_type = 0;
    col = 0;
    for (idx = 0; idx < 1000; idx++) {
        if (vfs_readdir(fd, name, &entry_type, idx) < 0) break;
        if (!show_all && name[0] == '.') continue;
        
        if (long_format) {
            if (entry_type == 0x02 || entry_type == 0x08)
                type_str = "d";
            else if (entry_type == 0x03)
                type_str = "c";
            else if (entry_type == 0x04)
                type_str = "b";
            else if (entry_type == 0x06)
                type_str = "l";
            else if (entry_type == 0x05)
                type_str = "p";
            else
                type_str = "-";
            printf("%s  %s\n", type_str, name);
        } else {
            printf("%-16s", name);
            col++;
            if (col >= 4) {
                putchar('\n');
                col = 0;
            }
        }
    }
    if (!long_format && col > 0) putchar('\n');

    vfs_close_fd(fd);
    return 0;
}
