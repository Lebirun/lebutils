#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_LIST (279u | LEBIRUN_SYSCALL_FLAG)

static long lsmod_syscall2(unsigned int n, long a1, long a2) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static char *lsmod_get_names(int *size_out) {
    char *names;
    int required;
    int actual;

    required = (int)lsmod_syscall2(SYS_LKE_LIST, 0, 0);
    if (required <= 0) {
        *size_out = required;
        return NULL;
    }
    for (;;) {
        names = (char *)malloc((size_t)required);
        if (!names) {
            *size_out = -12;
            return NULL;
        }
        actual = (int)lsmod_syscall2(SYS_LKE_LIST, (long)names, required);
        if (actual <= required) {
            if (actual < 0) {
                free(names);
                names = NULL;
            }
            *size_out = actual;
            return names;
        }
        free(names);
        required = actual;
    }
}

int cmd_lsmod(int argc, char **argv) {
    char *list;
    int list_size;
    size_t offset;
    size_t name_len;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: lsmod");
        puts("List all loaded Lebirun Kernel Extension modules.");
        return 0;
    }

    list = lsmod_get_names(&list_size);
    if (list_size < 0) {
        fprintf(stderr, "lsmod: list failed (%d)\n", list_size);
        return 1;
    }

    if (list_size == 0) {
        printf("No modules loaded.\n");
        free(list);
        return 0;
    }

    printf("Module\n");
    offset = 0;
    while (offset < (size_t)list_size) {
        name_len = strlen(list + offset);
        printf("%s\n", list + offset);
        offset += name_len + 1;
    }

    free(list);
    return 0;
}
