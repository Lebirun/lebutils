#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_LIST (279u | LEBIRUN_SYSCALL_FLAG)

#define LKE_NAME_MAX 128
#define LKE_MAX_LIST 1024

typedef struct {
    char name[LKE_NAME_MAX];
    int loaded;
} lsmod_info_t;

static long lsmod_syscall2(unsigned int n, long a1, long a2) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1), "c"(a2) : "memory");
    return ret;
}

int cmd_lsmod(int argc, char **argv) {
    lsmod_info_t *list;
    int count;
    int i;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: lsmod");
        puts("List all loaded Lebirun Kernel Extension modules.");
        return 0;
    }

    list = (lsmod_info_t *)malloc(LKE_MAX_LIST * sizeof(lsmod_info_t));
    if (!list) {
        fprintf(stderr, "lsmod: out of memory\n");
        return 1;
    }

    count = (int)lsmod_syscall2(SYS_LKE_LIST, (long)list, LKE_MAX_LIST);
    if (count < 0) {
        fprintf(stderr, "lsmod: list failed (%d)\n", count);
        free(list);
        return 1;
    }

    if (count == 0) {
        printf("No modules loaded.\n");
        free(list);
        return 0;
    }

    printf("Module\n");
    for (i = 0; i < count; i++) {
        printf("%s\n", list[i].name);
    }

    free(list);
    return 0;
}
