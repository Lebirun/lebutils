#include <stdio.h>
#include <unistd.h>
#include "cu.h"

int cmd_rm(int argc, char **argv) {
    int force = 0;
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p == 'f') force = 1;
            }
        } else {
            file_count++;
        }
    }

    if (file_count == 0) {
        if (force) return 0;
        printf("rm: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        char path[256];
        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            if (!force) { rc = 1; }
            continue;
        }

        int ret = vfs_unlink(path);
        if (ret < 0 && !force) {
            printf("rm: cannot remove '%s'\n", path);
            rc = 1;
        }
    }
    return rc;
}
