#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>
#include "cu.h"

#define O_RDONLY 0
#define O_DIRECTORY 0200000

static const char *find_name;
static int find_type;

static int find_is_dir(unsigned int etype) {
    return ((etype & 0x07) == 0x02) || (etype & 0x08);
}

static int find_type_match(unsigned int etype) {
    if (!find_type) return 1;
    if (find_type == 'd') return find_is_dir(etype);
    if (find_type == 'l') return (etype & 0x07) == 0x06;
    if (find_type == 'f') return (etype & 0x07) == 0x01;
    if (find_type == 'p') return (etype & 0x07) == 0x05;
    if (find_type == 'c') return (etype & 0x07) == 0x03;
    if (find_type == 'b') return (etype & 0x07) == 0x04;
    if (find_type == 's') return (etype & 0x07) == 0x07;
    return 1;
}

static int find_etype(const char *path, unsigned int *etype) {
    int fd;
    uint64_t size, type;

    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -1;
    size = 0;
    type = 0;
    if (vfs_stat(fd, &size, &type) < 0) {
        vfs_close_fd(fd);
        return -1;
    }
    vfs_close_fd(fd);
    *etype = (unsigned int)type;
    return 0;
}

static void find_show(const char *path, unsigned int etype) {
    if (!find_type_match(etype)) return;
    if (find_name && fnmatch(find_name, cu_basename(path), 0) != 0) return;
    puts(path);
}

static int find_grow(char **path, unsigned int *cap, unsigned int need) {
    char *np;
    unsigned int nc;

    if (need <= *cap) return 0;
    nc = *cap ? *cap : 256;
    while (nc < need) nc *= 2;
    np = (char *)realloc(*path, nc);
    if (!np) {
        fprintf(stderr, "find: out of memory\n");
        return -1;
    }
    *path = np;
    *cap = nc;
    return 0;
}

static int find_walk(char **path, unsigned int *len, unsigned int *cap) {
    int fd;
    unsigned int idx;
    unsigned int etype, base, nlen;
    char name[64];
    int rc;

    fd = vfs_open(*path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return 0;
    rc = 0;
    base = *len;
    for (idx = 0; ; idx++) {
        etype = 0;
        if (vfs_readdir(fd, name, &etype, idx) < 0) break;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        nlen = (unsigned int)strlen(name);
        if (find_grow(path, cap, base + 1 + nlen + 1) < 0) {
            rc = 1;
            break;
        }
        if (base == 1 && (*path)[0] == '/')
            strcpy(*path + base, name);
        else {
            (*path)[base] = '/';
            strcpy(*path + base + 1, name);
        }
        *len = (unsigned int)strlen(*path);
        find_show(*path, etype);
        if (find_is_dir(etype))
            if (find_walk(path, len, cap)) rc = 1;
        (*path)[base] = '\0';
        *len = base;
    }
    vfs_close_fd(fd);
    return rc;
}

static int find_one(const char *arg) {
    char *path;
    unsigned int len, cap, etype;
    int rc;

    cap = 256;
    path = (char *)malloc(cap);
    if (!path) {
        fprintf(stderr, "find: out of memory\n");
        return 1;
    }
    if (cu_path_abs(arg, path, cap) < 0) {
        fprintf(stderr, "find: invalid path '%s'\n", arg);
        free(path);
        return 1;
    }
    if (find_etype(path, &etype) < 0) {
        fprintf(stderr, "find: cannot access '%s'\n", path);
        free(path);
        return 1;
    }
    find_show(path, etype);
    rc = 0;
    if (find_is_dir(etype)) {
        len = (unsigned int)strlen(path);
        rc = find_walk(&path, &len, &cap);
    }
    free(path);
    return rc;
}

static int find_cwd(void) {
    char *path;
    unsigned int len, cap, etype;
    int rc;

    cap = 256;
    path = (char *)malloc(cap);
    if (!path) return 1;
    if (!getcwd(path, cap)) {
        fprintf(stderr, "find: cannot get cwd\n");
        free(path);
        return 1;
    }
    if (find_etype(path, &etype) < 0) {
        fprintf(stderr, "find: cannot access '%s'\n", path);
        free(path);
        return 1;
    }
    find_show(path, etype);
    rc = 0;
    if (find_is_dir(etype)) {
        len = (unsigned int)strlen(path);
        rc = find_walk(&path, &len, &cap);
    }
    free(path);
    return rc;
}

static int find_is_opt(const char *a) {
    return a[0] == '-' && a[1] != '\0';
}

int cmd_find(int argc, char **argv) {
    int i, npaths, rc;

    find_name = NULL;
    find_type = 0;
    npaths = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: find [PATH...] [-name PATTERN] [-type c] [-print]");
            puts("Walk directories and print matching entries.");
            return 0;
        }
        if (strcmp(argv[i], "-name") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "find: -name needs a pattern\n");
                return 1;
            }
            find_name = argv[i];
        } else if (strcmp(argv[i], "-type") == 0) {
            if (++i >= argc || !argv[i][0] || argv[i][1]) {
                fprintf(stderr, "find: -type needs one of fdlpcbs\n");
                return 1;
            }
            find_type = argv[i][0];
        } else if (strcmp(argv[i], "-print") == 0) {
            continue;
        } else if (find_is_opt(argv[i])) {
            fprintf(stderr, "find: unknown option '%s'\n", argv[i]);
            return 1;
        } else {
            npaths++;
        }
    }

    if (!npaths) return find_cwd();
    rc = 0;
    for (i = 1; i < argc; i++) {
        if (find_is_opt(argv[i])) {
            if (strcmp(argv[i], "-name") == 0 || strcmp(argv[i], "-type") == 0) i++;
            continue;
        }
        if (find_one(argv[i])) rc = 1;
    }
    return rc;
}
