#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include <lebirun/syscall.h>
#include "cu.h"

#define PANIC_CMD_CUSTOM  0
#define PANIC_CMD_OOM     1
#define PANIC_CMD_ASSERT  2
#define PANIC_CMD_GENERIC 3

static void print_help(void) {
    puts("Usage: panic [OPTION]");
    puts("Trigger a kernel panic for debugging purposes.\n");
    puts("Options:");
    puts("  --custom MSG...  Trigger a custom kernel panic with MSG");
    puts("  --oom            Trigger an out-of-memory panic");
    puts("  --assert         Trigger an assert failure panic");
    puts("  --generic        Trigger a generic kernel panic");
    puts("  --help           Display this help and exit");
}

int cmd_panic(int argc, char **argv) {
    int i;
    int j;
    char msg_buf[128];
    int pos;
    int len;

    if (argc < 2) {
        print_help();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--custom") == 0) {
            pos = 0;
            msg_buf[0] = '\0';
            for (j = i + 1; j < argc; j++) {
                if (pos > 0 && pos < 127) {
                    msg_buf[pos++] = ' ';
                }
                len = 0;
                while (argv[j][len] != '\0') len++;
                if (pos + len > 127) len = 127 - pos;
                memcpy(msg_buf + pos, argv[j], len);
                pos += len;
            }
            msg_buf[pos] = '\0';
            leb_syscall2(LEB_SYSCALL_PANIC, PANIC_CMD_CUSTOM, (long)msg_buf);
            return 0;
        }
        if (strcmp(argv[i], "--oom") == 0) {
            leb_syscall2(LEB_SYSCALL_PANIC, PANIC_CMD_OOM, 0);
            return 0;
        }
        if (strcmp(argv[i], "--assert") == 0) {
            leb_syscall2(LEB_SYSCALL_PANIC, PANIC_CMD_ASSERT, 0);
            return 0;
        }
        if (strcmp(argv[i], "--generic") == 0) {
            leb_syscall2(LEB_SYSCALL_PANIC, PANIC_CMD_GENERIC, 0);
            return 0;
        }
    }

    printf("panic: unknown option '%s'\n", argv[1]);
    puts("Try 'panic --help' for more information.");
    return 1;
}
