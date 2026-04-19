#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lebirun.h>
#include "cu.h"

static unsigned int ping_parse_ip(const char *s) {
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    int i;
    int octet;
    int dots;

    a = 0;
    b = 0;
    c = 0;
    d = 0;
    octet = 0;
    dots = 0;

    for (i = 0; s[i]; i++) {
        if (s[i] >= '0' && s[i] <= '9') {
            octet = octet * 10 + (s[i] - '0');
        } else if (s[i] == '.') {
            if (dots == 0) a = octet;
            else if (dots == 1) b = octet;
            else if (dots == 2) c = octet;
            octet = 0;
            dots++;
        } else {
            return 0;
        }
    }
    if (dots != 3) return 0;
    d = octet;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

int cmd_ping(int argc, char **argv) {
    int count;
    int i;
    int timeout;
    unsigned int ip;
    const char *target;
    int ret;
    int sent;
    int received;

    count = 4;
    timeout = 3000;
    target = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: ping [-c COUNT] [-W TIMEOUT] <ip>");
            puts("Send ICMP echo requests.");
            puts("  -c COUNT   number of pings (default 4)");
            puts("  -W TIMEOUT timeout in seconds (default 3)");
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count <= 0) count = 4;
        } else if (strcmp(argv[i], "-W") == 0 && i + 1 < argc) {
            timeout = atoi(argv[++i]) * 1000;
            if (timeout <= 0) timeout = 3000;
        } else if (argv[i][0] != '-') {
            target = argv[i];
        }
    }

    if (!target) {
        fprintf(stderr, "ping: missing target address\n");
        return 1;
    }

    ip = ping_parse_ip(target);
    if (ip == 0) {
        fprintf(stderr, "ping: invalid address '%s'\n", target);
        return 1;
    }

    printf("PING %s: %d data bytes\n", target, 56);

    sent = 0;
    received = 0;
    for (i = 0; i < count; i++) {
        sent++;
        ret = (int)leb_syscall3(LEB_SYSCALL_NET_PING_ONE, (long)ip, (long)(i + 1), (long)timeout);
        if (ret >= 0) {
            received++;
            printf("Reply from %s: seq=%d time=%dms\n", target, i + 1, ret);
        } else {
            printf("Request timeout for seq %d\n", i + 1);
        }
        if (i + 1 < count) {
            sleep_ms(1000);
        }
    }

    printf("\n--- %s ping statistics ---\n", target);
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
        sent, received, sent > 0 ? ((sent - received) * 100 / sent) : 0);

    return (received > 0) ? 0 : 1;
}
