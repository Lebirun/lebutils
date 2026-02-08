#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

static int mkdir_recursive(const char *path) {
    char tmp[256];
    char *p;
    size_t len, i;

    len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    
    for (i = 0; i <= len; i++) tmp[i] = path[i];
    
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            vfs_mkdir(tmp, 0x07);
            *p = '/';
        }
    }
    return vfs_mkdir(tmp, 0x07);
}

int cmd_mkdir(int argc, char **argv) {
    int parents, dir_count, i, ret, rc;
    const char *p;
    char path[256];

    parents = 0;
    dir_count = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: mkdir [-p] DIRECTORY...");
            puts("Create directories.");
            puts("");
            puts("  -p         create parent directories as needed");
            puts("  -h, --help display this help and exit");
            return 0;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'p') parents = 1;
            }
        } else {
            dir_count++;
        }
    }

    if (dir_count == 0) {
        printf("mkdir: missing operand\n");
        return 1;
    }

    rc = 0;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            rc = 1;
            continue;
        }

        if (parents) {
            ret = mkdir_recursive(path);
        } else {
            ret = vfs_mkdir(path, 0x07);
        }
        
        if (ret < 0 && !parents) {
            printf("mkdir: cannot create directory '%s'\n", path);
            rc = 1;
        }
    }
    return rc;
}
