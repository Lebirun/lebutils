#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lebirun.h>
#include <lebirun/syscall.h>
#include "cu.h"

static int dns_parse_ip(const char *s, unsigned long *out) {
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

static int dns_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int dns_parse_ip6(const char *s, unsigned char *out) {
    unsigned int groups[8];
    int ngroups;
    int gap;
    int i;
    int v;
    int digits;
    int g;
    int h;

    for (i = 0; i < 8; i++) groups[i] = 0;
    ngroups = 0;
    gap = -1;
    i = 0;
    if (!s || !*s) return -1;
    if (s[0] == ':') {
        if (s[1] != ':') return -1;
        gap = 0;
        i = 2;
        if (!s[2]) {
            for (g = 0; g < 16; g++) out[g] = 0;
            return 0;
        }
    }
    while (1) {
        v = 0;
        digits = 0;
        while ((h = dns_hex_val(s[i])) >= 0) {
            v = v * 16 + (unsigned int)h;
            digits++;
            if (digits > 4) return -1;
            i++;
        }
        if (digits == 0) return -1;
        if (ngroups >= 8) return -1;
        groups[ngroups++] = (unsigned int)v;
        if (!s[i]) break;
        if (s[i] != ':') return -1;
        if (s[i + 1] == ':') {
            if (gap >= 0) return -1;
            gap = ngroups;
            i += 2;
            if (!s[i]) break;
        } else {
            i++;
            if (!s[i]) return -1;
        }
    }
    if (gap < 0) {
        if (ngroups != 8) return -1;
    } else {
        for (g = ngroups - 1; g >= gap; g--)
            groups[g + (8 - ngroups)] = groups[g];
        for (g = gap; g < gap + (8 - ngroups); g++)
            groups[g] = 0;
    }
    for (g = 0; g < 8; g++) {
        out[g * 2] = (unsigned char)(groups[g] >> 8);
        out[g * 2 + 1] = (unsigned char)(groups[g] & 0xff);
    }
    return 0;
}

static void dns_print_ip(unsigned long long ip) {
    printf("%u.%u.%u.%u",
           (unsigned int)((ip >> 24) & 0xff),
           (unsigned int)((ip >> 16) & 0xff),
           (unsigned int)((ip >> 8) & 0xff),
           (unsigned int)(ip & 0xff));
}

static void dns_print_ip6(unsigned char *ip) {
    int i;

    for (i = 0; i < 16; i += 2) {
        if (i) putchar(':');
        printf("%x", (ip[i] << 8) | ip[i + 1]);
    }
}

static int dns_show(void) {
    unsigned int servers[16];
    unsigned char servers6[8][16];
    int count;
    int count6;
    int i;

    count = 0;
    count6 = 0;
    if ((int)leb_syscall3(LEB_SYSCALL_NET_DNS_GET, (long)servers, (long)&count, 16) < 0) {
        fprintf(stderr, "dns: cannot get DNS servers\n");
        return 1;
    }
    if ((int)leb_syscall3(LEB_SYSCALL_NET_DNS6_GET, (long)servers6, (long)&count6, 8) < 0) {
        fprintf(stderr, "dns: cannot get DNS servers\n");
        return 1;
    }
    if (count < 0) count = 0;
    if (count > 16) count = 16;
    if (count6 < 0) count6 = 0;
    if (count6 > 8) count6 = 8;
    if (count == 0 && count6 == 0) {
        puts("(none)");
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (count6 == 0) printf("DNS: ");
        else printf("DNS (IPv4): ");
        dns_print_ip(servers[i]);
        putchar('\n');
    }
    for (i = 0; i < count6; i++) {
        printf("DNS (IPv6): ");
        dns_print_ip6(servers6[i]);
        putchar('\n');
    }
    return 0;
}

int cmd_dns(int argc, char **argv) {
    unsigned int *addrs4;
    unsigned char (*addrs6)[16];
    unsigned long ip;
    unsigned char ip6[16];
    int n4;
    int n6;
    int i;
    int z;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: dns [IP...]");
        puts("Show or set DNS server addresses (IPv4 and IPv6).");
        return 0;
    }

    if (argc == 1) return dns_show();

    addrs4 = (unsigned int *)malloc((unsigned int)(argc - 1) * sizeof(*addrs4));
    addrs6 = malloc((unsigned int)(argc - 1) * sizeof(*addrs6));
    if (!addrs4 || !addrs6) {
        fprintf(stderr, "dns: out of memory\n");
        free(addrs4);
        free(addrs6);
        return 1;
    }
    n4 = 0;
    n6 = 0;
    for (i = 1; i < argc; i++) {
        if (dns_parse_ip(argv[i], &ip) == 0 && ip != 0) {
            addrs4[n4++] = (unsigned int)ip;
        } else if (dns_parse_ip6(argv[i], ip6) == 0) {
            for (z = 0; z < 16 && !ip6[z]; z++) ;
            if (z == 16) {
                fprintf(stderr, "dns: invalid IP address '%s'\n", argv[i]);
                free(addrs4);
                free(addrs6);
                return 1;
            }
            memcpy(addrs6[n6++], ip6, 16);
        } else {
            fprintf(stderr, "dns: invalid IP address '%s'\n", argv[i]);
            free(addrs4);
            free(addrs6);
            return 1;
        }
    }

    if (n4 > 0 && (int)leb_syscall3(LEB_SYSCALL_NET_DNS_SET, (long)addrs4, 0, (long)n4) < 0) {
        fprintf(stderr, "dns: failed to set DNS servers\n");
        free(addrs4);
        free(addrs6);
        return 1;
    }
    if (n6 > 0 && (int)leb_syscall3(LEB_SYSCALL_NET_DNS6_SET, (long)addrs6, 0, (long)n6) < 0) {
        fprintf(stderr, "dns: failed to set DNS servers\n");
        free(addrs4);
        free(addrs6);
        return 1;
    }
    free(addrs4);
    free(addrs6);
    return 0;
}
