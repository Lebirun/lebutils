#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include <lebirun/syscall.h>
#include "cu.h"

int cmd_dhcp(int argc, char **argv) {
    int status;
    int ret;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: dhcp");
            puts("Obtain an IPv4 address via DHCP.");
            return 0;
        }
        fprintf(stderr, "dhcp: unknown argument '%s'\n", argv[i]);
        return 1;
    }

    status = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 0);
    if (status == 1) {
        fprintf(stderr, "dhcp: already configured via DHCP\n");
        return 1;
    }

    printf("Requesting IP address via DHCP...\n");
    ret = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 2);
    if (ret < 0) {
        fprintf(stderr, "dhcp: request failed\n");
        return 1;
    }

    printf("DHCP configuration successful\n");
    return 0;
}
