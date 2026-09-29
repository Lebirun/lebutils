#include <stdio.h>
#include <string.h>
#include <lebirun.h>
#include <lebirun/syscall.h>
#include "cu.h"

typedef struct {
    unsigned long long ip;
    unsigned char mac[6];
} __attribute__((packed)) ip_neigh_t;

static void ip_print_usage(void) {
    fprintf(stderr, "Usage: ip [OPTIONS] OBJECT COMMAND\n");
    fprintf(stderr, "Objects:\n");
    fprintf(stderr, "  address | a            show or configure IPv4 addresses\n");
    fprintf(stderr, "  link | l               show link layer info\n");
    fprintf(stderr, "  route | r              show or configure the default route\n");
    fprintf(stderr, "  neigh | n              show ARP neighbours\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  ip a [show] [dev IF]\n");
    fprintf(stderr, "  ip link [show] [dev IF]\n");
    fprintf(stderr, "  ip route [show]\n");
    fprintf(stderr, "  ip neigh [show] [dev IF]\n");
    fprintf(stderr, "  ip addr add ADDR/PREFIX [dev IF]\n");
    fprintf(stderr, "  ip addr del ADDR/PREFIX [dev IF]\n");
    fprintf(stderr, "  ip addr flush [dev IF]\n");
    fprintf(stderr, "  ip route add default via GW [dev IF]\n");
    fprintf(stderr, "  ip route del default\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -brief, -b             terse output\n");
    fprintf(stderr, "  -4                     IPv4 only\n");
    fprintf(stderr, "  -h, --help, help       show this help\n");
}

static void ip_print_ip(unsigned long long ip) {
    printf("%u.%u.%u.%u",
           (unsigned int)((ip >> 24) & 0xff),
           (unsigned int)((ip >> 16) & 0xff),
           (unsigned int)((ip >> 8) & 0xff),
           (unsigned int)(ip & 0xff));
}

static int ip_is_zero6(unsigned char *ip) {
    int i;

    for (i = 0; i < 16; i++) {
        if (ip[i]) return 0;
    }
    return 1;
}

static void ip_print_ip6(unsigned char *ip) {
    int i;

    for (i = 0; i < 16; i += 2) {
        if (i) putchar(':');
        printf("%x", (ip[i] << 8) | ip[i + 1]);
    }
}

static int ip_parse_ip(const char *s, unsigned long *out) {
    unsigned int v[4];
    int i;
    int dots;
    unsigned int cur;

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    v[3] = 0;
    dots = 0;
    cur = 0;
    i = 0;
    if (!s || !*s) return -1;
    while (s[i]) {
        if (s[i] >= '0' && s[i] <= '9') {
            cur = cur * 10 + (unsigned int)(s[i] - '0');
            if (cur > 255) return -1;
        } else if (s[i] == '.') {
            if (dots >= 3) return -1;
            v[dots++] = cur;
            cur = 0;
        } else {
            return -1;
        }
        i++;
    }
    if (dots != 3) return -1;
    v[3] = cur;
    *out = ((unsigned long)v[0] << 24) | ((unsigned long)v[1] << 16) |
           ((unsigned long)v[2] << 8) | (unsigned long)v[3];
    return 0;
}

static unsigned long ip_mask_from_prefix(int prefix) {
    unsigned long m;
    int i;

    if (prefix <= 0) return 0;
    if (prefix >= 32) return 0xfffffffful;
    m = 0;
    for (i = 0; i < prefix; i++) m = (m >> 1) | 0x80000000ul;
    return m;
}

static int ip_prefix_from_mask(unsigned long mask) {
    int n;
    unsigned long bit;

    n = 0;
    bit = 0x80000000ul;
    while (bit) {
        if (mask & bit) n++;
        else break;
        bit >>= 1;
    }
    return n;
}

static int ip_parse_prefix(const char *s, int *out) {
    int v;
    int i;

    if (!s || !*s) return -1;
    v = 0;
    for (i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
        if (v > 32) return -1;
    }
    *out = v;
    return 0;
}

static int ip_parse_addr_mask(const char *s, unsigned long *ip, unsigned long *mask) {
    const char *slash;
    char addr[32];
    char mpart[32];
    int alen;
    int mlen;
    int i;
    int prefix;

    slash = strchr(s, '/');
    if (!slash) {
        if (ip_parse_ip(s, ip) < 0) return -1;
        *mask = 0xfffffffful;
        return 0;
    }
    alen = (int)(slash - s);
    mlen = (int)strlen(slash + 1);
    if (alen <= 0 || alen >= (int)sizeof(addr) || mlen <= 0 || mlen >= (int)sizeof(mpart)) return -1;
    for (i = 0; i < alen; i++) addr[i] = s[i];
    addr[alen] = '\0';
    for (i = 0; i < mlen; i++) mpart[i] = slash[1 + i];
    mpart[mlen] = '\0';
    if (ip_parse_ip(addr, ip) < 0) return -1;
    if (strchr(mpart, '.')) {
        if (ip_parse_ip(mpart, mask) < 0) return -1;
    } else {
        if (ip_parse_prefix(mpart, &prefix) < 0) return -1;
        *mask = ip_mask_from_prefix(prefix);
    }
    return 0;
}

static int ip_getinfo(netinfo_user_t *info) {
    memset(info, 0, sizeof(*info));
    if ((int)leb_syscall1(LEB_SYSCALL_NET_GETINFO, (long)info) < 0) {
        fprintf(stderr, "ip: cannot get interface info\n");
        return -1;
    }
    info->name[15] = '\0';
    return 0;
}

static int ip_is_lo(const char *name) {
    return name[0] == 'l' && name[1] == 'o' && name[2] == '\0';
}

static int ip_dev_ok(const char *name, const char *want) {
    if (!want || !*want) return 1;
    return strcmp(name, want) == 0;
}

static const char *ip_state(const netinfo_user_t *info) {
    return info->link_up ? "UP" : "DOWN";
}

static void ip_print_flags(const netinfo_user_t *info) {
    if (ip_is_lo(info->name)) {
        printf("<%s>", info->link_up ? "LOOPBACK,UP,LOWER_UP" : "LOOPBACK");
    } else {
        printf("<%s>", info->link_up ? "BROADCAST,MULTICAST,UP,LOWER_UP" : "BROADCAST,MULTICAST");
    }
}

static int ip_show_addr(int brief, const char *dev) {
    netinfo_user_t info;

    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    if (brief) {
        printf("%s %s ", info.name, ip_state(&info));
        if (info.ipv4) {
            ip_print_ip(info.ipv4);
            printf("/%d", ip_prefix_from_mask((unsigned long)info.netmask));
        }
        if (!ip_is_zero6(info.ipv6)) {
            putchar(' ');
            ip_print_ip6(info.ipv6);
            printf("/%d", info.ipv6_prefix);
        }
        putchar('\n');
        return 0;
    }
    printf("1: %s: ", info.name);
    ip_print_flags(&info);
    printf(" mtu %llu state %s\n", info.mtu, ip_state(&info));
    if (ip_is_lo(info.name)) {
        printf("    link/loopback %02X:%02X:%02X:%02X:%02X:%02X\n",
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
    } else {
        printf("    link/ether %02X:%02X:%02X:%02X:%02X:%02X\n",
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
    }
    if (info.ipv4) {
        printf("    inet ");
        ip_print_ip(info.ipv4);
        printf("/%d scope global %s\n", ip_prefix_from_mask((unsigned long)info.netmask), info.name);
    }
    if (!ip_is_zero6(info.ipv6)) {
        printf("    inet6 ");
        ip_print_ip6(info.ipv6);
        printf("/%d scope global %s\n", info.ipv6_prefix, info.name);
    }
    return 0;
}

static int ip_show_link(int brief, const char *dev) {
    netinfo_user_t info;

    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    if (brief) {
        printf("%s %s %02X:%02X:%02X:%02X:%02X:%02X\n", info.name, ip_state(&info),
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
        return 0;
    }
    printf("1: %s: ", info.name);
    ip_print_flags(&info);
    printf(" mtu %llu state %s\n", info.mtu, ip_state(&info));
    if (ip_is_lo(info.name)) {
        printf("    link/loopback %02X:%02X:%02X:%02X:%02X:%02X\n",
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
    } else {
        printf("    link/ether %02X:%02X:%02X:%02X:%02X:%02X\n",
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
    }
    return 0;
}

static int ip_show_route(const char *dev) {
    netinfo_user_t info;
    unsigned long net;

    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    if (info.gateway) {
        printf("default via ");
        ip_print_ip(info.gateway);
        printf(" dev %s\n", info.name);
    }
    if (info.ipv4) {
        net = (unsigned long)info.ipv4 & (unsigned long)info.netmask;
        ip_print_ip(net);
        printf("/%d dev %s scope link\n",
               ip_prefix_from_mask((unsigned long)info.netmask), info.name);
    }
    return 0;
}

static int ip_show_neigh(const char *dev) {
    netinfo_user_t info;
    ip_neigh_t entries[32];
    int count;
    int i;

    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    count = 0;
    if ((int)leb_syscall3(LEB_SYSCALL_NET_ARP_GET, (long)entries, (long)&count, 32) < 0) {
        fprintf(stderr, "ip: cannot read neighbour table\n");
        return 1;
    }
    if (count < 0) count = 0;
    if (count > 32) count = 32;
    for (i = 0; i < count; i++) {
        ip_print_ip(entries[i].ip);
        printf(" dev %s lladdr %02X:%02X:%02X:%02X:%02X:%02X REACHABLE\n",
               info.name,
               entries[i].mac[0], entries[i].mac[1], entries[i].mac[2],
               entries[i].mac[3], entries[i].mac[4], entries[i].mac[5]);
    }
    return 0;
}

static const char *ip_opt_dev(int argc, char **argv, int *idx) {
    if (*idx + 1 < argc && strcmp(argv[*idx], "dev") == 0) {
        (*idx)++;
        return argv[(*idx)++];
    }
    return NULL;
}

static int ip_addr_add(int argc, char **argv, int pos) {
    netinfo_user_t info;
    unsigned long ip;
    unsigned long mask;
    const char *dev;
    int ret;

    if (pos >= argc) {
        fprintf(stderr, "Usage: ip addr add ADDR/PREFIX [dev IF]\n");
        return 1;
    }
    if (ip_parse_addr_mask(argv[pos++], &ip, &mask) < 0) {
        fprintf(stderr, "ip: invalid address '%s'\n", argv[pos - 1]);
        return 1;
    }
    dev = ip_opt_dev(argc, argv, &pos);
    if (pos < argc) {
        fprintf(stderr, "Usage: ip addr add ADDR/PREFIX [dev IF]\n");
        return 1;
    }
    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    ret = (int)leb_syscall4(LEB_SYSCALL_NET_IFCONFIG, (long)ip, (long)mask, 0, 3);
    if (ret < 0) {
        fprintf(stderr, "ip: failed to set address\n");
        return 1;
    }
    return 0;
}

static int ip_addr_del(int argc, char **argv, int pos) {
    netinfo_user_t info;
    unsigned long ip;
    unsigned long mask;
    const char *dev;
    int ret;

    if (pos >= argc) {
        fprintf(stderr, "Usage: ip addr del ADDR/PREFIX [dev IF]\n");
        return 1;
    }
    if (ip_parse_addr_mask(argv[pos++], &ip, &mask) < 0) {
        fprintf(stderr, "ip: invalid address '%s'\n", argv[pos - 1]);
        return 1;
    }
    dev = ip_opt_dev(argc, argv, &pos);
    if (pos < argc) {
        fprintf(stderr, "Usage: ip addr del ADDR/PREFIX [dev IF]\n");
        return 1;
    }
    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    (void)mask;
    if ((unsigned long)info.ipv4 != ip) {
        fprintf(stderr, "ip: address not found\n");
        return 1;
    }
    ret = (int)leb_syscall4(LEB_SYSCALL_NET_IFCONFIG, 0, 0, 0, 3);
    if (ret < 0) {
        fprintf(stderr, "ip: failed to delete address\n");
        return 1;
    }
    return 0;
}

static int ip_addr_flush(int argc, char **argv, int pos) {
    netinfo_user_t info;
    const char *dev;
    int ret;

    dev = NULL;
    if (pos < argc) {
        if (strcmp(argv[pos], "dev") == 0) {
            pos++;
            if (pos >= argc) {
                fprintf(stderr, "Usage: ip addr flush [dev IF]\n");
                return 1;
            }
            dev = argv[pos++];
        } else {
            fprintf(stderr, "Usage: ip addr flush [dev IF]\n");
            return 1;
        }
    }
    if (pos < argc) {
        fprintf(stderr, "Usage: ip addr flush [dev IF]\n");
        return 1;
    }
    if (ip_getinfo(&info) < 0) return 1;
    if (!ip_dev_ok(info.name, dev)) {
        fprintf(stderr, "ip: device '%s' not found\n", dev);
        return 1;
    }
    ret = (int)leb_syscall4(LEB_SYSCALL_NET_IFCONFIG, 0, 0, 0, 7);
    if (ret < 0) {
        fprintf(stderr, "ip: failed to flush addresses\n");
        return 1;
    }
    return 0;
}

static int ip_route_add(int argc, char **argv, int pos) {
    netinfo_user_t info;
    unsigned long gw;
    const char *dev;
    int ret;

    if (pos + 2 < argc && strcmp(argv[pos], "default") == 0 && strcmp(argv[pos + 1], "via") == 0) {
        pos += 2;
        if (ip_parse_ip(argv[pos++], &gw) < 0) {
            fprintf(stderr, "ip: invalid gateway '%s'\n", argv[pos - 1]);
            return 1;
        }
        dev = ip_opt_dev(argc, argv, &pos);
        if (pos < argc) {
            fprintf(stderr, "Usage: ip route add default via GW [dev IF]\n");
            return 1;
        }
        if (ip_getinfo(&info) < 0) return 1;
        if (!ip_dev_ok(info.name, dev)) {
            fprintf(stderr, "ip: device '%s' not found\n", dev);
            return 1;
        }
        ret = (int)leb_syscall4(LEB_SYSCALL_NET_IFCONFIG, 0, 0, (long)gw, 4);
        if (ret < 0) {
            fprintf(stderr, "ip: failed to set default route\n");
            return 1;
        }
        return 0;
    }
    fprintf(stderr, "ip: only 'default via GW' routes are supported\n");
    return 1;
}

static int ip_route_del(int argc, char **argv, int pos) {
    int ret;

    if (pos < argc && strcmp(argv[pos], "default") == 0) {
        pos++;
        if (pos < argc) {
            fprintf(stderr, "Usage: ip route del default\n");
            return 1;
        }
        ret = (int)leb_syscall4(LEB_SYSCALL_NET_IFCONFIG, 0, 0, 0, 4);
        if (ret < 0) {
            fprintf(stderr, "ip: failed to delete default route\n");
            return 1;
        }
        return 0;
    }
    fprintf(stderr, "ip: only 'default' routes are supported\n");
    return 1;
}

static int ip_object_addr(int argc, char **argv, int pos, int brief) {
    const char *dev;

    if (pos < argc && (strcmp(argv[pos], "show") == 0 || strcmp(argv[pos], "list") == 0)) pos++;
    if (pos < argc && strcmp(argv[pos], "add") == 0) return ip_addr_add(argc, argv, pos + 1);
    if (pos < argc && (strcmp(argv[pos], "del") == 0 || strcmp(argv[pos], "delete") == 0))
        return ip_addr_del(argc, argv, pos + 1);
    if (pos < argc && strcmp(argv[pos], "flush") == 0) return ip_addr_flush(argc, argv, pos + 1);
    dev = NULL;
    if (pos < argc) {
        if (strcmp(argv[pos], "dev") == 0) {
            pos++;
            if (pos >= argc) {
                fprintf(stderr, "Usage: ip address [show] [dev IF]\n");
                return 1;
            }
            dev = argv[pos++];
        } else if (pos + 1 == argc) {
            dev = argv[pos++];
        } else {
            fprintf(stderr, "Usage: ip address [show] [dev IF]\n");
            return 1;
        }
    }
    if (pos < argc) {
        fprintf(stderr, "Usage: ip address [show] [dev IF]\n");
        return 1;
    }
    return ip_show_addr(brief, dev);
}

static int ip_object_link(int argc, char **argv, int pos, int brief) {
    const char *dev;

    if (pos < argc && (strcmp(argv[pos], "show") == 0 || strcmp(argv[pos], "list") == 0)) pos++;
    if (pos < argc && strcmp(argv[pos], "set") == 0) {
        fprintf(stderr, "ip: link state changes are not supported\n");
        return 1;
    }
    dev = NULL;
    if (pos < argc) {
        if (strcmp(argv[pos], "dev") == 0) {
            pos++;
            if (pos >= argc) {
                fprintf(stderr, "Usage: ip link [show] [dev IF]\n");
                return 1;
            }
            dev = argv[pos++];
        } else if (pos + 1 == argc) {
            dev = argv[pos++];
        } else {
            fprintf(stderr, "Usage: ip link [show] [dev IF]\n");
            return 1;
        }
    }
    if (pos < argc) {
        fprintf(stderr, "Usage: ip link [show] [dev IF]\n");
        return 1;
    }
    return ip_show_link(brief, dev);
}

static int ip_object_route(int argc, char **argv, int pos, int brief) {
    const char *dev;

    (void)brief;
    if (pos < argc && (strcmp(argv[pos], "show") == 0 || strcmp(argv[pos], "list") == 0)) pos++;
    if (pos < argc && strcmp(argv[pos], "add") == 0) return ip_route_add(argc, argv, pos + 1);
    if (pos < argc && (strcmp(argv[pos], "del") == 0 || strcmp(argv[pos], "delete") == 0))
        return ip_route_del(argc, argv, pos + 1);
    dev = NULL;
    if (pos < argc) {
        if (strcmp(argv[pos], "dev") == 0) {
            pos++;
            if (pos >= argc) {
                fprintf(stderr, "Usage: ip route [show] [dev IF]\n");
                return 1;
            }
            dev = argv[pos++];
        } else if (pos + 1 == argc) {
            dev = argv[pos++];
        } else {
            fprintf(stderr, "Usage: ip route [show] [dev IF]\n");
            return 1;
        }
    }
    if (pos < argc) {
        fprintf(stderr, "Usage: ip route [show] [dev IF]\n");
        return 1;
    }
    return ip_show_route(dev);
}

static int ip_object_neigh(int argc, char **argv, int pos, int brief) {
    const char *dev;

    (void)brief;
    if (pos < argc && (strcmp(argv[pos], "show") == 0 || strcmp(argv[pos], "list") == 0)) pos++;
    dev = NULL;
    if (pos < argc) {
        if (strcmp(argv[pos], "dev") == 0) {
            pos++;
            if (pos >= argc) {
                fprintf(stderr, "Usage: ip neigh [show] [dev IF]\n");
                return 1;
            }
            dev = argv[pos++];
        } else if (pos + 1 == argc) {
            dev = argv[pos++];
        } else {
            fprintf(stderr, "Usage: ip neigh [show] [dev IF]\n");
            return 1;
        }
    }
    if (pos < argc) {
        fprintf(stderr, "Usage: ip neigh [show] [dev IF]\n");
        return 1;
    }
    return ip_show_neigh(dev);
}

int cmd_ip(int argc, char **argv) {
    int pos;
    int brief;

    brief = 0;
    pos = 1;
    while (pos < argc && argv[pos][0] == '-') {
        if (strcmp(argv[pos], "-h") == 0 || strcmp(argv[pos], "--help") == 0) {
            ip_print_usage();
            return 0;
        }
        if (strcmp(argv[pos], "-brief") == 0 || strcmp(argv[pos], "-b") == 0) {
            brief = 1;
        } else if (strcmp(argv[pos], "-4") == 0) {
        } else if (strcmp(argv[pos], "-6") == 0) {
            fprintf(stderr, "ip: IPv6 is not supported\n");
            return 1;
        } else {
            fprintf(stderr, "ip: unknown option '%s'\n", argv[pos]);
            ip_print_usage();
            return 1;
        }
        pos++;
    }
    if (pos >= argc) {
        ip_print_usage();
        return 1;
    }
    if (strcmp(argv[pos], "help") == 0) {
        ip_print_usage();
        return 0;
    }
    if (strcmp(argv[pos], "address") == 0 || strcmp(argv[pos], "addr") == 0 ||
        strcmp(argv[pos], "a") == 0)
        return ip_object_addr(argc, argv, pos + 1, brief);
    if (strcmp(argv[pos], "link") == 0 || strcmp(argv[pos], "l") == 0)
        return ip_object_link(argc, argv, pos + 1, brief);
    if (strcmp(argv[pos], "route") == 0 || strcmp(argv[pos], "r") == 0 ||
        strcmp(argv[pos], "ro") == 0)
        return ip_object_route(argc, argv, pos + 1, brief);
    if (strcmp(argv[pos], "neigh") == 0 || strcmp(argv[pos], "neighbour") == 0 ||
        strcmp(argv[pos], "neighbor") == 0 || strcmp(argv[pos], "n") == 0)
        return ip_object_neigh(argc, argv, pos + 1, brief);
    fprintf(stderr, "ip: unknown object '%s'\n", argv[pos]);
    ip_print_usage();
    return 1;
}
