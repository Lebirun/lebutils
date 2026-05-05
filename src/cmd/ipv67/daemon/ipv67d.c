#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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
#define IPV67_CMD_SEND         7
#define IPV67_CMD_PING         8
#define IPV67_CMD_SET_PORT     9
#define IPV67_CMD_GET_PORT     10
#define IPV67_CMD_ADDR_PARSE   11
#define IPV67_CMD_ADDR_FORMAT  12
#define IPV67_CMD_ADD_PEER6    13
#define IPV67_CMD_ADD_PEER_HOST 14
#define IPV67_CMD_ADD_ENDPOINT 15
#define IPV67_CMD_PROBE_PEERS  16
#define IPV67_CMD_SET_AUTH_KEY 19
#define IPV67_CMD_SET_IDENTITY_KEY 20

#define IPV67_PEER_IPV4        4
#define IPV67_PEER_IPV6        6
#define IPV67_AUTH_KEY_SIZE    32
#define IPV67_IDENTITY_SIZE    32
#define IPV67_ALIAS_SIZE       16

#define IPV67_ZONE_MAX    6
#define IPV67_DOMAIN_MAX  32
#define IPV67_NODE_MAX    6
#define IPV67_ADDR_STR_MAX  (IPV67_ZONE_MAX + 1 + IPV67_ZONE_MAX + 1 + IPV67_DOMAIN_MAX + 1 + IPV67_NODE_MAX + 1 + IPV67_NODE_MAX + 1)

#define IPV67_CONFIG_FILE "/etc/ipv67/ipv67.conf"
#define IPV67_MAX_CONFIG_PEERS 32
#define IPV67_PORT_DEFAULT 6767

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
    unsigned char authenticated;
    unsigned char session_established;
    unsigned char missed_probes;
    char alias[IPV67_ALIAS_SIZE];
    unsigned char public_key[IPV67_IDENTITY_SIZE];
    char addr_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_peer_user_t;

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
    long ret;
    int i;

    if (req && req->arg4 == 0) req->arg4 = ipv67_instance_port;
    for (i = 0; i < 64; i++) {
        ret = leb_syscall1(LEB_SYSCALL_IPV67, (long)req);
        if (ret != -11) return ret;
        leb_syscall1(LEB_SYSCALL_SLEEP, 10);
    }
    return ret;
}

static void ipv67d_log(FILE *log, const char *msg) {
    if (log) {
        fputs(msg, log);
        fputc('\n', log);
        fflush(log);
    }
}

static void write_pid_file(const char *path) {
    FILE *f;
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static void remove_pid_file(const char *path) {
    remove(path);
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

static int parse_ipv4_text(const char *s, unsigned int *out) {
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
        if (s[i] >= '0' && s[i] <= '9') octet = octet * 10 + (s[i] - '0');
        else if (s[i] == '.') {
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
    if (parse_ipv4_text(host, &endpoint->ipv4) == 0) {
        endpoint->family = IPV67_PEER_IPV4;
        endpoint->port = port_val;
        return 0;
    }
    copy_text(endpoint->host, sizeof(endpoint->host), host, (int)strlen(host));
    endpoint->family = 0;
    endpoint->port = port_val;
    return 0;
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

static void load_config_line(char *line, ipv67_config_t *cfg) {
    char *p;
    char *key;
    char *val;

    p = line;
    key = next_token(&p);
    if (!key) return;
    val = next_token(&p);
    if (!val) return;
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

static int load_config(const char *path, ipv67_config_t *cfg) {
    int fd;
    int r;
    int i;
    int pos;
    char buf[2048];
    char line[512];

    config_init(cfg);
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    r = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (r < 0) return -1;
    buf[r] = '\0';
    pos = 0;
    for (i = 0; i <= r; i++) {
        if (buf[i] == '\n' || buf[i] == '\0') {
            line[pos] = '\0';
            load_config_line(line, cfg);
            pos = 0;
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = buf[i];
        }
    }
    return 0;
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

static void do_heartbeat(FILE *log) {
    long ret;
    ipv67_syscall_req_t probe_req;

    memset(&probe_req, 0, sizeof(probe_req));
    probe_req.cmd = IPV67_CMD_PROBE_PEERS;
    ret = ipv67_syscall(&probe_req);
    if (ret < 0) ipv67d_log(log, "ipv67d: peer probe failed");
}

static void show_usage(void) {
    puts("Usage: ipv67d [options]");
    puts("");
    puts("IPv67 network daemon. Manages peer discovery and heartbeats.");
    puts("");
    puts("Options:");
    puts("  -c <file>    Load IPv67 config file (default: " IPV67_CONFIG_FILE ")");
    puts("  -a <addr>    Set IPv67 address on startup");
    puts("  -k <keyfile> Load identity key from file (default: " IPV67_IDENTITY_KEY ")");
    puts("  -p <port>    Set IPv67 UDP port (default 6767)");
    puts("  --peer       Run peer probing mode");
    puts("  --manual     Disable automatic peer probes");
    puts("  -h           Show this help");
    puts("");
    puts("ipv67d runs in the foreground and should be started by the init system.");
}

int main(int argc, char **argv) {
    ipv67_syscall_req_t req;
    FILE *log;
    char addr_buf[IPV67_ADDR_STR_MAX];
    ipv67_addr_t addr;
    char *set_addr;
    int set_port;
    int i;
    long ret;
    unsigned int tick;
    unsigned int last_heartbeat;
    unsigned int heartbeat_interval;
    uint8_t seed[IPV67_SEED_SIZE];
    char key_addr[IPV67_ADDR_STR_MAX];
    const char *keyfile;
    int load_ret;
    const char *config_path;
    static ipv67_config_t cfg;
    int config_loaded;
    int peer_mode;
    int manual_mode;
    static char pid_path[128];
    static char log_path[128];

    set_addr = NULL;
    set_port = 0;
    keyfile = NULL;
    config_path = IPV67_CONFIG_FILE;
    config_init(&cfg);
    config_loaded = 0;
    peer_mode = 0;
    manual_mode = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_usage();
            return 0;
        }
        if (strcmp(argv[i], "--peer") == 0) {
            peer_mode = 1;
        } else if (strcmp(argv[i], "--manual") == 0) {
            manual_mode = 1;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            set_addr = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            set_port = atoi(argv[++i]);
            if (set_port <= 0 || set_port > 65535) {
                fputs("ipv67d: invalid port\n", stderr);
                return 1;
            }
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            keyfile = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    if (load_config(config_path, &cfg) == 0) {
        config_loaded = 1;
        if (!keyfile && cfg.keyfile[0]) keyfile = cfg.keyfile;
        if (!set_addr && cfg.address[0]) set_addr = cfg.address;
        if (set_port == 0 && cfg.port > 0) set_port = cfg.port;
    }
    if (set_port > 0) ipv67_instance_port = (unsigned short)set_port;
    else ipv67_instance_port = (unsigned short)cfg.port;

    snprintf(pid_path, sizeof(pid_path), "/var/run/ipv67d-%s-%u.pid", peer_mode ? "peer" : "node", (unsigned int)ipv67_instance_port);
    snprintf(log_path, sizeof(log_path), "/var/log/ipv67d-%s-%u.log", peer_mode ? "peer" : "node", (unsigned int)ipv67_instance_port);

    log = fopen(log_path, "a");
    ipv67d_log(log, "ipv67d: IPv67 stack selected");

    if (set_port > 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_PORT;
        req.arg1 = (unsigned long long)(unsigned int)set_port;
        ipv67_syscall(&req);
    }

    if (keyfile) {
        load_ret = ipv67_load_seed_from(keyfile, seed);
    } else {
        load_ret = ipv67_load_seed(seed);
    }
    if (load_ret == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
        req.arg1 = (unsigned long long)(long)seed;
        req.arg2 = IPV67_AUTH_KEY_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) ipv67d_log(log, "ipv67d: failed to install identity key");
        else ipv67d_log(log, "ipv67d: installed identity key");
        if (!set_addr) {
            ipv67_derive_addr(seed, key_addr);
            set_addr = key_addr;
            ipv67d_log(log, "ipv67d: loaded identity key");
        }
    }

    if (set_addr) {
        ret = parse_ipv67_addr(set_addr, &addr);
        if (ret < 0) {
            ipv67d_log(log, "ipv67d: invalid address");
            fprintf(stderr, "ipv67d: invalid address (%ld)\n", ret);
            if (log) fclose(log);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            ipv67d_log(log, "ipv67d: failed to set address");
        } else {
            ipv67d_log(log, "ipv67d: address configured");
        }
    }

    if (peer_mode && config_loaded) {
        for (i = 0; i < cfg.peer_count; i++) {
            ret = add_peer_to_kernel(cfg.peer_endpoint[i]);
            if (ret < 0) ipv67d_log(log, "ipv67d: failed to add configured peer");
        }
    }

    memset(addr_buf, 0, sizeof(addr_buf));
    memset(&addr, 0, sizeof(addr));
    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_GET_ADDR;
    req.arg1 = (unsigned long long)(long)&addr;
    ipv67_syscall(&req);
    ret = format_ipv67_addr(&addr, addr_buf);
    if (ret >= 0 && addr_buf[0] != '.') {
        printf("ipv67d: address = %s\n", addr_buf);
    } else {
        puts("ipv67d: no address set (use ipv67cli setaddr <addr>)");
    }

    write_pid_file(pid_path);
    printf("ipv67d: running (%s, log: %s)\n", peer_mode ? "peer" : "node", log_path);
    ipv67d_log(log, "ipv67d: started");

    heartbeat_interval = 30000;
    last_heartbeat = getticks();

    while (1) {
        tick = getticks();
        if (peer_mode && !manual_mode && tick - last_heartbeat >= heartbeat_interval) {
            do_heartbeat(log);
            last_heartbeat = tick;
        }
        leb_syscall1(LEB_SYSCALL_SLEEP, 1000);
    }

    ipv67d_log(log, "ipv67d: stopping");
    remove_pid_file(pid_path);
    if (log) fclose(log);
    return 0;
}
