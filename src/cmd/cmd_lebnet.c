#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include "cu.h"

static void print_usage(void) {
    fprintf(stderr, "Usage: lebnet <command> [options]\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  dhcp              Obtain IP address via DHCP\n");
    fprintf(stderr, "  dns <ip>          Set DNS server address\n");
    fprintf(stderr, "  info              Show network interface info\n");
    fprintf(stderr, "  help              Show this help\n");
}

static int parse_ip(const char *str, unsigned long *out) {
    unsigned int a, b, c, d;
    int count;
    int i;
    const char *p;

    a = 0;
    b = 0;
    c = 0;
    d = 0;
    count = 0;
    p = str;

    for (i = 0; p[i]; i++) {
        if (p[i] != '.' && (p[i] < '0' || p[i] > '9'))
            return -1;
    }

    p = str;
    while (*p >= '0' && *p <= '9') {
        a = a * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return -1;
    p++;
    count++;

    while (*p >= '0' && *p <= '9') {
        b = b * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return -1;
    p++;
    count++;

    while (*p >= '0' && *p <= '9') {
        c = c * 10 + (*p - '0');
        p++;
    }
    if (*p != '.') return -1;
    p++;
    count++;

    while (*p >= '0' && *p <= '9') {
        d = d * 10 + (*p - '0');
        p++;
    }
    if (*p != '\0') return -1;
    count++;

    if (count != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        return -1;

    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

static int cmd_lebnet_dhcp(void) {
    int status;
    int ret;

    status = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 0);
    if (status == 1) {
        fprintf(stderr, "lebnet: network already configured via DHCP\n");
        return 1;
    }

    printf("Requesting IP address via DHCP...\n");
    ret = (int)leb_syscall1(LEB_SYSCALL_NET_DHCP, 2);
    if (ret < 0) {
        fprintf(stderr, "lebnet: DHCP request failed\n");
        return 1;
    }

    printf("DHCP configuration successful\n");
    return 0;
}

static int cmd_lebnet_dns(int argc, char **argv) {
    unsigned long ip;

    if (argc < 3) {
        fprintf(stderr, "Usage: lebnet dns <ip address>\n");
        return 1;
    }

    if (parse_ip(argv[2], &ip) < 0) {
        fprintf(stderr, "lebnet: invalid IP address '%s'\n", argv[2]);
        return 1;
    }

    if ((int)leb_syscall1(LEB_SYSCALL_NET_DNS, (long)ip) < 0) {
        fprintf(stderr, "lebnet: failed to set DNS server\n");
        return 1;
    }

    printf("DNS server set to %s\n", argv[2]);
    return 0;
}

static int cmd_lebnet_info(void) {
    int ret;

    ret = (int)leb_syscall1(LEB_SYSCALL_NET_GETINFO, 0);
    if (ret < 0) {
        fprintf(stderr, "lebnet: failed to get network info\n");
        return 1;
    }

    return 0;
}

int cmd_lebnet(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "dhcp") == 0)
        return cmd_lebnet_dhcp();
    if (strcmp(argv[1], "dns") == 0)
        return cmd_lebnet_dns(argc, argv);
    if (strcmp(argv[1], "info") == 0)
        return cmd_lebnet_info();
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    fprintf(stderr, "lebnet: unknown command '%s'\n", argv[1]);
    print_usage();
    return 1;
}
