#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include "cu.h"

typedef struct {
    char name[16];
    unsigned char mac[6];
    unsigned char _pad1[2];
    unsigned long long ipv4;
    unsigned long long netmask;
    unsigned long long gateway;
    unsigned long long dns;
    unsigned long long mtu;
    unsigned char link_up;
    unsigned char dhcp_configured;
    unsigned char _pad2[2];
} __attribute__((packed)) netinfo_user_t;

static void print_usage(void) {
    fprintf(stderr, "Usage: lebnet <command> [options]\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  dhcp              Obtain IP address via DHCP\n");
    fprintf(stderr, "  static <ip> <mask> [gateway]\n");
    fprintf(stderr, "                    Set a static IPv4 address\n");
    fprintf(stderr, "  dns <ip>          Set DNS server address\n");
    fprintf(stderr, "  info              Show network interface info\n");
    fprintf(stderr, "  arp               Show ARP cache\n");
    fprintf(stderr, "  help              Show this help\n");
}

static void print_ip_value(unsigned long long ip) {
    printf("%u.%u.%u.%u",
           (unsigned int)((ip >> 24) & 0xff),
           (unsigned int)((ip >> 16) & 0xff),
           (unsigned int)((ip >> 8) & 0xff),
           (unsigned int)(ip & 0xff));
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

static int cmd_lebnet_static(int argc, char **argv) {
    char ip_text[32];
    char netmask_text[32];
    const char *slash;
    unsigned long ip;
    unsigned long netmask;
    unsigned long gateway;
    int ip_len;
    int mask_len;
    int i;
    int ret;

    if (argc < 3) {
        fprintf(stderr, "Usage: lebnet static <ip> <netmask> [gateway]\n");
        fprintf(stderr, "       lebnet static <ip>/<netmask> [gateway]\n");
        return 1;
    }
    gateway = 0;
    slash = strchr(argv[2], '/');
    if (slash) {
        ip_len = (int)(slash - argv[2]);
        mask_len = (int)strlen(slash + 1);
        if (ip_len <= 0 || ip_len >= (int)sizeof(ip_text) || mask_len <= 0 || mask_len >= (int)sizeof(netmask_text)) {
            fprintf(stderr, "lebnet: invalid static address '%s'\n", argv[2]);
            return 1;
        }
        for (i = 0; i < ip_len; i++) ip_text[i] = argv[2][i];
        ip_text[ip_len] = '\0';
        for (i = 0; i < mask_len; i++) netmask_text[i] = slash[1 + i];
        netmask_text[mask_len] = '\0';
        if (parse_ip(ip_text, &ip) < 0) {
            fprintf(stderr, "lebnet: invalid IP address '%s'\n", ip_text);
            return 1;
        }
        if (parse_ip(netmask_text, &netmask) < 0) {
            fprintf(stderr, "lebnet: invalid netmask '%s'\n", netmask_text);
            return 1;
        }
        if (argc >= 4 && parse_ip(argv[3], &gateway) < 0) {
            fprintf(stderr, "lebnet: invalid gateway '%s'\n", argv[3]);
            return 1;
        }
    } else {
        if (argc < 4) {
            fprintf(stderr, "Usage: lebnet static <ip> <netmask> [gateway]\n");
            fprintf(stderr, "       lebnet static <ip>/<netmask> [gateway]\n");
            return 1;
        }
        if (parse_ip(argv[2], &ip) < 0) {
            fprintf(stderr, "lebnet: invalid IP address '%s'\n", argv[2]);
            return 1;
        }
        if (parse_ip(argv[3], &netmask) < 0) {
            fprintf(stderr, "lebnet: invalid netmask '%s'\n", argv[3]);
            return 1;
        }
        if (argc >= 5 && parse_ip(argv[4], &gateway) < 0) {
            fprintf(stderr, "lebnet: invalid gateway '%s'\n", argv[4]);
            return 1;
        }
    }
    ret = (int)leb_syscall3(LEB_SYSCALL_NET_IFCONFIG, (long)ip, (long)netmask, (long)gateway);
    if (ret < 0) {
        fprintf(stderr, "lebnet: failed to set static address\n");
        return 1;
    }
    printf("Static IPv4 configured\n");
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
    netinfo_user_t info;
    int ret;

    memset(&info, 0, sizeof(info));
    ret = (int)leb_syscall1(LEB_SYSCALL_NET_GETINFO, (long)&info);
    if (ret < 0) {
        fprintf(stderr, "lebnet: failed to get network info\n");
        return 1;
    }

    info.name[15] = '\0';
    printf("Interface: %s\n", info.name);
    printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           info.mac[0], info.mac[1], info.mac[2],
           info.mac[3], info.mac[4], info.mac[5]);
    printf("  IPv4: ");
    print_ip_value(info.ipv4);
    putchar('\n');
    printf("  Netmask: ");
    print_ip_value(info.netmask);
    putchar('\n');
    printf("  Gateway: ");
    print_ip_value(info.gateway);
    putchar('\n');
    printf("  DNS: ");
    print_ip_value(info.dns);
    putchar('\n');
    printf("  MTU: %llu\n", info.mtu);
    printf("  Link: %s\n", info.link_up ? "UP" : "DOWN");
    printf("  DHCP: %s\n", info.dhcp_configured ? "Yes" : "No");
    return 0;
}

static int cmd_lebnet_arp(void) {
    int ret;

    ret = (int)leb_syscall1(LEB_SYSCALL_NET_ARP, 0);
    if (ret < 0) {
        fprintf(stderr, "lebnet: failed to show ARP cache\n");
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
    if (strcmp(argv[1], "static") == 0)
        return cmd_lebnet_static(argc, argv);
    if (strcmp(argv[1], "dns") == 0)
        return cmd_lebnet_dns(argc, argv);
    if (strcmp(argv[1], "info") == 0)
        return cmd_lebnet_info();
    if (strcmp(argv[1], "arp") == 0)
        return cmd_lebnet_arp();
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    fprintf(stderr, "lebnet: unknown command '%s'\n", argv[1]);
    print_usage();
    return 1;
}
