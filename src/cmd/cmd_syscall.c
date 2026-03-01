#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

static inline int do_syscall0(int num)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline int do_syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
        : "memory"
    );
    return ret;
}

static inline int do_syscall2(int num, int arg1, int arg2)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2)
        : "memory"
    );
    return ret;
}

static inline int do_syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

static void hexdump(const unsigned char *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        printf("%02x", buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
        else if ((i + 1) % 4 == 0)
            printf("  ");
        else
            printf(" ");
    }
    if (len > 0 && len % 16 != 0)
        printf("\n");
}

static void print_usage(void)
{
    printf("Usage: syscall [--buf <size>] <number> [arg1] [arg2] [arg3]\n");
    printf("\n");
    printf("Invoke a raw kernel syscall by number.\n");
    printf("Numbers can be decimal, hex (0x...), or octal (0...).\n");
    printf("\n");
    printf("Options:\n");
    printf("  --buf <size>  Allocate a buffer of <size> bytes, pass as\n");
    printf("                arg1=buf, arg2=size; hexdump result after call.\n");
    printf("                Extra args shift to arg3.\n");
}

int cmd_syscall(int argc, char **argv)
{
    int nr;
    int arg1;
    int arg2;
    int arg3;
    int ret;
    int buf_mode;
    int buf_size;
    unsigned char *buf;
    int argi;

    buf_mode = 0;
    buf_size = 0;
    buf = NULL;
    argi = 1;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        print_usage();
        return (argc < 2) ? 1 : 0;
    }

    if (strcmp(argv[argi], "--buf") == 0) {
        if (argi + 2 >= argc) {
            printf("syscall: --buf requires <size> and <number>\n");
            return 1;
        }
        buf_mode = 1;
        argi++;
        buf_size = (int)strtol(argv[argi], NULL, 0);
        if (buf_size <= 0 || buf_size > 4096) {
            printf("syscall: buffer size must be 1-4096\n");
            return 1;
        }
        argi++;
    }

    if (argi >= argc) {
        printf("syscall: missing syscall number\n");
        return 1;
    }

    nr = (int)strtol(argv[argi], NULL, 0);
    argi++;

    if (buf_mode) {
        buf = (unsigned char *)malloc(buf_size);
        if (!buf) {
            printf("syscall: failed to allocate buffer\n");
            return 1;
        }
        memset(buf, 0, buf_size);
        arg1 = (int)(unsigned int)buf;
        arg2 = buf_size;
        arg3 = (argi < argc) ? (int)strtol(argv[argi], NULL, 0) : 0;
        ret = do_syscall3(nr, arg1, arg2, arg3);
        printf("syscall(%d, buf, %d) = %d (0x%08x)\n", nr, buf_size, ret, (unsigned int)ret);
        hexdump(buf, (ret > 0 && ret <= buf_size) ? ret : buf_size);
        free(buf);
    } else {
        arg1 = (argi < argc) ? (int)strtol(argv[argi++], NULL, 0) : 0;
        arg2 = (argi < argc) ? (int)strtol(argv[argi++], NULL, 0) : 0;
        arg3 = (argi < argc) ? (int)strtol(argv[argi++], NULL, 0) : 0;

        if (argc - 1 <= 1)
            ret = do_syscall0(nr);
        else if (argc - 1 == 2)
            ret = do_syscall1(nr, arg1);
        else if (argc - 1 == 3)
            ret = do_syscall2(nr, arg1, arg2);
        else
            ret = do_syscall3(nr, arg1, arg2, arg3);

        printf("syscall(%d) = %d (0x%08x)\n", nr, ret, (unsigned int)ret);
    }

    return 0;
}
