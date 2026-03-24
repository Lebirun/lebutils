#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_LOAD   (277u | LEBIRUN_SYSCALL_FLAG)
#define SYS_LKE_UNLOAD (278u | LEBIRUN_SYSCALL_FLAG)
#define SYS_LKE_LIST   (279u | LEBIRUN_SYSCALL_FLAG)

#define LKE_NAME_MAX 128
#define LKE_MAX_LIST 1024

typedef struct {
    char name[LKE_NAME_MAX];
    int loaded;
} lke_info_t;

static long lke_syscall1(unsigned int n, long a1) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1) : "memory");
    return ret;
}

static long lke_syscall2(unsigned int n, long a1, long a2) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1), "c"(a2) : "memory");
    return ret;
}

int cmd_lke(int argc, char **argv) {
    int rc;
    int i;
    int count;
    lke_info_t *list;
    char path[256];

    if (argc < 2) {
        fprintf(stderr, "Usage: lke <load|unload|list> [args]\n");
        return 1;
    }

    if (strcmp(argv[1], "load") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: lke load <path>\n");
            return 1;
        }
        if (getuid() != 0) {
            fprintf(stderr, "lke: load requires root\n");
            return 1;
        }
        if (cu_path_abs(argv[2], path, sizeof(path)) != 0) {
            fprintf(stderr, "lke: path too long\n");
            return 1;
        }
        rc = (int)lke_syscall1(SYS_LKE_LOAD, (long)path);
        if (rc < 0) {
            fprintf(stderr, "lke: load failed (%d)\n", rc);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "unload") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: lke unload <name>\n");
            return 1;
        }
        if (getuid() != 0) {
            fprintf(stderr, "lke: unload requires root\n");
            return 1;
        }
        rc = (int)lke_syscall1(SYS_LKE_UNLOAD, (long)argv[2]);
        if (rc < 0) {
            fprintf(stderr, "lke: unload failed (%d)\n", rc);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        list = (lke_info_t *)malloc(LKE_MAX_LIST * sizeof(lke_info_t));
        if (!list) {
            fprintf(stderr, "lke: out of memory\n");
            return 1;
        }
        count = (int)lke_syscall2(SYS_LKE_LIST, (long)list, LKE_MAX_LIST);
        if (count < 0) {
            fprintf(stderr, "lke: list failed (%d)\n", count);
            free(list);
            return 1;
        }
        if (count == 0) {
            printf("No LKE modules loaded.\n");
            free(list);
            return 0;
        }
        printf("Loaded LKE modules:\n");
        for (i = 0; i < count; i++) {
            printf("  %s\n", list[i].name);
        }
        free(list);
        return 0;
    }

    fprintf(stderr, "lke: unknown command '%s'\n", argv[1]);
    return 1;
}
