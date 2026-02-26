#include <stdio.h>
#include <lebirun.h>
#include "cu.h"

int cmd_dhcp(int argc, char **argv) {
    int status;
    int ret;

    (void)argv;
    if (argc > 1) {
        fprintf(stderr, "Usage: dhcp\n");
        return 1;
    }

    status = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 0);
    if (status == 1) {
        fprintf(stderr, "dhcp: already configured\n");
        return 1;
    }

    ret = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 2);
    if (ret < 0) {
        fprintf(stderr, "dhcp: failed\n");
        return 1;
    }

    return 0;
}
