#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_LOAD (277u | LEBIRUN_SYSCALL_FLAG)

static long insmod_syscall1(unsigned int n, long a1) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1) : "memory");
    return ret;
}

int cmd_insmod(int argc, char **argv) {
    int rc;
    char path[256];

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: insmod <path>");
        puts("Load a Lebirun Kernel Extension (.lke) module.");
        return argc < 2 ? 1 : 0;
    }

    if (getuid() != 0) {
        fprintf(stderr, "insmod: requires root\n");
        return 1;
    }

    if (cu_path_abs(argv[1], path, sizeof(path)) != 0) {
        fprintf(stderr, "insmod: path too long\n");
        return 1;
    }

    rc = (int)insmod_syscall1(SYS_LKE_LOAD, (long)path);
    if (rc < 0) {
        fprintf(stderr, "insmod: failed to load '%s' (error %d)\n", argv[1], rc);
        return 1;
    }

    return 0;
}
