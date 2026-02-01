#include <stdio.h>
#include <unistd.h>
#include "cu.h"

int cmd_rm(int argc, char **argv) {
    int force, file_count, i, ret, rc;
    const char *p;
    char path[256];

    force = 0;
    file_count = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
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

    rc = 0;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            if (!force) { rc = 1; }
            continue;
        }

        ret = vfs_unlink(path);
        if (ret < 0 && !force) {
            printf("rm: cannot remove '%s'\n", path);
            rc = 1;
        }
    }
    return rc;
}
