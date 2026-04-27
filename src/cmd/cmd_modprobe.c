#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define LEBIRUN_SYSCALL_FLAG 0x80000000u
#define SYS_LKE_LOAD   (277u | LEBIRUN_SYSCALL_FLAG)
#define SYS_LKE_UNLOAD (278u | LEBIRUN_SYSCALL_FLAG)

#define LKE_DIR "/lib/lke"

static long modprobe_syscall1(unsigned int n, long a1) {
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "b"(a1) : "memory");
    return ret;
}

int cmd_modprobe(int argc, char **argv) {
    char path[256];
    int remove_mode;
    const char *name;
    int n;
    int rc;

    remove_mode = 0;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: modprobe [-r] <name>");
        puts("Load (or unload with -r) a module from " LKE_DIR " by name.");
        return argc < 2 ? 1 : 0;
    }

    if (strcmp(argv[1], "-r") == 0) {
        remove_mode = 1;
        if (argc < 3) {
            fprintf(stderr, "modprobe: -r requires a module name\n");
            return 1;
        }
        name = argv[2];
    } else {
        name = argv[1];
    }

    if (getuid() != 0) {
        fprintf(stderr, "modprobe: requires root\n");
        return 1;
    }

    if (remove_mode) {
        rc = (int)modprobe_syscall1(SYS_LKE_UNLOAD, (long)name);
        if (rc < 0) {
            fprintf(stderr, "modprobe: failed to unload '%s' (error %d)\n", name, rc);
            return 1;
        }
        return 0;
    }

    n = snprintf(path, sizeof(path), "%s/%s.lke", LKE_DIR, name);
    if (n < 0 || n >= (int)sizeof(path)) {
        fprintf(stderr, "modprobe: name too long\n");
        return 1;
    }

    rc = (int)modprobe_syscall1(SYS_LKE_LOAD, (long)path);
    if (rc < 0) {
        fprintf(stderr, "modprobe: failed to load '%s' (error %d)\n", name, rc);
        return 1;
    }

    return 0;
}
