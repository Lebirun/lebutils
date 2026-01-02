#include <stdio.h>
#include <unistd.h>
#include "cu.h"

int cmd_touch(int argc, char **argv) {
    int no_create = 0;
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p == 'c') no_create = 1;
            }
        } else {
            file_count++;
        }
    }

    if (file_count == 0) {
        printf("touch: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        char path[256];
        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            rc = 1;
            continue;
        }

        int fd = vfs_open(path, 0);
        if (fd >= 0) {
            vfs_close_fd(fd);
            continue;
        }
        
        if (no_create) continue;

        int ret = vfs_create(path, 0x06);
        if (ret < 0) {
            printf("touch: cannot create '%s'\n", path);
            rc = 1;
        }
    }
    return rc;
}
