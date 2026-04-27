#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_UNLOAD (278u | LEBIRUN_SYSCALL_FLAG)

static long rmmod_syscall1(unsigned int n, long a1) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1) : "memory");
    return ret;
}

int cmd_rmmod(int argc, char **argv) {
    int rc;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: rmmod <name>");
        puts("Unload a loaded Lebirun Kernel Extension module by name.");
        return argc < 2 ? 1 : 0;
    }

    if (getuid() != 0) {
        fprintf(stderr, "rmmod: requires root\n");
        return 1;
    }

    rc = (int)rmmod_syscall1(SYS_LKE_UNLOAD, (long)argv[1]);
    if (rc < 0) {
        fprintf(stderr, "rmmod: failed to unload '%s' (error %d)\n", argv[1], rc);
        return 1;
    }

    return 0;
}
