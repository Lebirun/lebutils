#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <lebirun.h>
#include "../ipv67_crypto.h"

#define IPV67_CMD_INIT         0
#define IPV67_CMD_SET_ADDR     1
#define IPV67_CMD_GET_ADDR     2
#define IPV67_CMD_ADD_PEER     3
#define IPV67_CMD_REMOVE_PEER  4
#define IPV67_CMD_GET_PEERS    5
#define IPV67_CMD_PEER_COUNT   6
#define IPV67_CMD_PING         8
#define IPV67_CMD_SET_PORT     9
#define IPV67_CMD_GET_PORT     10
#define IPV67_CMD_ADDR_PARSE   11
#define IPV67_CMD_ADDR_FORMAT  12
#define IPV67_CMD_ADD_PEER6    13
#define IPV67_CMD_ADD_PEER_HOST 14
#define IPV67_CMD_ADD_ENDPOINT 15
#define IPV67_CMD_PROBE_PEERS  16
#define IPV67_CMD_GET_ROUTES   17
#define IPV67_CMD_ROUTE_COUNT  18

#define IPV67_PEER_IPV4        4
#define IPV67_PEER_IPV6        6

#define IPV67_CONFIG_FILE      "/etc/ipv67/ipv67.conf"
#define IPV67_MAX_CONFIG_PEERS 32
#define IPV67_PORT_DEFAULT     6767
#define IPV67_ERR_TIMEOUT      -6

#define IPV67_ZONE_MAX    6
#define IPV67_DOMAIN_MAX  32
#define IPV67_NODE_MAX    6
#define IPV67_ADDR_STR_MAX  (IPV67_ZONE_MAX + 1 + IPV67_ZONE_MAX + 1 + IPV67_DOMAIN_MAX + 1 + IPV67_NODE_MAX + 1 + IPV67_NODE_MAX + 1)

typedef struct {
    int      cmd;
    unsigned long long arg1;
    unsigned long long arg2;
    unsigned long long arg3;
    unsigned long long arg4;
} __attribute__((packed)) ipv67_syscall_req_t;

typedef struct {
    unsigned int ipv4;
    unsigned char ipv6[16];
    unsigned short port;
    unsigned char family;
    char addr_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_peer_user_t;

typedef struct {
    unsigned int next_hop_ipv4;
    unsigned char next_hop_ipv6[16];
    unsigned short next_hop_port;
    unsigned char next_hop_family;
    unsigned char hops;
    char dest_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_route_user_t;

typedef struct {
    char zone1[IPV67_ZONE_MAX + 1];
    char zone2[IPV67_ZONE_MAX + 1];
    char domain[IPV67_DOMAIN_MAX + 1];
    char node1[IPV67_NODE_MAX + 1];
    char node2[IPV67_NODE_MAX + 1];
} ipv67_addr_t;

typedef struct {
    int family;
    unsigned int ipv4;
    unsigned char ipv6[16];
    int port;
    char host[256];
} ipv67_endpoint_t;

typedef struct {
    char keyfile[256];
    char address[IPV67_ADDR_STR_MAX];
    int port;
    int peer_count;
    char peer_endpoint[IPV67_MAX_CONFIG_PEERS][256];
} ipv67_config_t;

static unsigned short ipv67_instance_port = IPV67_PORT_DEFAULT;

static long ipv67_syscall(ipv67_syscall_req_t *req) {
    if (req && req->arg4 == 0) req->arg4 = ipv67_instance_port;
    return leb_syscall1(LEB_SYSCALL_IPV67, (long)req);
}

static void print_ipv4(unsigned int ip) {
    printf("%u.%u.%u.%u",
        (ip >> 24) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 8) & 0xff,
        ip & 0xff);
}

static void print_ipv6(const unsigned char *ip) {
    int i;
    unsigned int part;

    for (i = 0; i < 8; i++) {
        part = ((unsigned int)ip[i * 2] << 8) | (unsigned int)ip[i * 2 + 1];
        if (i > 0) putchar(':');
        printf("%x", part);
    }
}

static int parse_ipv67_addr(const char *text, ipv67_addr_t *addr) {
    ipv67_syscall_req_t req;
    long ret;

    memset(&req, 0, sizeof(req));
    memset(addr, 0, sizeof(*addr));
    req.cmd = IPV67_CMD_ADDR_PARSE;
    req.arg1 = (unsigned long long)(long)text;
    req.arg2 = (unsigned long long)(long)addr;
    ret = ipv67_syscall(&req);
    return (int)ret;
}

static int format_ipv67_addr(const ipv67_addr_t *addr, char *text) {
    ipv67_syscall_req_t req;
    long ret;

    memset(&req, 0, sizeof(req));
    memset(text, 0, IPV67_ADDR_STR_MAX);
    req.cmd = IPV67_CMD_ADDR_FORMAT;
    req.arg1 = (unsigned long long)(long)addr;
    req.arg2 = (unsigned long long)(long)text;
    ret = ipv67_syscall(&req);
    text[IPV67_ADDR_STR_MAX - 1] = '\0';
    return (int)ret;
}

static void print_route_hint(void) {
    puts("No route to address. Add a peer first, for example:");
    puts("  ipv67cli addpeer <peer-ipv4>:<peer-port>");
    puts("  ipv67cli addpeer [<peer-ipv6>]:<peer-port>");
    puts("  ipv67cli addpeer <domain>:<peer-port>");
    puts("Then keep ipv67d running so it can probe the endpoint and learn its IPv67 address.");
}

static void show_usage(void) {
    puts("Usage: ipv67cli [-c <config.conf>] [-k <keyfile>] <command> [args]");
    puts("");
    puts("Commands:");
    puts("  reset              Reset and load this IPv67 config");
    puts("  addr               Show current IPv67 address");
    puts("  setaddr <addr>     Set IPv67 address");
    puts("  addpeer <endpoint> Add a persistent peer endpoint");
    puts("  addpeer <host> <port>");
    puts("                     Add a persistent peer endpoint");
    puts("  rmpeer <endpoint|ipv67addr>");
    puts("                     Remove a configured endpoint or runtime peer");
    puts("  peers              List known peers");
    puts("  routes             List known routes");
    puts("  probe [endpoint]   Probe configured peers or one endpoint now");
    puts("  ping [self|ipv67addr]");
    puts("                     Ping an IPv67 address");
    puts("  status             Show IPv67 runtime status");
    puts("  config             Show loaded IPv67 config");
    puts("  setport <port>     Set IPv67 UDP port");
    puts("  port               Show current IPv67 UDP port");
    puts("  keygen             Generate a new IPv67 identity key");
    puts("  pubaddr            Show address derived from stored identity key");
    puts("  mine <regex>       Mine a vanity address matching regex");
    puts("");
    puts("Config:");
    puts("  Default config: " IPV67_CONFIG_FILE);
    puts("  Peer lines are kept as: peer <endpoint>");
}

static int parse_ipv4(const char *s, unsigned int *out) {
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    int octet;
    int dots;
    int i;

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
            if (dots == 0) a = (unsigned int)octet;
            else if (dots == 1) b = (unsigned int)octet;
            else if (dots == 2) c = (unsigned int)octet;
            octet = 0;
            dots++;
        } else {
            return -1;
        }
    }
    if (dots != 3) return -1;
    d = (unsigned int)octet;
    if (a > 255 || b > 255 || c > 255 || d > 255) return -1;
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

static int copy_text(char *dst, int dst_len, const char *src, int len) {
    int i;

    if (!dst || !src || dst_len <= 0 || len < 0 || len >= dst_len) return -1;
    for (i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = '\0';
    return 0;
}

static int is_domain_name(const char *s) {
    int i;
    int saw_alpha;

    if (!s || !s[0]) return 0;
    saw_alpha = 0;
    for (i = 0; s[i]; i++) {
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z')) saw_alpha = 1;
        else if ((s[i] >= '0' && s[i] <= '9') || s[i] == '-' || s[i] == '.') {
        } else {
            return 0;
        }
    }
    return saw_alpha;
}

static char *skip_spaces(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static char *next_token(char **p_in) {
    char *p;
    char *start;

    p = skip_spaces(*p_in);
    if (*p == '\0' || *p == '#') {
        *p_in = p;
        return NULL;
    }
    start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    if (*p) {
        *p = '\0';
        p++;
    }
    *p_in = p;
    return start;
}

static void config_init(ipv67_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->keyfile, IPV67_IDENTITY_KEY);
    cfg->port = IPV67_PORT_DEFAULT;
}

static void ensure_config_dir(void) {
    vfs_mkdir("/etc/ipv67", 0700);
}

static int load_config(const char *path, ipv67_config_t *cfg) {
    FILE *f;
    char line[512];
    char *p;
    char *key;
    char *val;

    config_init(cfg);
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        p = line;
        key = next_token(&p);
        if (!key) continue;
        val = next_token(&p);
        if (!val) continue;
        if (strcmp(key, "identity") == 0 || strcmp(key, "keyfile") == 0) {
            strncpy(cfg->keyfile, val, sizeof(cfg->keyfile) - 1);
            cfg->keyfile[sizeof(cfg->keyfile) - 1] = '\0';
        } else if (strcmp(key, "address") == 0) {
            strncpy(cfg->address, val, sizeof(cfg->address) - 1);
            cfg->address[sizeof(cfg->address) - 1] = '\0';
        } else if (strcmp(key, "port") == 0) {
            cfg->port = atoi(val);
            if (cfg->port <= 0 || cfg->port > 65535) cfg->port = IPV67_PORT_DEFAULT;
        } else if (strcmp(key, "peer") == 0 && cfg->peer_count < IPV67_MAX_CONFIG_PEERS) {
            strncpy(cfg->peer_endpoint[cfg->peer_count], val, sizeof(cfg->peer_endpoint[0]) - 1);
            cfg->peer_endpoint[cfg->peer_count][sizeof(cfg->peer_endpoint[0]) - 1] = '\0';
            cfg->peer_count++;
        }
    }
    fclose(f);
    return 0;
}

static int save_config(const char *path, const ipv67_config_t *cfg) {
    FILE *f;
    int i;

    ensure_config_dir();
    f = fopen(path, "w");
    if (!f) return -1;
    if (cfg->keyfile[0]) fprintf(f, "identity %s\n", cfg->keyfile);
    if (cfg->address[0]) fprintf(f, "address %s\n", cfg->address);
    if (cfg->port > 0) fprintf(f, "port %d\n", cfg->port);
    for (i = 0; i < cfg->peer_count; i++) {
        if (cfg->peer_endpoint[i][0]) fprintf(f, "peer %s\n", cfg->peer_endpoint[i]);
    }
    fclose(f);
    return 0;
}

static int config_find_peer(const ipv67_config_t *cfg, const char *endpoint) {
    int i;

    for (i = 0; i < cfg->peer_count; i++) {
        if (strcmp(cfg->peer_endpoint[i], endpoint) == 0) return i;
    }
    return -1;
}

static int config_add_peer(ipv67_config_t *cfg, const char *endpoint) {
    int idx;

    idx = config_find_peer(cfg, endpoint);
    if (idx >= 0) return 0;
    if (cfg->peer_count >= IPV67_MAX_CONFIG_PEERS) return -1;
    strncpy(cfg->peer_endpoint[cfg->peer_count], endpoint, sizeof(cfg->peer_endpoint[0]) - 1);
    cfg->peer_endpoint[cfg->peer_count][sizeof(cfg->peer_endpoint[0]) - 1] = '\0';
    cfg->peer_count++;
    return 1;
}

static int config_remove_peer(ipv67_config_t *cfg, const char *endpoint) {
    int idx;
    int i;

    idx = config_find_peer(cfg, endpoint);
    if (idx < 0) return -1;
    for (i = idx; i + 1 < cfg->peer_count; i++) {
        memcpy(cfg->peer_endpoint[i], cfg->peer_endpoint[i + 1], sizeof(cfg->peer_endpoint[i]));
    }
    memset(cfg->peer_endpoint[cfg->peer_count - 1], 0, sizeof(cfg->peer_endpoint[0]));
    cfg->peer_count--;
    return 0;
}

static int parse_peer_endpoint(const char *s, ipv67_endpoint_t *endpoint) {
    char host[256];
    char port_buf[8];
    int i;
    int port_len;
    int colon_count;
    int colon_pos;
    int close_pos;
    int port_val;

    if (!s || !endpoint) return -1;

    memset(endpoint, 0, sizeof(*endpoint));
    port_len = 0;
    colon_count = 0;
    colon_pos = -1;
    close_pos = -1;

    if (s[0] == '[') {
        for (i = 1; s[i]; i++) {
            if (s[i] == ']') {
                close_pos = i;
                break;
            }
        }
        if (close_pos <= 1 || s[close_pos + 1] != ':') return -1;
        if (copy_text(host, sizeof(host), s + 1, close_pos - 1) < 0) return -1;
        for (i = close_pos + 2; s[i]; i++) {
            if (port_len >= (int)sizeof(port_buf) - 1) return -1;
            if (s[i] < '0' || s[i] > '9') return -1;
            port_buf[port_len++] = s[i];
        }
        port_buf[port_len] = '\0';
        port_val = atoi(port_buf);
        if (port_val <= 0 || port_val > 65535) return -1;
        if (inet_pton(AF_INET6, host, endpoint->ipv6) != 1) return -1;
        endpoint->family = IPV67_PEER_IPV6;
        endpoint->port = port_val;
        return 0;
    }

    for (i = 0; s[i]; i++) {
        if (s[i] == ':') {
            colon_count++;
            colon_pos = i;
        }
    }
    if (colon_count != 1 || colon_pos <= 0 || s[colon_pos + 1] == '\0') return -1;
    if (copy_text(host, sizeof(host), s, colon_pos) < 0) return -1;

    for (i = colon_pos + 1; s[i]; i++) {
        if (port_len >= (int)sizeof(port_buf) - 1) return -1;
        if (s[i] < '0' || s[i] > '9') return -1;
        port_buf[port_len++] = s[i];
    }
    port_buf[port_len] = '\0';
    port_val = atoi(port_buf);
    if (port_val <= 0 || port_val > 65535) return -1;

    if (parse_ipv4(host, &endpoint->ipv4) == 0) {
        endpoint->family = IPV67_PEER_IPV4;
        endpoint->port = port_val;
        return 0;
    }

    if (is_domain_name(host)) {
        copy_text(endpoint->host, sizeof(endpoint->host), host, (int)strlen(host));
        endpoint->family = 0;
        endpoint->port = port_val;
        return 0;
    }

    return -1;
}

static int add_peer_to_kernel(const char *endpoint_text) {
    ipv67_endpoint_t endpoint;
    ipv67_syscall_req_t req;
    long ret;

    if (parse_peer_endpoint(endpoint_text, &endpoint) < 0) return -1;
    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_ADD_ENDPOINT;
    req.arg2 = (unsigned long long)(unsigned short)endpoint.port;
    if (endpoint.family == IPV67_PEER_IPV4) {
        req.arg1 = (unsigned long long)endpoint.ipv4;
        req.arg3 = IPV67_PEER_IPV4;
    } else if (endpoint.family == IPV67_PEER_IPV6) {
        req.arg1 = (unsigned long long)(long)endpoint.ipv6;
        req.arg3 = IPV67_PEER_IPV6;
    } else {
        req.arg1 = (unsigned long long)(long)endpoint.host;
        req.arg3 = 0;
    }
    ret = ipv67_syscall(&req);
    return (int)ret;
}

static int load_runtime_config(const ipv67_config_t *cfg) {
    ipv67_syscall_req_t req;
    ipv67_addr_t addr;
    long ret;
    int i;

    if (!cfg) return -1;
    if (cfg->port > 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_PORT;
        req.arg1 = (unsigned long long)(unsigned int)cfg->port;
        ret = ipv67_syscall(&req);
        if (ret < 0) return (int)ret;
    }
    if (cfg->address[0]) {
        ret = parse_ipv67_addr(cfg->address, &addr);
        if (ret < 0) return (int)ret;
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) return (int)ret;
    }
    for (i = 0; i < cfg->peer_count; i++) {
        ret = add_peer_to_kernel(cfg->peer_endpoint[i]);
        if (ret < 0) return (int)ret;
    }
    return 0;
}

int cmd_ipv67cli(int argc, char **argv) {
    ipv67_syscall_req_t req;
    char addr_buf[IPV67_ADDR_STR_MAX];
    ipv67_peer_user_t peers[64];
    ipv67_route_user_t routes[64];
    long ret;
    int count;
    int i;
    int port_val;
    const char *keyfile;
    ipv67_addr_t addr;
    uint8_t seed[IPV67_SEED_SIZE];
    char pubaddr[IPV67_ADDR_STR_MAX];
    long gr;
    unsigned long long attempts;
    const char *pattern;
    int match;
    const char *pa;
    const char *aa;
    const char *aa2;
    int endpoint_ret;
    ipv67_endpoint_t endpoint;
    const char *config_path;
    ipv67_config_t cfg;
    char endpoint_text[300];
    ipv67_syscall_req_t probe_req;
    int config_changed;

    if (argc < 2) {
        show_usage();
        return 1;
    }

    config_path = IPV67_CONFIG_FILE;
    keyfile = IPV67_IDENTITY_KEY;
    while (argc >= 4 && argv[1][0] == '-') {
        if (strcmp(argv[1], "-k") == 0) {
            keyfile = argv[2];
            argc -= 2;
            argv += 2;
        } else if (strcmp(argv[1], "-c") == 0) {
            config_path = argv[2];
            argc -= 2;
            argv += 2;
        } else {
            break;
        }
    }
    if (load_config(config_path, &cfg) == 0) {
        if (strcmp(keyfile, IPV67_IDENTITY_KEY) == 0 && cfg.keyfile[0]) keyfile = cfg.keyfile;
    }
    ipv67_instance_port = (unsigned short)cfg.port;

    if (strcmp(argv[1], "reset") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_INIT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: reset failed (%ld)\n", ret);
            return 1;
        }
        ret = load_runtime_config(&cfg);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: config load failed (%ld)\n", ret);
            return 1;
        }
        puts("IPv67 config loaded into kernel.");
        return 0;
    }

    if (strcmp(argv[1], "addr") == 0) {
        memset(&req, 0, sizeof(req));
        memset(&addr, 0, sizeof(addr));
        memset(addr_buf, 0, sizeof(addr_buf));
        req.cmd = IPV67_CMD_GET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get addr failed (%ld)\n", ret);
            return 1;
        }
        ret = format_ipv67_addr(&addr, addr_buf);
        if (ret < 0 || addr_buf[0] == '.') {
            puts("IPv67 address: not set");
            return 0;
        }
        printf("IPv67 address: %s\n", addr_buf);
        return 0;
    }

    if (strcmp(argv[1], "setaddr") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: setaddr requires an address\n", stderr);
            return 1;
        }
        ret = parse_ipv67_addr(argv[2], &addr);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: invalid IPv67 address (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: setaddr failed (%ld)\n", ret);
            return 1;
        }
        strncpy(cfg.address, argv[2], sizeof(cfg.address) - 1);
        cfg.address[sizeof(cfg.address) - 1] = '\0';
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("Address set to: %s\n", argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "addpeer") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: addpeer requires: <endpoint>\n", stderr);
            return 1;
        }
        if (argc >= 5) {
            fputs("ipv67cli: addpeer no longer accepts a peer IPv67 address; use addpeer <endpoint>\n", stderr);
            return 1;
        }
        if (argc == 4) {
            port_val = atoi(argv[3]);
            if (port_val > 0 && port_val <= 65535) {
                snprintf(endpoint_text, sizeof(endpoint_text), "%s:%d", argv[2], port_val);
                ret = add_peer_to_kernel(endpoint_text);
                if (ret < 0) {
                    fprintf(stderr, "ipv67cli: addpeer failed (%ld)\n", ret);
                    return 1;
                }
                config_changed = config_add_peer(&cfg, endpoint_text);
                if (config_changed < 0) {
                    fputs("ipv67cli: too many configured peers\n", stderr);
                    return 1;
                }
                if (save_config(config_path, &cfg) < 0) {
                    fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                    return 1;
                }
                printf("Peer endpoint added: %s\n", endpoint_text);
                puts("ipv67d will probe it and learn its IPv67 address when the remote node replies.");
                return 0;
            }
        }
        endpoint_ret = parse_peer_endpoint(argv[2], &endpoint);
        if (endpoint_ret < 0) {
            fputs("ipv67cli: invalid peer endpoint\n", stderr);
            return 1;
        }
        if (argc >= 4) {
            fputs("ipv67cli: addpeer no longer accepts a peer IPv67 address; use addpeer <endpoint>\n", stderr);
            return 1;
        }
        ret = add_peer_to_kernel(argv[2]);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: addpeer failed (%ld)\n", ret);
            return 1;
        }
        config_changed = config_add_peer(&cfg, argv[2]);
        if (config_changed < 0) {
            fputs("ipv67cli: too many configured peers\n", stderr);
            return 1;
        }
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("Peer endpoint added: %s\n", argv[2]);
        puts("ipv67d will probe it and learn its IPv67 address when the remote node replies.");
        return 0;
    }

    if (strcmp(argv[1], "rmpeer") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: rmpeer requires an endpoint or address\n", stderr);
            return 1;
        }
        if (parse_peer_endpoint(argv[2], &endpoint) == 0) {
            if (config_remove_peer(&cfg, argv[2]) < 0) {
                fprintf(stderr, "ipv67cli: peer endpoint not configured: %s\n", argv[2]);
                return 1;
            }
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            printf("Peer endpoint removed: %s\n", argv[2]);
            puts("Restart ipv67d or run ipv67cli reset to reload runtime peers.");
            return 0;
        }
        ret = parse_ipv67_addr(argv[2], &addr);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: invalid endpoint or IPv67 address (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_REMOVE_PEER;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: rmpeer failed (%ld)\n", ret);
            return 1;
        }
        printf("Peer removed: %s\n", argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "config") == 0) {
        printf("Config: %s\n", config_path);
        printf("Identity: %s\n", cfg.keyfile[0] ? cfg.keyfile : "(none)");
        printf("Address: %s\n", cfg.address[0] ? cfg.address : "(not set)");
        printf("Port: %d\n", cfg.port);
        if (cfg.peer_count == 0) {
            puts("Peers: none");
        } else {
            printf("Peers (%d):\n", cfg.peer_count);
            for (i = 0; i < cfg.peer_count; i++) {
                printf("  %s\n", cfg.peer_endpoint[i]);
            }
        }
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get addr failed (%ld)\n", ret);
            return 1;
        }
        ret = format_ipv67_addr(&addr, addr_buf);
        if (ret < 0 || addr_buf[0] == '.') printf("Address: not set\n");
        else printf("Address: %s\n", addr_buf);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PORT;
        ret = ipv67_syscall(&req);
        if (ret < 0) printf("Port: unknown\n");
        else printf("Port: %ld\n", ret);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PEER_COUNT;
        ret = ipv67_syscall(&req);
        if (ret < 0) printf("Runtime peers: unknown\n");
        else printf("Runtime peers: %ld\n", ret);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_ROUTE_COUNT;
        ret = ipv67_syscall(&req);
        if (ret < 0) printf("Runtime routes: unknown\n");
        else printf("Runtime routes: %ld\n", ret);
        printf("Configured peers: %d\n", cfg.peer_count);
        return 0;
    }

    if (strcmp(argv[1], "peers") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PEER_COUNT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: peer_count failed (%ld)\n", ret);
            return 1;
        }
        count = (int)ret;
        if (count == 0) {
            if (cfg.peer_count == 0) {
                puts("No known peers.");
            } else {
                printf("Configured peers (%d):\n", cfg.peer_count);
                for (i = 0; i < cfg.peer_count; i++) {
                    printf("  unresolved  %s\n", cfg.peer_endpoint[i]);
                }
            }
            return 0;
        }
        printf("Known peers (%d):\n", count);
        if (count > 64) count = 64;
        memset(peers, 0, sizeof(peers));
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PEERS;
        req.arg1 = (unsigned long long)(long)peers;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get_peers failed (%ld)\n", ret);
            return 1;
        }
        for (i = 0; i < (int)ret; i++) {
            peers[i].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
            if (strcmp(peers[i].addr_str, "....") == 0 || peers[i].addr_str[0] == '.')
                printf("  unresolved  ");
            else
                printf("  %s  ", peers[i].addr_str);
            if (peers[i].family == IPV67_PEER_IPV6) {
                putchar('[');
                print_ipv6(peers[i].ipv6);
                printf("]:%u\n", (unsigned int)peers[i].port);
            } else {
                print_ipv4(peers[i].ipv4);
                printf(":%u\n", (unsigned int)peers[i].port);
            }
        }
        return 0;
    }

    if (strcmp(argv[1], "routes") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_ROUTE_COUNT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: route_count failed (%ld)\n", ret);
            return 1;
        }
        count = (int)ret;
        if (count == 0) {
            puts("No known routes.");
            return 0;
        }
        printf("Known routes (%d):\n", count);
        if (count > 64) count = 64;
        memset(routes, 0, sizeof(routes));
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_ROUTES;
        req.arg1 = (unsigned long long)(long)routes;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get_routes failed (%ld)\n", ret);
            return 1;
        }
        for (i = 0; i < (int)ret; i++) {
            routes[i].dest_str[IPV67_ADDR_STR_MAX - 1] = '\0';
            printf("  %s  via ", routes[i].dest_str);
            if (routes[i].next_hop_family == IPV67_PEER_IPV6) {
                putchar('[');
                print_ipv6(routes[i].next_hop_ipv6);
                printf("]:%u", (unsigned int)routes[i].next_hop_port);
            } else {
                print_ipv4(routes[i].next_hop_ipv4);
                printf(":%u", (unsigned int)routes[i].next_hop_port);
            }
            printf("  hops=%u\n", (unsigned int)routes[i].hops);
        }
        return 0;
    }

    if (strcmp(argv[1], "probe") == 0) {
        ret = load_runtime_config(&cfg);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: config load failed (%ld)\n", ret);
            return 1;
        }
        if (argc >= 3) {
            ret = add_peer_to_kernel(argv[2]);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: probe endpoint failed (%ld)\n", ret);
                return 1;
            }
        }
        memset(&probe_req, 0, sizeof(probe_req));
        probe_req.cmd = IPV67_CMD_PROBE_PEERS;
        ret = ipv67_syscall(&probe_req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: probe failed (%ld)\n", ret);
            return 1;
        }
        puts("Peer probes sent.");
        return 0;
    }

    if (strcmp(argv[1], "ping") == 0) {
        if (argc < 3 || strcmp(argv[2], "self") == 0) {
            if (!cfg.address[0]) {
                fputs("ipv67cli: no configured address; run ipv67cli keygen or setaddr first\n", stderr);
                return 1;
            }
            strncpy(addr_buf, cfg.address, sizeof(addr_buf) - 1);
            addr_buf[sizeof(addr_buf) - 1] = '\0';
        } else {
            strncpy(addr_buf, argv[2], sizeof(addr_buf) - 1);
            addr_buf[sizeof(addr_buf) - 1] = '\0';
        }
        ret = parse_ipv67_addr(addr_buf, &addr);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: invalid IPv67 address (%ld)\n", ret);
            return 1;
        }
        ret = load_runtime_config(&cfg);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: config load failed (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PING;
        req.arg1 = (unsigned long long)(long)&addr;
        req.arg2 = 3000;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            if (ret == IPV67_ERR_TIMEOUT) {
                printf("Request timeout for %s\n", addr_buf);
                return 1;
            }
            if (ret == -2) print_route_hint();
            fprintf(stderr, "ipv67cli: ping failed (%ld)\n", ret);
            return 1;
        }
        printf("Reply from %s: time=%ld ms\n", addr_buf, ret);
        return 0;
    }

    if (strcmp(argv[1], "setport") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: setport requires a port number\n", stderr);
            return 1;
        }
        port_val = atoi(argv[2]);
        if (port_val <= 0 || port_val > 65535) {
            fputs("ipv67cli: invalid port\n", stderr);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_PORT;
        req.arg1 = (unsigned long long)(unsigned int)port_val;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: setport failed (%ld)\n", ret);
            return 1;
        }
        snprintf(addr_buf, sizeof(addr_buf), "%d", port_val);
        cfg.port = port_val;
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("IPv67 UDP port set to %d\n", port_val);
        return 0;
    }

    if (strcmp(argv[1], "port") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PORT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get port failed (%ld)\n", ret);
            return 1;
        }
        printf("IPv67 UDP port: %ld\n", ret);
        return 0;
    }

    if (strcmp(argv[1], "keygen") == 0) {
        gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)seed, (long)IPV67_SEED_SIZE, 0);
        if (gr != IPV67_SEED_SIZE) {
            fputs("ipv67cli: keygen: getrandom failed\n", stderr);
            return 1;
        }
        if (ipv67_save_seed_to(keyfile, seed) < 0) {
            fprintf(stderr, "ipv67cli: keygen: failed to save identity key to %s\n", keyfile);
            return 1;
        }
        ipv67_derive_addr(seed, pubaddr);
        if (parse_ipv67_addr(pubaddr, &addr) == 0) {
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_SET_ADDR;
            req.arg1 = (unsigned long long)(long)&addr;
            ipv67_syscall(&req);
        }
        strncpy(cfg.keyfile, keyfile, sizeof(cfg.keyfile) - 1);
        cfg.keyfile[sizeof(cfg.keyfile) - 1] = '\0';
        strncpy(cfg.address, pubaddr, sizeof(cfg.address) - 1);
        cfg.address[sizeof(cfg.address) - 1] = '\0';
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("IPv67 identity key generated.\n");
        printf("Address: %s\n", pubaddr);
        return 0;
    }

    if (strcmp(argv[1], "pubaddr") == 0) {
        if (ipv67_load_seed_from(keyfile, seed) < 0) {
            fprintf(stderr, "ipv67cli: no identity key found at %s\n", keyfile);
            return 1;
        }
        ipv67_derive_addr(seed, pubaddr);
        printf("%s\n", pubaddr);
        return 0;
    }

    if (strcmp(argv[1], "mine") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: mine requires a regex pattern\n", stderr);
            return 1;
        }
        pattern = argv[2];
        attempts = 0;
        printf("Mining for address matching: %s\n", pattern);
        printf("Press Ctrl+C to stop.\n");
        fflush(stdout);

        while (1) {
            gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)seed, (long)IPV67_SEED_SIZE, 0);
            if (gr != IPV67_SEED_SIZE) {
                fputs("ipv67cli: mine: getrandom failed\n", stderr);
                return 1;
            }
            ipv67_derive_addr(seed, pubaddr);
            attempts++;

            match = 1;
            pa = pattern;
            aa = pubaddr;
            if (*pa == '^') pa++;
            while (*pa && *aa) {
                if (*pa == '$') {
                    if (*aa != '\0') match = 0;
                    break;
                }
                if (*(pa+1) == '*') {
                    if (*pa == '.') {
                        aa2 = aa;
                        while (*aa2) aa2++;
                        aa = aa2;
                        pa += 2;
                    } else {
                        while (*aa == *pa) aa++;
                        pa += 2;
                    }
                } else if (*pa == '.' || *pa == *aa) {
                    pa++; aa++;
                } else {
                    match = 0;
                    break;
                }
            }
            if (match && (*pa == '\0' || *pa == '$')) {
                if (ipv67_save_seed_to(keyfile, seed) < 0) {
                    fprintf(stderr, "ipv67cli: mine: failed to save identity key to %s\n", keyfile);
                    return 1;
                }
                printf("Found after %llu attempts!\n", attempts);
                printf("Address: %s\n", pubaddr);
                return 0;
            }

            if ((attempts % 100000) == 0) {
                printf("  %llu attempts...\r", attempts);
                fflush(stdout);
            }
        }
        return 0;
    }

    fprintf(stderr, "ipv67cli: unknown command '%s'\n", argv[1]);
    show_usage();
    return 1;
}

int main(int argc, char **argv) {
    return cmd_ipv67cli(argc, argv);
}
