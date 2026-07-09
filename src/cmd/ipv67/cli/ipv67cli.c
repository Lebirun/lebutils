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
#define IPV67_CMD_SET_AUTH_KEY 19
#define IPV67_CMD_SET_IDENTITY_KEY 20
#define IPV67_CMD_SET_ASN      21
#define IPV67_CMD_REMOVE_ASN   22
#define IPV67_CMD_GET_ASNS     23
#define IPV67_CMD_ASN_COUNT    24
#define IPV67_CMD_GET_STATS    25
#define IPV67_CMD_CLEAR_STATS  26
#define IPV67_CMD_TRACE        27
#define IPV67_CMD_SET_AUTH_REQUIRED 28
#define IPV67_CMD_GET_AUTH_REQUIRED 29
#define IPV67_CMD_GET_LOCAL_ASN 30
#define IPV67_CMD_PUNCH        31
#define IPV67_CMD_SHUTDOWN     32

#define IPV67_PEER_IPV4        4
#define IPV67_PEER_IPV6        6

#define IPV67_CONFIG_FILE      "/etc/ipv67/ipv67.conf"
#define IPV67_MAX_CONFIG_PEERS 32
#define IPV67_PORT_DEFAULT     6767
#define IPV67_ERR_TIMEOUT      -6
#define IPV67_ALIAS_SIZE       16
#define IPV67_IDENTITY_SIZE    32
#define IPV67_AUTH_KEY_SIZE    32
#define IPV67_MAX_CONFIG_ASNS  32
#define IPV67_ASN_NAME_SIZE    32
#define IPV67_ASN_LABEL_SIZE   32
#define IPV67_ASN_COUNTRY_SIZE 3
#define IPV67_ASN_SOURCE_SIZE  16
#define IPV67_TRACE_MAX_HOPS   16
#define IPV67_ASN_FLAG_LOCAL   0x01
#define IPV67_ASN_FLAG_BROAD   0x02
#define IPV67_ASN_FLAG_CONFLICT 0x04
#define IPV67_ASN_FLAG_AUTH    0x08
#define IPV67_ASN_FLAG_RELAY   0x10
#define IPV67_ASN_FLAG_QUARANTINE 0x20
#define IPV67_ASN_OWNERSHIP_SPECIFICITY 136

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
    unsigned char authenticated;
    unsigned char session_established;
    unsigned char missed_probes;
    unsigned char addr_verified;
    char alias[IPV67_ALIAS_SIZE];
    unsigned char public_key[IPV67_IDENTITY_SIZE];
    char addr_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_peer_user_t;

typedef struct {
    unsigned int next_hop_ipv4;
    unsigned char next_hop_ipv6[16];
    unsigned short next_hop_port;
    unsigned char next_hop_family;
    unsigned char hops;
    unsigned char metric;
    unsigned int sequence;
    unsigned char public_key[IPV67_IDENTITY_SIZE];
    char dest_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_route_user_t;

typedef struct {
    unsigned int asn;
    unsigned char specificity;
    unsigned char flags;
    unsigned char conflict_count;
    char country[IPV67_ASN_COUNTRY_SIZE];
    char source[IPV67_ASN_SOURCE_SIZE];
    char name[IPV67_ASN_NAME_SIZE];
    char label[IPV67_ASN_LABEL_SIZE];
    char start_str[IPV67_ADDR_STR_MAX];
    char end_str[IPV67_ADDR_STR_MAX];
} __attribute__((packed)) ipv67_asn_user_t;

typedef struct {
    unsigned long long rx_packets;
    unsigned long long tx_packets;
    unsigned long long forwarded_packets;
    unsigned long long replay_drops;
    unsigned long long auth_drops;
    unsigned long long no_route_drops;
    unsigned long long malformed_drops;
    unsigned long long route_updates;
    unsigned long long asn_claims_rx;
    unsigned long long asn_claims_tx;
    unsigned long long punch_requests_tx;
    unsigned long long punch_observed_tx;
    unsigned long long punch_packets_rx;
    unsigned long long punch_peers_learned;
    unsigned long long auth_hello_rx;
    unsigned long long auth_reply_rx;
    unsigned long long auth_done_rx;
    unsigned long long auth_payload_ok;
    unsigned long long auth_payload_fail;
    unsigned long long auth_sessions;
    unsigned long long auth_fail_self;
    unsigned long long auth_fail_challenge;
    unsigned long long auth_fail_shared;
    unsigned long long auth_fail_identity;
    unsigned long long auth_session_fail;
} __attribute__((packed)) ipv67_stats_user_t;

typedef struct {
    unsigned char hop;
    unsigned char final;
    unsigned char reserved[2];
    unsigned int rtt;
    char addr[IPV67_ADDR_STR_MAX];
    char alias[IPV67_ALIAS_SIZE];
} __attribute__((packed)) ipv67_trace_hop_user_t;

typedef struct {
    unsigned char hop_count;
    unsigned char complete;
    unsigned char reserved[6];
    ipv67_trace_hop_user_t hops[IPV67_TRACE_MAX_HOPS];
} __attribute__((packed)) ipv67_trace_result_user_t;

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
    char authkey[IPV67_AUTH_KEY_SIZE * 2 + 1];
    char address[IPV67_ADDR_STR_MAX];
    int port;
    int probe_interval;
    int auth_required;
    int auth_required_set;
    int peer_count;
    int asn_count;
    char peer_endpoint[IPV67_MAX_CONFIG_PEERS][256];
    ipv67_asn_user_t asns[IPV67_MAX_CONFIG_ASNS];
} ipv67_config_t;

static unsigned short ipv67_instance_port = IPV67_PORT_DEFAULT;

static int asn_claim_is_metadata_only_cli(const ipv67_asn_user_t *claim);

static long ipv67_syscall(ipv67_syscall_req_t *req) {
    long ret;
    int i;
    static int module_error_printed;

    if (req && req->arg4 == 0) req->arg4 = ipv67_instance_port;
    for (i = 0; i < 64; i++) {
        ret = leb_syscall1(LEB_SYSCALL_IPV67, (long)req);
        if ((ret == -19 || ret == -38) && !module_error_printed) {
            fputs("ipv67cli: IPv67 module is not loaded; load ipv67.lke before using IPv67 commands\n", stderr);
            module_error_printed = 1;
        }
        if (ret != -11) return ret;
        leb_syscall1(LEB_SYSCALL_SLEEP, 10);
    }
    return ret;
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

static int ipv4_is_loopback_cli(unsigned int ip) {
    return ((ip >> 24) & 0xff) == 127;
}

static int ipv6_is_loopback_cli(const unsigned char *ip) {
    int i;

    if (!ip) return 0;
    for (i = 0; i < 15; i++) {
        if (ip[i] != 0) return 0;
    }
    return ip[15] == 1;
}

static int endpoint_is_loopback_cli(unsigned char family, unsigned int ipv4, const unsigned char *ipv6) {
    if (family == IPV67_PEER_IPV4) return ipv4_is_loopback_cli(ipv4);
    if (family == IPV67_PEER_IPV6) return ipv6_is_loopback_cli(ipv6);
    return 0;
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
    puts("  shutdown           Free this IPv67 runtime instance");
    puts("  addr               Show current IPv67 address");
    puts("  addrraw            Show current IPv67 address only");
    puts("  setaddr <addr>     Set IPv67 address from configured ASN ownership");
    puts("  addpeer <endpoint> Add a persistent peer endpoint");
    puts("  addpeer <host> <port>");
    puts("                     Add a persistent peer endpoint");
    puts("  rmpeer <endpoint|ipv67addr>");
    puts("                     Remove a configured endpoint or runtime peer");
    puts("  peers              List known peers");
    puts("  peeraddr           Show first authenticated verified peer address only");
    puts("  waitpeer [tries]   Wait for an authenticated verified peer");
    puts("  routes             List known routes");
    puts("  asn self [country <cc>] [label <text>] [name <text>]");
    puts("                     Claim the current IPv67 address as one ASN address");
    puts("  asn prefix <prefix> [country <cc>] [label <text>] [name <text>]");
    puts("                     Claim an IPv67 ASN prefix for your identity ASN");
    puts("                     Example: asn prefix 1928ab.676141.wowamveryhappy.000001.*");
    puts("                     Usable ownership prefixes must be zone1.zone2.domain.node1.* or exact");
    puts("  asn range <start> <end> [country <cc>] [label <text>] [name <text>]");
    puts("                     Claim an IPv67 ASN range for your identity ASN");
    puts("  asn id             Show this identity's local ASN number");
    puts("  asn use [<ipv67addr>|auto|random]");
    puts("                     Set this node address from an owned ASN claim; default is random");
    puts("  asn rm <prefix|start> [end]");
    puts("                     Remove a configured ASN claim");
    puts("  asns               List local and learned ASN claims");
    puts("  asn conflicts      List conflicting ASN claims");
    puts("                     ASN numbers are derived from the identity key, not typed");
    puts("  probe [endpoint]   Probe configured peers or one endpoint now");
    puts("  ping [self|ipv67addr]");
    puts("                     Ping an IPv67 address");
    puts("  pingpeer           Ping first authenticated verified peer");
    puts("  trace <ipv67addr>  Trace routed IPv67 hops");
    puts("  punch <ipv67addr>  Request NAT endpoint discovery for a peer");
    puts("  check              Show IPv67 mesh readiness evidence");
    puts("  testplan           Show a two-machine IPv67 verification recipe");
    puts("  smoke-asn          Run the single-machine ASN smoke test");
    puts("  stats [clear]      Show or clear IPv67 counters");
    puts("  status             Show IPv67 runtime status");
    puts("  config             Show loaded IPv67 config");
    puts("  setport <port>     Set IPv67 UDP port");
    puts("  probe-interval <ms>");
    puts("                     Set automatic peer probe interval");
    puts("  port               Show current IPv67 UDP port");
    puts("  authkey <hex64|auto>");
    puts("                     Set shared HMAC authentication key");
    puts("  auth-required <on|off>");
    puts("                     Require authenticated non-bootstrap packets");
    puts("  keygen             Generate a new IPv67 identity key");
    puts("  keyensure          Load an existing key or generate one if missing");
    puts("");
    puts("Config:");
    puts("  Default config: " IPV67_CONFIG_FILE);
    puts("  Peer lines are kept as: peer <endpoint>");
    puts("  Auth key lines are kept as: authkey <64 hex chars>");
    puts("  Auth policy is kept as: auth_required <0|1>");
    puts("  Probe interval is kept as: probe_interval <milliseconds>");
    puts("  ASN lines are kept as: asn <start> <end> <specificity> <country> <name> <label>");
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

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int is_decimal_text(const char *text) {
    int i;

    if (!text || !text[0]) return 0;
    for (i = 0; text[i]; i++) {
        if (text[i] < '0' || text[i] > '9') return 0;
    }
    return 1;
}

static int is_asn_text(const char *text) {
    if (!text || !text[0]) return 0;
    if ((text[0] == 'A' || text[0] == 'a') && (text[1] == 'S' || text[1] == 's')) return is_decimal_text(text + 2);
    return is_decimal_text(text);
}

static int key_present_cli(const unsigned char *key) {
    int i;

    if (!key) return 0;
    for (i = 0; i < IPV67_IDENTITY_SIZE; i++) {
        if (key[i] != 0) return 1;
    }
    return 0;
}

static void print_key_tag_cli(const unsigned char *key) {
    int i;

    if (!key_present_cli(key)) return;
    printf("  key=");
    for (i = 0; i < 7; i++) printf("%02x", (unsigned int)key[i]);
}

static int parse_hex_key(const char *text, unsigned char *out, int out_len) {
    int i;
    int hi;
    int lo;

    if (!text || !out || out_len <= 0) return -1;
    for (i = 0; i < out_len * 2; i++) {
        if (!text[i]) return -1;
        if (hex_value(text[i]) < 0) return -1;
    }
    if (text[out_len * 2]) return -1;
    for (i = 0; i < out_len; i++) {
        hi = hex_value(text[i * 2]);
        lo = hex_value(text[i * 2 + 1]);
        out[i] = (unsigned char)((hi << 4) | lo);
    }
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
    cfg->probe_interval = 10000;
}

static void ensure_config_dir(void) {
    vfs_mkdir("/etc/ipv67", 0700);
}

static int write_all_fd(int fd, const char *buf, int len) {
    int off;
    int ret;

    off = 0;
    while (off < len) {
        ret = (int)write(fd, buf + off, (size_t)(len - off));
        if (ret <= 0) return -1;
        off += ret;
    }
    return 0;
}

static void load_config_line(char *line, ipv67_config_t *cfg) {
    char *p;
    char *key;
    char *val;
    char *start;

    p = line;
    key = next_token(&p);
    if (!key) return;
    val = next_token(&p);
    if (!val) return;
    if (strcmp(key, "identity") == 0 || strcmp(key, "keyfile") == 0) {
        strncpy(cfg->keyfile, val, sizeof(cfg->keyfile) - 1);
        cfg->keyfile[sizeof(cfg->keyfile) - 1] = '\0';
    } else if (strcmp(key, "authkey") == 0) {
        strncpy(cfg->authkey, val, sizeof(cfg->authkey) - 1);
        cfg->authkey[sizeof(cfg->authkey) - 1] = '\0';
    } else if (strcmp(key, "auth_required") == 0) {
        cfg->auth_required = atoi(val) ? 1 : 0;
        cfg->auth_required_set = 1;
    } else if (strcmp(key, "address") == 0) {
        strncpy(cfg->address, val, sizeof(cfg->address) - 1);
        cfg->address[sizeof(cfg->address) - 1] = '\0';
    } else if (strcmp(key, "port") == 0) {
        cfg->port = atoi(val);
        if (cfg->port <= 0 || cfg->port > 65535) cfg->port = IPV67_PORT_DEFAULT;
    } else if (strcmp(key, "probe_interval") == 0) {
        cfg->probe_interval = atoi(val);
        if (cfg->probe_interval < 1000) cfg->probe_interval = 1000;
    } else if (strcmp(key, "peer") == 0 && cfg->peer_count < IPV67_MAX_CONFIG_PEERS) {
        strncpy(cfg->peer_endpoint[cfg->peer_count], val, sizeof(cfg->peer_endpoint[0]) - 1);
        cfg->peer_endpoint[cfg->peer_count][sizeof(cfg->peer_endpoint[0]) - 1] = '\0';
        cfg->peer_count++;
    } else if (strcmp(key, "asn") == 0 && cfg->asn_count < IPV67_MAX_CONFIG_ASNS) {
        cfg->asns[cfg->asn_count].asn = 0;
        if (strchr(val, '.')) {
            start = val;
        } else {
            start = next_token(&p);
            if (!start) return;
        }
        strncpy(cfg->asns[cfg->asn_count].start_str, start, IPV67_ADDR_STR_MAX - 1);
        val = next_token(&p);
        if (!val) return;
        strncpy(cfg->asns[cfg->asn_count].end_str, val, IPV67_ADDR_STR_MAX - 1);
        val = next_token(&p);
        if (!val) return;
        cfg->asns[cfg->asn_count].specificity = (unsigned char)atoi(val);
        val = next_token(&p);
        if (val) strncpy(cfg->asns[cfg->asn_count].country, val, IPV67_ASN_COUNTRY_SIZE - 1);
        val = next_token(&p);
        if (val) strncpy(cfg->asns[cfg->asn_count].name, val, IPV67_ASN_NAME_SIZE - 1);
        val = next_token(&p);
        if (val) strncpy(cfg->asns[cfg->asn_count].label, val, IPV67_ASN_LABEL_SIZE - 1);
        cfg->asns[cfg->asn_count].flags = IPV67_ASN_FLAG_LOCAL;
        if (asn_claim_is_metadata_only_cli(&cfg->asns[cfg->asn_count])) cfg->asns[cfg->asn_count].flags |= IPV67_ASN_FLAG_BROAD;
        cfg->asn_count++;
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

static int save_config(const char *path, const ipv67_config_t *cfg) {
    int fd;
    int i;
    int len;
    char line[512];

    ensure_config_dir();
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    if (cfg->keyfile[0]) {
        len = snprintf(line, sizeof(line), "identity %s\n", cfg->keyfile);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    if (cfg->authkey[0]) {
        len = snprintf(line, sizeof(line), "authkey %s\n", cfg->authkey);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    if (cfg->auth_required || cfg->auth_required_set) {
        len = snprintf(line, sizeof(line), "auth_required %d\n", cfg->auth_required ? 1 : 0);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    if (cfg->address[0]) {
        len = snprintf(line, sizeof(line), "address %s\n", cfg->address);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    if (cfg->port > 0) {
        len = snprintf(line, sizeof(line), "port %d\n", cfg->port);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    if (cfg->probe_interval > 0) {
        len = snprintf(line, sizeof(line), "probe_interval %d\n", cfg->probe_interval);
        if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }
    for (i = 0; i < cfg->peer_count; i++) {
        if (cfg->peer_endpoint[i][0]) {
            len = snprintf(line, sizeof(line), "peer %s\n", cfg->peer_endpoint[i]);
            if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
                close(fd);
                return -1;
            }
        }
    }
    for (i = 0; i < cfg->asn_count; i++) {
        if (cfg->asns[i].start_str[0] && cfg->asns[i].end_str[0]) {
            len = snprintf(line, sizeof(line), "asn %s %s %u %s %s %s\n",
                           cfg->asns[i].start_str,
                           cfg->asns[i].end_str,
                           (unsigned int)cfg->asns[i].specificity,
                           cfg->asns[i].country[0] ? cfg->asns[i].country : "XX",
                           cfg->asns[i].name[0] ? cfg->asns[i].name : "custom",
                           cfg->asns[i].label[0] ? cfg->asns[i].label : "claim");
            if (len < 0 || len >= (int)sizeof(line) || write_all_fd(fd, line, len) < 0) {
                close(fd);
                return -1;
            }
        }
    }
    close(fd);
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

static void copy_token(char *dst, int dst_len, const char *src) {
    int i;

    if (!dst || dst_len <= 0) return;
    memset(dst, 0, dst_len);
    if (!src) return;
    for (i = 0; src[i] && i < dst_len - 1; i++) dst[i] = src[i];
}

static unsigned int hex_part_value_cli(const char *s) {
    unsigned int v;
    int i;
    char c;

    v = 0;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
    }
    return v;
}

static int addr_part_cmp_cli(const char *a, const char *b, int hex) {
    unsigned int av;
    unsigned int bv;

    if (hex) {
        av = hex_part_value_cli(a);
        bv = hex_part_value_cli(b);
        if (av < bv) return -1;
        if (av > bv) return 1;
        return 0;
    }
    return strcmp(a, b);
}

static int addr_cmp_cli(const ipv67_addr_t *a, const ipv67_addr_t *b) {
    int ret;

    ret = addr_part_cmp_cli(a->zone1, b->zone1, 1);
    if (ret) return ret;
    ret = addr_part_cmp_cli(a->zone2, b->zone2, 1);
    if (ret) return ret;
    ret = addr_part_cmp_cli(a->domain, b->domain, 0);
    if (ret) return ret;
    ret = addr_part_cmp_cli(a->node1, b->node1, 1);
    if (ret) return ret;
    return addr_part_cmp_cli(a->node2, b->node2, 1);
}

static int addr_in_asn_claim(const ipv67_addr_t *addr, const ipv67_asn_user_t *asn) {
    ipv67_addr_t start;
    ipv67_addr_t end;

    if (!addr || !asn) return 0;
    if (parse_ipv67_addr(asn->start_str, &start) < 0) return 0;
    if (parse_ipv67_addr(asn->end_str, &end) < 0) return 0;
    return addr_cmp_cli(&start, addr) <= 0 && addr_cmp_cli(addr, &end) <= 0;
}

static unsigned char asn_range_specificity_cli(const ipv67_addr_t *start, const ipv67_addr_t *end) {
    if (!start || !end) return 0;
    if (strcmp(start->zone1, end->zone1) != 0) return 0;
    if (strcmp(start->zone2, end->zone2) != 0) return 24;
    if (strcmp(start->domain, end->domain) != 0) return 48;
    if (strcmp(start->node1, end->node1) != 0) return 112;
    if (strcmp(start->node2, end->node2) != 0) return 136;
    return 160;
}

static int asn_claim_grants_ownership_cli(const ipv67_asn_user_t *claim) {
    if (!claim) return 0;
    if (claim->specificity < IPV67_ASN_OWNERSHIP_SPECIFICITY) return 0;
    if (claim->flags & IPV67_ASN_FLAG_RELAY) return 0;
    return 1;
}

static int asn_claim_is_metadata_only_cli(const ipv67_asn_user_t *claim) {
    if (!claim) return 1;
    return claim->specificity < IPV67_ASN_OWNERSHIP_SPECIFICITY;
}

static unsigned int hash_word_cli(const uint8_t *hash, int off) {
    unsigned int v;

    v = 0;
    v |= ((unsigned int)hash[off & 63]) << 24;
    v |= ((unsigned int)hash[(off + 1) & 63]) << 16;
    v |= ((unsigned int)hash[(off + 2) & 63]) << 8;
    v |= (unsigned int)hash[(off + 3) & 63];
    return v;
}

static void choose_hex_part_cli(char *out, int out_len, const char *start, const char *end, unsigned int rnd) {
    unsigned int lo;
    unsigned int hi;
    unsigned int span;
    unsigned int value;

    if (strcmp(start, end) == 0) {
        copy_token(out, out_len, start);
        return;
    }
    lo = hex_part_value_cli(start);
    hi = hex_part_value_cli(end);
    if (hi < lo) {
        copy_token(out, out_len, start);
        return;
    }
    span = hi - lo + 1;
    value = lo;
    if (span > 0) value = lo + (rnd % span);
    snprintf(out, out_len, "%06x", value & 0xffffff);
}

static int auto_asn_hash_cli(const uint8_t seed[IPV67_SEED_SIZE], unsigned int asn, const ipv67_asn_user_t *claim, int attempt, uint8_t hash[IPV67_HASH_SIZE]) {
    uint8_t material[IPV67_SEED_SIZE + 4 + IPV67_ADDR_STR_MAX + IPV67_ADDR_STR_MAX + 4];
    int pos;
    int i;

    memset(material, 0, sizeof(material));
    pos = 0;
    for (i = 0; i < IPV67_SEED_SIZE; i++) material[pos++] = seed[i];
    material[pos++] = (uint8_t)((asn >> 24) & 0xff);
    material[pos++] = (uint8_t)((asn >> 16) & 0xff);
    material[pos++] = (uint8_t)((asn >> 8) & 0xff);
    material[pos++] = (uint8_t)(asn & 0xff);
    for (i = 0; claim->start_str[i] && pos < (int)sizeof(material); i++) material[pos++] = (uint8_t)claim->start_str[i];
    for (i = 0; claim->end_str[i] && pos < (int)sizeof(material); i++) material[pos++] = (uint8_t)claim->end_str[i];
    material[pos++] = (uint8_t)((attempt >> 24) & 0xff);
    material[pos++] = (uint8_t)((attempt >> 16) & 0xff);
    material[pos++] = (uint8_t)((attempt >> 8) & 0xff);
    material[pos++] = (uint8_t)(attempt & 0xff);
    ipv67_sha512(material, (size_t)pos, hash);
    return 0;
}

static int auto_asn_address_from_claim_cli(const ipv67_config_t *cfg, unsigned int asn, const ipv67_asn_user_t *claim, char *out) {
    uint8_t seed[IPV67_SEED_SIZE];
    uint8_t hash[IPV67_HASH_SIZE];
    ipv67_addr_t start;
    ipv67_addr_t end;
    ipv67_addr_t addr;
    char text[IPV67_ADDR_STR_MAX];
    int attempt;
    int ret;

    if (!cfg || !claim || !out) return -1;
    if (ipv67_load_seed_from(cfg->keyfile, seed) < 0) return -2;
    if (parse_ipv67_addr(claim->start_str, &start) < 0) return -1;
    if (parse_ipv67_addr(claim->end_str, &end) < 0) return -1;
    for (attempt = 0; attempt < 64; attempt++) {
        auto_asn_hash_cli(seed, asn, claim, attempt, hash);
        memset(&addr, 0, sizeof(addr));
        choose_hex_part_cli(addr.zone1, sizeof(addr.zone1), start.zone1, end.zone1, hash_word_cli(hash, 0));
        choose_hex_part_cli(addr.zone2, sizeof(addr.zone2), start.zone2, end.zone2, hash_word_cli(hash, 4));
        if (strcmp(start.domain, end.domain) == 0) {
            copy_token(addr.domain, sizeof(addr.domain), start.domain);
        } else {
            ret = snprintf(addr.domain, sizeof(addr.domain), "%08x%08x", hash_word_cli(hash, 8), hash_word_cli(hash, 12));
            if (ret < 0 || ret >= (int)sizeof(addr.domain)) copy_token(addr.domain, sizeof(addr.domain), start.domain);
        }
        choose_hex_part_cli(addr.node1, sizeof(addr.node1), start.node1, end.node1, hash_word_cli(hash, 16));
        choose_hex_part_cli(addr.node2, sizeof(addr.node2), start.node2, end.node2, hash_word_cli(hash, 20));
        if (!addr_in_asn_claim(&addr, claim)) copy_token(addr.domain, sizeof(addr.domain), start.domain);
        if (addr_in_asn_claim(&addr, claim) && format_ipv67_addr(&addr, text) == 0) {
            copy_token(out, IPV67_ADDR_STR_MAX, text);
            return 0;
        }
    }
    copy_token(out, IPV67_ADDR_STR_MAX, claim->start_str);
    return 1;
}

static int random_asn_address_from_claim_cli(const ipv67_asn_user_t *claim, char *out) {
    uint8_t hash[IPV67_HASH_SIZE];
    ipv67_addr_t start;
    ipv67_addr_t end;
    ipv67_addr_t addr;
    char text[IPV67_ADDR_STR_MAX];
    long gr;
    int attempt;
    int ret;

    if (!claim || !out) return -1;
    if (parse_ipv67_addr(claim->start_str, &start) < 0) return -1;
    if (parse_ipv67_addr(claim->end_str, &end) < 0) return -1;
    for (attempt = 0; attempt < 64; attempt++) {
        gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)hash, (long)sizeof(hash), 0);
        if (gr != (long)sizeof(hash)) return -2;
        memset(&addr, 0, sizeof(addr));
        choose_hex_part_cli(addr.zone1, sizeof(addr.zone1), start.zone1, end.zone1, hash_word_cli(hash, 0));
        choose_hex_part_cli(addr.zone2, sizeof(addr.zone2), start.zone2, end.zone2, hash_word_cli(hash, 4));
        if (strcmp(start.domain, end.domain) == 0) {
            copy_token(addr.domain, sizeof(addr.domain), start.domain);
        } else {
            ret = snprintf(addr.domain, sizeof(addr.domain), "%08x%08x", hash_word_cli(hash, 8), hash_word_cli(hash, 12));
            if (ret < 0 || ret >= (int)sizeof(addr.domain)) copy_token(addr.domain, sizeof(addr.domain), start.domain);
        }
        choose_hex_part_cli(addr.node1, sizeof(addr.node1), start.node1, end.node1, hash_word_cli(hash, 16));
        choose_hex_part_cli(addr.node2, sizeof(addr.node2), start.node2, end.node2, hash_word_cli(hash, 20));
        if (!addr_in_asn_claim(&addr, claim)) copy_token(addr.domain, sizeof(addr.domain), start.domain);
        if (addr_in_asn_claim(&addr, claim) && format_ipv67_addr(&addr, text) == 0) {
            copy_token(out, IPV67_ADDR_STR_MAX, text);
            return 0;
        }
    }
    return -1;
}

static int split_ipv67_pattern(const char *text, char parts[5][IPV67_DOMAIN_MAX + 1]) {
    int part;
    int pos;
    int i;
    int j;

    if (!text) return -1;
    memset(parts, 0, 5 * (IPV67_DOMAIN_MAX + 1));
    part = 0;
    pos = 0;
    for (i = 0; text[i]; i++) {
        if (text[i] == '.') {
            if (part >= 4 || pos == 0) return -1;
            part++;
            pos = 0;
        } else {
            if (pos >= IPV67_DOMAIN_MAX) return -1;
            parts[part][pos++] = text[i];
        }
    }
    if (pos == 0) return -1;
    if (part < 4 && strcmp(parts[part], "*") == 0) {
        for (j = part + 1; j < 5; j++) copy_token(parts[j], IPV67_DOMAIN_MAX + 1, "*");
        return 0;
    }
    if (part != 4) return -1;
    return 0;
}

static int derive_local_asn_cli(const ipv67_config_t *cfg, unsigned int *asn) {
    uint8_t seed[IPV67_SEED_SIZE];
    ipv67_syscall_req_t req;
    long ret;

    if (!cfg || !asn) return -1;
    if (ipv67_load_seed_from(cfg->keyfile, seed) < 0) return -2;
    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
    req.arg1 = (unsigned long long)(long)seed;
    req.arg2 = IPV67_SEED_SIZE;
    ret = ipv67_syscall(&req);
    if (ret < 0) return -1;
    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_GET_LOCAL_ASN;
    ret = ipv67_syscall(&req);
    if (ret <= 0) return -1;
    *asn = (unsigned int)ret;
    return 0;
}

static int derive_local_asn_offline_cli(const ipv67_config_t *cfg, unsigned int *asn) {
    uint8_t seed[IPV67_SEED_SIZE];
    unsigned int value;

    if (!cfg || !asn) return -1;
    if (ipv67_load_seed_from(cfg->keyfile, seed) < 0) return -2;
    value = ipv67_derive_asn(seed);
    if (value == 0) return -1;
    *asn = value;
    return 0;
}

static int build_asn_prefix(const char *text, ipv67_asn_user_t *out) {
    char parts[5][IPV67_DOMAIN_MAX + 1];
    char start[IPV67_ADDR_STR_MAX];
    char end[IPV67_ADDR_STR_MAX];
    int star;
    int i;
    int specificity;
    int ret;
    ipv67_addr_t tmp;

    if (!text || !out) return -1;
    if (parse_ipv67_addr(text, &tmp) == 0) {
        copy_token(out->start_str, IPV67_ADDR_STR_MAX, text);
        copy_token(out->end_str, IPV67_ADDR_STR_MAX, text);
        out->specificity = 160;
        return 0;
    }
    if (split_ipv67_pattern(text, parts) < 0) return -1;
    star = -1;
    for (i = 0; i < 5; i++) {
        if (strcmp(parts[i], "*") == 0) {
            if (star < 0) star = i;
        } else if (star >= 0) {
            return -1;
        }
    }
    if (star < 0) return -1;
    for (i = star; i < 5; i++) {
        if (strcmp(parts[i], "*") != 0) return -1;
    }
    specificity = 160;
    if (star == 1) specificity = 24;
    else if (star == 2) specificity = 48;
    else if (star == 3) specificity = 112;
    else if (star == 4) specificity = 136;
    else return -1;
    if (star <= 1) {
        copy_token(parts[1], IPV67_DOMAIN_MAX + 1, "000000");
    }
    if (star <= 2) {
        copy_token(parts[2], IPV67_DOMAIN_MAX + 1, "0");
    }
    if (star <= 3) {
        copy_token(parts[3], IPV67_DOMAIN_MAX + 1, "000000");
    }
    if (star <= 4) {
        copy_token(parts[4], IPV67_DOMAIN_MAX + 1, "000000");
    }
    ret = snprintf(start, sizeof(start), "%s.%s.%s.%s.%s", parts[0], parts[1], parts[2], parts[3], parts[4]);
    if (ret < 0 || ret >= (int)sizeof(start)) return -1;
    if (star <= 1) copy_token(parts[1], IPV67_DOMAIN_MAX + 1, "ffffff");
    if (star <= 2) copy_token(parts[2], IPV67_DOMAIN_MAX + 1, "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
    if (star <= 3) copy_token(parts[3], IPV67_DOMAIN_MAX + 1, "ffffff");
    if (star <= 4) copy_token(parts[4], IPV67_DOMAIN_MAX + 1, "ffffff");
    ret = snprintf(end, sizeof(end), "%s.%s.%s.%s.%s", parts[0], parts[1], parts[2], parts[3], parts[4]);
    if (ret < 0 || ret >= (int)sizeof(end)) return -1;
    if (parse_ipv67_addr(start, &tmp) < 0) return -1;
    if (parse_ipv67_addr(end, &tmp) < 0) return -1;
    copy_token(out->start_str, IPV67_ADDR_STR_MAX, start);
    copy_token(out->end_str, IPV67_ADDR_STR_MAX, end);
    out->specificity = (unsigned char)specificity;
    return 0;
}

static int config_find_asn_claim(const ipv67_config_t *cfg, const ipv67_asn_user_t *claim) {
    int i;

    if (!cfg || !claim) return -1;
    for (i = 0; i < cfg->asn_count; i++) {
        if (strcmp(cfg->asns[i].start_str, claim->start_str) == 0 &&
            strcmp(cfg->asns[i].end_str, claim->end_str) == 0) {
            if (cfg->asns[i].asn != 0 && claim->asn != 0 && cfg->asns[i].asn != claim->asn) continue;
            return i;
        }
    }
    return -1;
}

static int config_add_asn(ipv67_config_t *cfg, const ipv67_asn_user_t *claim) {
    int idx;

    idx = config_find_asn_claim(cfg, claim);
    if (idx >= 0) {
        memcpy(&cfg->asns[idx], claim, sizeof(*claim));
        return 0;
    }
    if (cfg->asn_count >= IPV67_MAX_CONFIG_ASNS) return -1;
    memcpy(&cfg->asns[cfg->asn_count], claim, sizeof(*claim));
    cfg->asn_count++;
    return 1;
}

static int config_remove_asn(ipv67_config_t *cfg, const ipv67_asn_user_t *claim) {
    int idx;
    int i;

    idx = config_find_asn_claim(cfg, claim);
    if (idx < 0) return -1;
    for (i = idx; i + 1 < cfg->asn_count; i++) {
        memcpy(&cfg->asns[i], &cfg->asns[i + 1], sizeof(cfg->asns[i]));
    }
    memset(&cfg->asns[cfg->asn_count - 1], 0, sizeof(cfg->asns[0]));
    cfg->asn_count--;
    return 0;
}

static int apply_asn_options(ipv67_asn_user_t *claim, int argc, char **argv, int start) {
    int i;

    for (i = start; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "country") == 0) copy_token(claim->country, IPV67_ASN_COUNTRY_SIZE, argv[i + 1]);
        else if (strcmp(argv[i], "name") == 0) copy_token(claim->name, IPV67_ASN_NAME_SIZE, argv[i + 1]);
        else if (strcmp(argv[i], "label") == 0) copy_token(claim->label, IPV67_ASN_LABEL_SIZE, argv[i + 1]);
        else return -1;
    }
    if (i != argc) return -1;
    if (!claim->country[0]) copy_token(claim->country, IPV67_ASN_COUNTRY_SIZE, "XX");
    if (!claim->name[0]) copy_token(claim->name, IPV67_ASN_NAME_SIZE, "custom");
    if (!claim->label[0]) copy_token(claim->label, IPV67_ASN_LABEL_SIZE, "claim");
    return 0;
}

static int install_asn_claim(const ipv67_asn_user_t *claim) {
    ipv67_syscall_req_t req;
    long ret;

    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_SET_ASN;
    req.arg1 = (unsigned long long)(long)claim;
    ret = ipv67_syscall(&req);
    return (int)ret;
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
    ipv67_asn_user_t claim;
    uint8_t seed[IPV67_SEED_SIZE];
    unsigned char auth_key[IPV67_AUTH_KEY_SIZE];
    long ret;
    int i;
    unsigned int local_asn;
    int identity_loaded;

    if (!cfg) return -1;
    local_asn = 0;
    identity_loaded = 0;
    if (cfg->port > 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_PORT;
        req.arg1 = (unsigned long long)(unsigned int)cfg->port;
        ret = ipv67_syscall(&req);
        if (ret < 0) return (int)ret;
    }
    if (cfg->authkey[0]) {
        if (parse_hex_key(cfg->authkey, auth_key, IPV67_AUTH_KEY_SIZE) < 0) return -1;
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_AUTH_KEY;
        req.arg1 = (unsigned long long)(long)auth_key;
        req.arg2 = IPV67_AUTH_KEY_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) return (int)ret;
    }
    if (cfg->keyfile[0] && ipv67_load_seed_from(cfg->keyfile, seed) == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
        req.arg1 = (unsigned long long)(long)seed;
        req.arg2 = IPV67_SEED_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) return (int)ret;
        identity_loaded = 1;
    }
    memset(&req, 0, sizeof(req));
    req.cmd = IPV67_CMD_SET_AUTH_REQUIRED;
    req.arg1 = (cfg->auth_required || (!cfg->auth_required_set && identity_loaded)) ? 1 : 0;
    ret = ipv67_syscall(&req);
    if (ret < 0) return (int)ret;
    for (i = 0; i < cfg->asn_count; i++) {
        memcpy(&claim, &cfg->asns[i], sizeof(claim));
        if (claim.asn == 0) {
            if (local_asn == 0) {
                memset(&req, 0, sizeof(req));
                req.cmd = IPV67_CMD_GET_LOCAL_ASN;
                ret = ipv67_syscall(&req);
                if (ret <= 0) return (int)ret;
                local_asn = (unsigned int)ret;
            }
            claim.asn = local_asn;
        }
        ret = install_asn_claim(&claim);
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
    ipv67_peer_user_t *peers;
    ipv67_route_user_t *routes;
    ipv67_asn_user_t *asns;
    ipv67_asn_user_t asn_claim;
    ipv67_stats_user_t stats;
    ipv67_trace_result_user_t trace_result;
    long ret;
    int count;
    int i;
    int port_val;
    const char *keyfile;
    ipv67_addr_t addr;
    uint8_t seed[IPV67_SEED_SIZE];
    unsigned char auth_key[IPV67_AUTH_KEY_SIZE];
    long gr;
    int match;
    int endpoint_ret;
    ipv67_endpoint_t endpoint;
    const char *config_path;
    static ipv67_config_t cfg;
    static char endpoint_text[300];
    ipv67_syscall_req_t probe_req;
    int config_changed;
    int show_conflicts_only;
    int peer_total;
    int peer_auth;
    int peer_sessions;
    int peer_unresolved;
    int peer_remote;
    int peer_remote_auth;
    int peer_remote_sessions;
    int peer_remote_auth_sessions;
    int peer_remote_verified_sessions;
    int peer_remote_unverified_sessions;
    int peer_loopback;
    int peer_nonloopback;
    int peer_nonloopback_auth_sessions;
    int peer_nonloopback_verified_sessions;
    int peer_nonloopback_unverified_sessions;
    int peer_self;
    int peer_is_remote;
    int peer_is_loopback;
    int route_total;
    int route_multihop;
    int route_remote;
    int route_loopback;
    int route_nonloopback;
    int route_owner_keys;
    int route_nonloopback_owner_keys;
    int asn_total;
    int asn_auth;
    int asn_conflicts;
    int asn_usable_ownership;
    int asn_metadata_only;
    int check_failures;
    int check_warnings;
    int auth_required;
    int wait_tries;
    int wait_found;
    int ensure_key;
    int key_generated;
    unsigned int derived_asn;
    const char *asn_use_mode;
    ipv67_addr_t range_start;
    ipv67_addr_t range_end;

    peers = NULL;
    routes = NULL;
    asns = NULL;

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

    if (strcmp(argv[1], "shutdown") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SHUTDOWN;
        ret = ipv67_syscall(&req);
        if (ret < 0 && ret != -3) {
            fprintf(stderr, "ipv67cli: shutdown failed (%ld)\n", ret);
            return 1;
        }
        puts("IPv67 runtime instance freed.");
        return 0;
    }

    if (strcmp(argv[1], "smoke-asn") == 0) {
        if (!is_asn_text("676141")) {
            fputs("ipv67cli: ASN typed-number rejection self-test failed\n", stderr);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_INIT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: reset failed (%ld)\n", ret);
            return 1;
        }
        gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)seed, (long)IPV67_SEED_SIZE, 0);
        if (gr != IPV67_SEED_SIZE) {
            fputs("ipv67cli: smoke-asn: getrandom failed\n", stderr);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
        req.arg1 = (unsigned long long)(long)seed;
        req.arg2 = IPV67_SEED_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: failed to install identity key (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_AUTH_REQUIRED;
        req.arg1 = 1;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: failed to require auth (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_LOCAL_ASN;
        ret = ipv67_syscall(&req);
        if (ret <= 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: failed to derive ASN (%ld)\n", ret);
            return 1;
        }
        derived_asn = (unsigned int)ret;
        memset(&asn_claim, 0, sizeof(asn_claim));
        asn_claim.asn = derived_asn;
        if (build_asn_prefix("1928ab.676141.single.000001.*", &asn_claim) < 0) {
            fputs("ipv67cli: smoke-asn: failed to build ASN prefix\n", stderr);
            return 1;
        }
        asn_claim.flags = IPV67_ASN_FLAG_LOCAL;
        copy_token(asn_claim.country, sizeof(asn_claim.country), "SE");
        copy_token(asn_claim.name, sizeof(asn_claim.name), "SingleMachine");
        copy_token(asn_claim.label, sizeof(asn_claim.label), "LocalASN");
        ret = install_asn_claim(&asn_claim);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: failed to install ASN claim (%ld)\n", ret);
            return 1;
        }
        memset(addr_buf, 0, sizeof(addr_buf));
        ret = random_asn_address_from_claim_cli(&asn_claim, addr_buf);
        if (ret < 0) {
            fputs("ipv67cli: smoke-asn: failed to allocate random ASN address\n", stderr);
            return 1;
        }
        ret = parse_ipv67_addr(addr_buf, &addr);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: invalid generated address (%ld)\n", ret);
            return 1;
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: set address failed (%ld)\n", ret);
            return 1;
        }
        printf("AS%u\n", derived_asn);
        printf("ASN %u prefix saved: %s - %s\n", derived_asn, asn_claim.start_str, asn_claim.end_str);
        printf("IPv67 address set from ASN %u: %s\n", derived_asn, addr_buf);
        printf("IPv67 address: %s\n", addr_buf);
        printf("  AS%u %s-%s country=%s name=%s label=%s specificity=%u local=yes auth=yes\n",
               derived_asn,
               asn_claim.start_str,
               asn_claim.end_str,
               asn_claim.country,
               asn_claim.name,
               asn_claim.label,
               (unsigned int)asn_claim.specificity);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PING;
        req.arg1 = (unsigned long long)(long)&addr;
        req.arg2 = 3000;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: smoke-asn: ping self failed (%ld)\n", ret);
            return 1;
        }
        printf("Reply from %s: time=%ld ms\n", addr_buf, ret);
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

    if (strcmp(argv[1], "addrraw") == 0) {
        memset(&req, 0, sizeof(req));
        memset(&addr, 0, sizeof(addr));
        memset(addr_buf, 0, sizeof(addr_buf));
        req.cmd = IPV67_CMD_GET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) return 1;
        ret = format_ipv67_addr(&addr, addr_buf);
        if (ret < 0 || addr_buf[0] == '.') return 1;
        puts(addr_buf);
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
            if (ret == -1) fputs("ipv67cli: address is not inside a usable ASN ownership claim; use ipv67cli asn use\n", stderr);
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
        printf("Auth key: %s\n", cfg.authkey[0] ? "configured" : "(none)");
        printf("Auth required: %s\n", cfg.auth_required ? "yes" : "no");
        printf("Address: %s\n", cfg.address[0] ? cfg.address : "(not set)");
        printf("Port: %d\n", cfg.port);
        printf("Probe interval: %d ms\n", cfg.probe_interval);
        if (cfg.peer_count == 0) {
            puts("Peers: none");
        } else {
            printf("Peers (%d):\n", cfg.peer_count);
            for (i = 0; i < cfg.peer_count; i++) {
                printf("  %s\n", cfg.peer_endpoint[i]);
            }
        }
        if (cfg.asn_count == 0) {
            puts("ASNs: none");
        } else {
            printf("ASNs (%d):\n", cfg.asn_count);
            for (i = 0; i < cfg.asn_count; i++) {
                if (cfg.asns[i].asn != 0) printf("  AS%u ", cfg.asns[i].asn);
                else printf("  AS(identity) ");
                printf("%s-%s country=%s name=%s label=%s\n",
                       cfg.asns[i].start_str,
                       cfg.asns[i].end_str,
                       cfg.asns[i].country[0] ? cfg.asns[i].country : "XX",
                       cfg.asns[i].name[0] ? cfg.asns[i].name : "custom",
                       cfg.asns[i].label[0] ? cfg.asns[i].label : "claim");
            }
        }
        return 0;
    }

    if (strcmp(argv[1], "peeraddr") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PEER_COUNT;
        ret = ipv67_syscall(&req);
        if (ret <= 0) return 1;
        count = (int)ret;
        peers = (ipv67_peer_user_t *)malloc(sizeof(ipv67_peer_user_t) * count);
        if (!peers) return 1;
        memset(peers, 0, sizeof(ipv67_peer_user_t) * count);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PEERS;
        req.arg1 = (unsigned long long)(long)peers;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            free(peers);
            return 1;
        }
        if (ret > count) ret = count;
        for (i = 0; i < (int)ret; i++) {
            peers[i].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
            if (!peers[i].authenticated || !peers[i].session_established || !peers[i].addr_verified) continue;
            if (strcmp(peers[i].addr_str, "....") == 0 || peers[i].addr_str[0] == '.') continue;
            puts(peers[i].addr_str);
            free(peers);
            return 0;
        }
        free(peers);
        return 1;
    }

    if (strcmp(argv[1], "waitpeer") == 0) {
        wait_tries = 10;
        if (argc >= 3) {
            wait_tries = atoi(argv[2]);
            if (wait_tries < 1) wait_tries = 1;
            if (wait_tries > 60) wait_tries = 60;
        }
        wait_found = 0;
        for (i = 0; i < wait_tries && !wait_found; i++) {
            memset(&probe_req, 0, sizeof(probe_req));
            probe_req.cmd = IPV67_CMD_PROBE_PEERS;
            ipv67_syscall(&probe_req);
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_PEER_COUNT;
            ret = ipv67_syscall(&req);
            if (ret > 0) {
                count = (int)ret;
                peers = (ipv67_peer_user_t *)malloc(sizeof(ipv67_peer_user_t) * count);
                if (!peers) {
                    fputs("ipv67cli: out of memory\n", stderr);
                    return 1;
                }
                memset(peers, 0, sizeof(ipv67_peer_user_t) * count);
                memset(&req, 0, sizeof(req));
                req.cmd = IPV67_CMD_GET_PEERS;
                req.arg1 = (unsigned long long)(long)peers;
                req.arg2 = (unsigned long long)((unsigned int)count);
                ret = ipv67_syscall(&req);
                if (ret > count) ret = count;
                if (ret > 0) {
                    for (port_val = 0; port_val < (int)ret; port_val++) {
                        peers[port_val].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
                        if (!peers[port_val].authenticated || !peers[port_val].session_established || !peers[port_val].addr_verified) continue;
                        if (strcmp(peers[port_val].addr_str, "....") == 0 || peers[port_val].addr_str[0] == '.') continue;
                        puts(peers[port_val].addr_str);
                        wait_found = 1;
                        break;
                    }
                }
                free(peers);
                peers = NULL;
            }
            if (!wait_found) leb_syscall1(LEB_SYSCALL_SLEEP, 1000);
        }
        if (!wait_found) {
            fputs("ipv67cli: no authenticated verified peer available\n", stderr);
            return 1;
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
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_AUTH_REQUIRED;
        ret = ipv67_syscall(&req);
        if (ret < 0) printf("Auth required: unknown\n");
        else printf("Auth required: %s\n", ret ? "yes" : "no");
        memset(&req, 0, sizeof(req));
        memset(&stats, 0, sizeof(stats));
        req.cmd = IPV67_CMD_GET_STATS;
        req.arg1 = (unsigned long long)(long)&stats;
        ret = ipv67_syscall(&req);
        if (ret == 0) {
            printf("RX packets: %llu\n", stats.rx_packets);
            printf("TX packets: %llu\n", stats.tx_packets);
            printf("Forwarded packets: %llu\n", stats.forwarded_packets);
            printf("Replay drops: %llu\n", stats.replay_drops);
            printf("Auth drops: %llu\n", stats.auth_drops);
            printf("No-route drops: %llu\n", stats.no_route_drops);
            printf("Malformed drops: %llu\n", stats.malformed_drops);
            printf("Route updates: %llu\n", stats.route_updates);
            printf("ASN claims rx: %llu\n", stats.asn_claims_rx);
            printf("ASN claims tx: %llu\n", stats.asn_claims_tx);
            printf("Punch requests tx: %llu\n", stats.punch_requests_tx);
            printf("Punch observed tx: %llu\n", stats.punch_observed_tx);
            printf("Punch packets rx: %llu\n", stats.punch_packets_rx);
            printf("Punch peers learned: %llu\n", stats.punch_peers_learned);
            printf("Auth hello rx: %llu\n", stats.auth_hello_rx);
            printf("Auth reply rx: %llu\n", stats.auth_reply_rx);
            printf("Auth done rx: %llu\n", stats.auth_done_rx);
            printf("Auth payload ok: %llu\n", stats.auth_payload_ok);
            printf("Auth payload fail: %llu\n", stats.auth_payload_fail);
            printf("Auth sessions: %llu\n", stats.auth_sessions);
            printf("Auth fail self: %llu\n", stats.auth_fail_self);
            printf("Auth fail challenge: %llu\n", stats.auth_fail_challenge);
            printf("Auth fail shared: %llu\n", stats.auth_fail_shared);
            printf("Auth fail identity: %llu\n", stats.auth_fail_identity);
            printf("Auth session fail: %llu\n", stats.auth_session_fail);
        }
        printf("Configured peers: %d\n", cfg.peer_count);
        printf("Probe interval: %d ms\n", cfg.probe_interval);
        return 0;
    }

    if (strcmp(argv[1], "stats") == 0) {
        memset(&req, 0, sizeof(req));
        if (argc >= 3 && strcmp(argv[2], "clear") == 0) {
            req.cmd = IPV67_CMD_CLEAR_STATS;
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: clear stats failed (%ld)\n", ret);
                return 1;
            }
            puts("IPv67 counters cleared.");
            return 0;
        }
        memset(&stats, 0, sizeof(stats));
        req.cmd = IPV67_CMD_GET_STATS;
        req.arg1 = (unsigned long long)(long)&stats;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get stats failed (%ld)\n", ret);
            return 1;
        }
        printf("RX packets: %llu\n", stats.rx_packets);
        printf("TX packets: %llu\n", stats.tx_packets);
        printf("Forwarded packets: %llu\n", stats.forwarded_packets);
        printf("Replay drops: %llu\n", stats.replay_drops);
        printf("Auth drops: %llu\n", stats.auth_drops);
        printf("No-route drops: %llu\n", stats.no_route_drops);
        printf("Malformed drops: %llu\n", stats.malformed_drops);
        printf("Route updates: %llu\n", stats.route_updates);
        printf("ASN claims rx: %llu\n", stats.asn_claims_rx);
        printf("ASN claims tx: %llu\n", stats.asn_claims_tx);
        printf("Punch requests tx: %llu\n", stats.punch_requests_tx);
        printf("Punch observed tx: %llu\n", stats.punch_observed_tx);
        printf("Punch packets rx: %llu\n", stats.punch_packets_rx);
        printf("Punch peers learned: %llu\n", stats.punch_peers_learned);
        printf("Auth hello rx: %llu\n", stats.auth_hello_rx);
        printf("Auth reply rx: %llu\n", stats.auth_reply_rx);
        printf("Auth done rx: %llu\n", stats.auth_done_rx);
        printf("Auth payload ok: %llu\n", stats.auth_payload_ok);
        printf("Auth payload fail: %llu\n", stats.auth_payload_fail);
        printf("Auth sessions: %llu\n", stats.auth_sessions);
        printf("Auth fail self: %llu\n", stats.auth_fail_self);
        printf("Auth fail challenge: %llu\n", stats.auth_fail_challenge);
        printf("Auth fail shared: %llu\n", stats.auth_fail_shared);
        printf("Auth fail identity: %llu\n", stats.auth_fail_identity);
        printf("Auth session fail: %llu\n", stats.auth_session_fail);
        return 0;
    }

    if (strcmp(argv[1], "check") == 0) {
        peer_total = 0;
        peer_auth = 0;
        peer_sessions = 0;
        peer_unresolved = 0;
        peer_remote = 0;
        peer_remote_auth = 0;
        peer_remote_sessions = 0;
        peer_remote_auth_sessions = 0;
        peer_remote_verified_sessions = 0;
        peer_remote_unverified_sessions = 0;
        peer_loopback = 0;
        peer_nonloopback = 0;
        peer_nonloopback_auth_sessions = 0;
        peer_nonloopback_verified_sessions = 0;
        peer_nonloopback_unverified_sessions = 0;
        peer_self = 0;
        route_total = 0;
        route_multihop = 0;
        route_remote = 0;
        route_loopback = 0;
        route_nonloopback = 0;
        route_owner_keys = 0;
        route_nonloopback_owner_keys = 0;
        asn_total = 0;
        asn_auth = 0;
        asn_conflicts = 0;
        asn_usable_ownership = 0;
        asn_metadata_only = 0;
        check_failures = 0;
        check_warnings = 0;
        auth_required = 0;
        memset(&stats, 0, sizeof(stats));

        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_AUTH_REQUIRED;
        ret = ipv67_syscall(&req);
        if (ret >= 0) auth_required = ret ? 1 : 0;

        addr_buf[0] = '\0';
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_ADDR;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret >= 0 && format_ipv67_addr(&addr, addr_buf) < 0) addr_buf[0] = '\0';
        if (addr_buf[0] == '.') addr_buf[0] = '\0';

        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PEER_COUNT;
        ret = ipv67_syscall(&req);
        if (ret >= 0) peer_total = (int)ret;
        if (peer_total > 0) {
            peers = (ipv67_peer_user_t *)malloc(sizeof(ipv67_peer_user_t) * peer_total);
            if (!peers) {
                fputs("ipv67cli: out of memory\n", stderr);
                return 1;
            }
            memset(peers, 0, sizeof(ipv67_peer_user_t) * peer_total);
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_GET_PEERS;
            req.arg1 = (unsigned long long)(long)peers;
            req.arg2 = (unsigned long long)((unsigned int)peer_total);
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                free(peers);
                fprintf(stderr, "ipv67cli: get_peers failed (%ld)\n", ret);
                return 1;
            }
            if (ret > peer_total) ret = peer_total;
            for (i = 0; i < (int)ret; i++) {
                peers[i].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
                peer_is_remote = 0;
                peer_is_loopback = endpoint_is_loopback_cli(peers[i].family, peers[i].ipv4, peers[i].ipv6);
                if (peer_is_loopback) peer_loopback++;
                else peer_nonloopback++;
                if (peers[i].authenticated) peer_auth++;
                if (peers[i].session_established) peer_sessions++;
                if (strcmp(peers[i].addr_str, "....") == 0 || peers[i].addr_str[0] == '.') peer_unresolved++;
                else if (addr_buf[0] && strcmp(peers[i].addr_str, addr_buf) == 0) peer_self++;
                else {
                    peer_remote++;
                    peer_is_remote = 1;
                }
                if (peer_is_remote && peers[i].authenticated) peer_remote_auth++;
                if (peer_is_remote && peers[i].session_established) peer_remote_sessions++;
                if (peer_is_remote && peers[i].authenticated && peers[i].session_established) peer_remote_auth_sessions++;
                if (peer_is_remote && peers[i].authenticated && peers[i].session_established && peers[i].addr_verified) peer_remote_verified_sessions++;
                if (peer_is_remote && peers[i].authenticated && peers[i].session_established && !peers[i].addr_verified) peer_remote_unverified_sessions++;
                if (peer_is_remote && !peer_is_loopback && peers[i].authenticated && peers[i].session_established) peer_nonloopback_auth_sessions++;
                if (peer_is_remote && !peer_is_loopback && peers[i].authenticated && peers[i].session_established && peers[i].addr_verified) peer_nonloopback_verified_sessions++;
                if (peer_is_remote && !peer_is_loopback && peers[i].authenticated && peers[i].session_established && !peers[i].addr_verified) peer_nonloopback_unverified_sessions++;
            }
            free(peers);
            peers = NULL;
        }

        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_ROUTE_COUNT;
        ret = ipv67_syscall(&req);
        if (ret >= 0) route_total = (int)ret;
        if (route_total > 0) {
            routes = (ipv67_route_user_t *)malloc(sizeof(ipv67_route_user_t) * route_total);
            if (!routes) {
                fputs("ipv67cli: out of memory\n", stderr);
                return 1;
            }
            memset(routes, 0, sizeof(ipv67_route_user_t) * route_total);
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_GET_ROUTES;
            req.arg1 = (unsigned long long)(long)routes;
            req.arg2 = (unsigned long long)((unsigned int)route_total);
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                free(routes);
                fprintf(stderr, "ipv67cli: get_routes failed (%ld)\n", ret);
                return 1;
            }
            if (ret > route_total) ret = route_total;
            for (i = 0; i < (int)ret; i++) {
                routes[i].dest_str[IPV67_ADDR_STR_MAX - 1] = '\0';
                if (routes[i].hops > 1 || routes[i].metric > 1) route_multihop++;
                if (!addr_buf[0] || strcmp(routes[i].dest_str, addr_buf) != 0) route_remote++;
                if (key_present_cli(routes[i].public_key)) route_owner_keys++;
                if (endpoint_is_loopback_cli(routes[i].next_hop_family, routes[i].next_hop_ipv4, routes[i].next_hop_ipv6)) route_loopback++;
                else {
                    route_nonloopback++;
                    if (key_present_cli(routes[i].public_key)) route_nonloopback_owner_keys++;
                }
            }
            free(routes);
            routes = NULL;
        }

        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_ASN_COUNT;
        ret = ipv67_syscall(&req);
        if (ret >= 0) asn_total = (int)ret;
        if (asn_total > 0) {
            asns = (ipv67_asn_user_t *)malloc(sizeof(ipv67_asn_user_t) * asn_total);
            if (!asns) {
                fputs("ipv67cli: out of memory\n", stderr);
                return 1;
            }
            memset(asns, 0, sizeof(ipv67_asn_user_t) * asn_total);
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_GET_ASNS;
            req.arg1 = (unsigned long long)(long)asns;
            req.arg2 = (unsigned long long)((unsigned int)asn_total);
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                free(asns);
                fprintf(stderr, "ipv67cli: get_asns failed (%ld)\n", ret);
                return 1;
            }
            if (ret > asn_total) ret = asn_total;
            for (i = 0; i < (int)ret; i++) {
                if (asns[i].flags & IPV67_ASN_FLAG_AUTH) asn_auth++;
                if (asns[i].conflict_count) asn_conflicts++;
                if ((asns[i].flags & IPV67_ASN_FLAG_AUTH) && asn_claim_grants_ownership_cli(&asns[i])) asn_usable_ownership++;
                else asn_metadata_only++;
            }
            free(asns);
            asns = NULL;
        }

        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_STATS;
        req.arg1 = (unsigned long long)(long)&stats;
        ret = ipv67_syscall(&req);
        if (ret < 0) memset(&stats, 0, sizeof(stats));

        puts("IPv67 mesh readiness:");
        if (peer_total > 0) printf("  peer discovery: pass peers=%d unresolved=%d loopback=%d non_loopback=%d\n", peer_total, peer_unresolved, peer_loopback, peer_nonloopback);
        else {
            puts("  peer discovery: fail peers=0");
            check_failures++;
        }
        if (peer_unresolved > 0) check_warnings++;
        if (peer_nonloopback_verified_sessions > 0) printf("  cross-machine peer evidence: pass non_loopback_verified_sessions=%d non_loopback_auth_sessions=%d remote_peers=%d loopback_peers=%d self_peers=%d\n", peer_nonloopback_verified_sessions, peer_nonloopback_auth_sessions, peer_remote, peer_loopback, peer_self);
        else {
            printf("  cross-machine peer evidence: fail non_loopback_verified_sessions=0 non_loopback_auth_sessions=%d remote_peers=%d loopback_peers=%d self_peers=%d\n", peer_nonloopback_auth_sessions, peer_remote, peer_loopback, peer_self);
            check_failures++;
        }
        if (peer_remote_unverified_sessions > 0) {
            printf("  peer address verification: warn remote_unverified_sessions=%d non_loopback_unverified_sessions=%d\n", peer_remote_unverified_sessions, peer_nonloopback_unverified_sessions);
            check_warnings++;
        } else {
            printf("  peer address verification: pass remote_unverified_sessions=0 verified_sessions=%d\n", peer_remote_verified_sessions);
        }

        if (!auth_required) {
            printf("  auth policy: fail required=no remote_auth_sessions=%d sessions=%d authenticated=%d\n", peer_remote_auth_sessions, peer_sessions, peer_auth);
            check_failures++;
        } else {
            puts("  auth policy: pass required=yes");
        }
        if (peer_nonloopback_verified_sessions > 0 && auth_required) printf("  peer authentication: pass non_loopback_verified_sessions=%d non_loopback_session_key_sessions=%d remote_verified_sessions=%d remote_session_key_sessions=%d remote_sessions=%d remote_authenticated=%d sessions=%d authenticated=%d\n", peer_nonloopback_verified_sessions, peer_nonloopback_auth_sessions, peer_remote_verified_sessions, peer_remote_auth_sessions, peer_remote_sessions, peer_remote_auth, peer_sessions, peer_auth);
        else if (auth_required) {
            printf("  peer authentication: fail required=yes non_loopback_verified_sessions=0 non_loopback_session_key_sessions=%d remote_sessions=%d authenticated=%d\n", peer_nonloopback_auth_sessions, peer_remote_sessions, peer_auth);
            check_failures++;
        }

        if (route_total > 0 && peer_nonloopback_verified_sessions > 0 && route_owner_keys == route_total) printf("  routing table: pass routes=%d owner_keys=%d multihop=%d non_loopback_routes=%d non_loopback_owner_keys=%d loopback_routes=%d verified_non_loopback_sessions=%d\n", route_total, route_owner_keys, route_multihop, route_nonloopback, route_nonloopback_owner_keys, route_loopback, peer_nonloopback_verified_sessions);
        else if (route_total > 0) {
            printf("  routing table: fail routes=%d owner_keys=%d multihop=%d non_loopback_routes=%d non_loopback_owner_keys=%d verified_non_loopback_sessions=%d\n", route_total, route_owner_keys, route_multihop, route_nonloopback, route_nonloopback_owner_keys, peer_nonloopback_verified_sessions);
            check_failures++;
        }
        else {
            puts("  routing table: fail routes=0");
            check_failures++;
        }
        if (route_remote > 0 && route_nonloopback > 0 && route_nonloopback_owner_keys > 0) printf("  remote route evidence: pass remote_routes=%d non_loopback_routes=%d non_loopback_owner_keys=%d loopback_routes=%d\n", route_remote, route_nonloopback, route_nonloopback_owner_keys, route_loopback);
        else {
            printf("  remote route evidence: fail remote_routes=%d non_loopback_routes=%d non_loopback_owner_keys=%d loopback_routes=%d\n", route_remote, route_nonloopback, route_nonloopback_owner_keys, route_loopback);
            check_failures++;
        }
        if (route_multihop == 0) {
            puts("  routed mesh evidence: warn no multi-hop routes learned yet");
            check_warnings++;
        } else {
            puts("  routed mesh evidence: pass multi-hop routes learned");
        }

        if (stats.forwarded_packets > 0) printf("  forwarding evidence: pass forwarded=%llu\n", stats.forwarded_packets);
        else {
            puts("  forwarding evidence: warn forwarded=0");
            check_warnings++;
        }
        if (stats.no_route_drops || stats.auth_drops || stats.replay_drops) {
            printf("  drop counters: warn no_route=%llu auth=%llu replay=%llu\n", stats.no_route_drops, stats.auth_drops, stats.replay_drops);
            check_warnings++;
        } else {
            puts("  drop counters: pass no_route=0 auth=0 replay=0");
        }

        if (asn_total > 0) printf("  ASN claims: pass claims=%d authenticated=%d usable_ownership=%d metadata_only=%d conflicts=%d\n", asn_total, asn_auth, asn_usable_ownership, asn_metadata_only, asn_conflicts);
        else {
            puts("  ASN claims: warn claims=0");
            check_warnings++;
        }
        if (asn_total > 0 && asn_usable_ownership == 0) {
            puts("  ASN ownership: warn no usable authenticated ownership claims");
            check_warnings++;
        }
        if (asn_total > 0 && asn_auth == 0) {
            puts("  ASN authentication: warn no authenticated ASN claims");
            check_warnings++;
        }
        if (asn_conflicts > 0) {
            puts("  ASN conflicts: warn conflicts present");
            check_warnings++;
        }
        if (stats.punch_requests_tx || stats.punch_observed_tx || stats.punch_packets_rx || stats.punch_peers_learned) {
            printf("  NAT punch evidence: pass requests_tx=%llu observed_tx=%llu packets_rx=%llu peers_learned=%llu\n", stats.punch_requests_tx, stats.punch_observed_tx, stats.punch_packets_rx, stats.punch_peers_learned);
        } else {
            puts("  NAT punch evidence: warn no punch packets observed");
            check_warnings++;
        }
        if (cfg.peer_count > 0 && cfg.probe_interval > 0) printf("  NAT refresh: pass configured_peers=%d probe_interval=%dms\n", cfg.peer_count, cfg.probe_interval);
        else {
            puts("  NAT refresh: warn no configured peers or probe interval");
            check_warnings++;
        }
        printf("Summary: failures=%d warnings=%d\n", check_failures, check_warnings);
        return check_failures ? 1 : 0;
    }

    if (strcmp(argv[1], "testplan") == 0) {
        puts("IPv67 verification:");
        puts("  1. On every machine:");
        puts("     ipv67cli keygen");
        puts("     ipv67cli asn prefix <owned-prefix> country <cc> name <name> label <label>");
        puts("     Example A: ipv67cli asn prefix 1928ab.676141.testa.000001.* country SE name TestA label TestA");
        puts("     Example B: ipv67cli asn prefix 1928ab.676142.testb.000001.* country SE name TestB label TestB");
        puts("     Do not type an ASN number; ipv67cli derives it from the identity key.");
        puts("     ipv67cli asn use");
        puts("  2. For direct peer/auth evidence, add each of two machines from the other:");
        puts("     ipv67cli addpeer <other-machine-ip>:6767");
        puts("  3. For routed mesh evidence, use three machines A-B-C:");
        puts("     On A: ipv67cli addpeer <B-ip>:6767");
        puts("     On B: ipv67cli addpeer <A-ip>:6767");
        puts("     On B: ipv67cli addpeer <C-ip>:6767");
        puts("     On C: ipv67cli addpeer <B-ip>:6767");
        puts("     Do not add A directly to C for this routed test.");
        puts("  4. Run the daemon on every machine:");
        puts("     ipv67d");
        puts("  5. Inspect evidence:");
        puts("     ipv67cli peers");
        puts("     ipv67cli routes");
        puts("     ipv67cli asns");
        puts("     ipv67cli check");
        puts("  6. Routed/NAT probes:");
        puts("     On A: ipv67cli trace <C-ipv67>");
        puts("     On A: ipv67cli ping <C-ipv67>");
        puts("     ipv67cli trace <remote-ipv67>");
        puts("     ipv67cli stats clear");
        puts("     ipv67cli punch <remote-ipv67>");
        puts("     ipv67cli stats");
        puts("     ipv67cli ping <remote-ipv67>");
        puts("Passing evidence requires authenticated and address-verified non-loopback sessions and non-loopback routes.");
        puts("Peer lines should show auth=yes session=yes addr_verified=yes for real remote peers.");
        puts("Loopback-only peers are useful for smoke tests, but ipv67cli check treats them as insufficient.");
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
        peers = (ipv67_peer_user_t *)malloc(sizeof(ipv67_peer_user_t) * count);
        if (!peers) {
            fputs("ipv67cli: out of memory\n", stderr);
            return 1;
        }
        memset(peers, 0, sizeof(ipv67_peer_user_t) * count);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PEERS;
        req.arg1 = (unsigned long long)(long)peers;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get_peers failed (%ld)\n", ret);
            free(peers);
            return 1;
        }
        if (ret > count) ret = count;
        for (i = 0; i < (int)ret; i++) {
            peers[i].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
            if (strcmp(peers[i].addr_str, "....") == 0 || peers[i].addr_str[0] == '.')
                printf("  unresolved  ");
            else
                printf("  %s  ", peers[i].addr_str);
            if (peers[i].family == IPV67_PEER_IPV6) {
                putchar('[');
                print_ipv6(peers[i].ipv6);
                printf("]:%u", (unsigned int)peers[i].port);
            } else {
                print_ipv4(peers[i].ipv4);
                printf(":%u", (unsigned int)peers[i].port);
            }
            peers[i].alias[IPV67_ALIAS_SIZE - 1] = '\0';
            if (peers[i].alias[0]) printf("  alias=%s", peers[i].alias);
            if (endpoint_is_loopback_cli(peers[i].family, peers[i].ipv4, peers[i].ipv6)) printf("  endpoint=loopback");
            if (peers[i].authenticated) printf("  auth=yes");
            else printf("  auth=no");
            if (peers[i].session_established) printf("  session=yes");
            else printf("  session=no");
            if (peers[i].addr_verified) printf("  addr_verified=yes");
            else printf("  addr_verified=no");
            if (peers[i].missed_probes) printf("  missed=%u", (unsigned int)peers[i].missed_probes);
            putchar('\n');
        }
        free(peers);
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
        routes = (ipv67_route_user_t *)malloc(sizeof(ipv67_route_user_t) * count);
        if (!routes) {
            fputs("ipv67cli: out of memory\n", stderr);
            return 1;
        }
        memset(routes, 0, sizeof(ipv67_route_user_t) * count);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_ROUTES;
        req.arg1 = (unsigned long long)(long)routes;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get_routes failed (%ld)\n", ret);
            free(routes);
            return 1;
        }
        if (ret > count) ret = count;
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
            if (endpoint_is_loopback_cli(routes[i].next_hop_family, routes[i].next_hop_ipv4, routes[i].next_hop_ipv6)) printf("  endpoint=loopback");
            print_key_tag_cli(routes[i].public_key);
            printf("  hops=%u metric=%u seq=%u\n", (unsigned int)routes[i].hops, (unsigned int)routes[i].metric, routes[i].sequence);
        }
        free(routes);
        return 0;
    }

    if (strcmp(argv[1], "asn") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: asn requires a subcommand\n", stderr);
            return 1;
        }
        if (strcmp(argv[2], "add") == 0) {
            fputs("ipv67cli: use 'asn prefix' or 'asn range' so the ASN has usable IPv67 space\n", stderr);
            return 1;
        }
        if (strcmp(argv[2], "id") == 0) {
            derived_asn = 0;
            ret = derive_local_asn_offline_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN id requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            if (ret < 0 || derived_asn == 0) {
                fputs("ipv67cli: failed to derive ASN id\n", stderr);
                return 1;
            }
            printf("AS%u\n", derived_asn);
            return 0;
        }
        if (strcmp(argv[2], "self") == 0) {
            memset(&asn_claim, 0, sizeof(asn_claim));
            derived_asn = 0;
            ret = derive_local_asn_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN self requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            asn_claim.asn = derived_asn;
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_GET_ADDR;
            req.arg1 = (unsigned long long)(long)&addr;
            ret = ipv67_syscall(&req);
            if (ret < 0 || asn_claim.asn == 0 || format_ipv67_addr(&addr, addr_buf) < 0 || addr_buf[0] == '.') {
                fputs("ipv67cli: no current IPv67 address to claim; use asn prefix/range and asn use first\n", stderr);
                return 1;
            }
            copy_token(asn_claim.start_str, IPV67_ADDR_STR_MAX, addr_buf);
            copy_token(asn_claim.end_str, IPV67_ADDR_STR_MAX, addr_buf);
            asn_claim.specificity = 160;
            asn_claim.flags = IPV67_ASN_FLAG_LOCAL;
            if (apply_asn_options(&asn_claim, argc, argv, 3) < 0) {
                fputs("ipv67cli: unknown ASN self option; use country <cc>, name <text>, or label <text>\n", stderr);
                return 1;
            }
            ret = install_asn_claim(&asn_claim);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: failed to install ASN claim (%ld)\n", ret);
                return 1;
            }
            if (config_add_asn(&cfg, &asn_claim) < 0) {
                fputs("ipv67cli: too many configured ASN claims\n", stderr);
                return 1;
            }
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            printf("ASN %u self address saved: %s\n", asn_claim.asn, addr_buf);
            return 0;
        }
        if (strcmp(argv[2], "prefix") == 0) {
            if (argc < 4) {
                fputs("ipv67cli: asn prefix requires <prefix>\n", stderr);
                return 1;
            }
            if (argc >= 4 && is_asn_text(argv[3])) {
                fputs("ipv67cli: do not type an ASN number here; your ASN is derived from your identity key. Use: asn prefix <prefix>\n", stderr);
                return 1;
            }
            memset(&asn_claim, 0, sizeof(asn_claim));
            derived_asn = 0;
            ret = derive_local_asn_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN claim requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            asn_claim.asn = derived_asn;
            if (ret < 0 || asn_claim.asn == 0 || build_asn_prefix(argv[3], &asn_claim) < 0) {
                fputs("ipv67cli: invalid ASN prefix; use zone1.zone2.domain.node1.* for usable address space\n", stderr);
                return 1;
            }
            asn_claim.flags = IPV67_ASN_FLAG_LOCAL;
            if (asn_claim_is_metadata_only_cli(&asn_claim)) asn_claim.flags |= IPV67_ASN_FLAG_BROAD;
            if (apply_asn_options(&asn_claim, argc, argv, 4) < 0) {
                fputs("ipv67cli: unknown ASN prefix option; use country <cc>, name <text>, or label <text>\n", stderr);
                return 1;
            }
            ret = install_asn_claim(&asn_claim);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: failed to install ASN claim (%ld)\n", ret);
                return 1;
            }
            if (config_add_asn(&cfg, &asn_claim) < 0) {
                fputs("ipv67cli: too many configured ASN claims\n", stderr);
                return 1;
            }
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            if (asn_claim.flags & IPV67_ASN_FLAG_BROAD) puts("Warning: broad ASN claim saved for metadata/conflicts only; use zone1.zone2.domain.node1.* or exact for addresses.");
            printf("ASN %u prefix saved: %s - %s\n", asn_claim.asn, asn_claim.start_str, asn_claim.end_str);
            return 0;
        }
        if (strcmp(argv[2], "range") == 0) {
            if (argc < 5) {
                fputs("ipv67cli: asn range requires <start> <end>\n", stderr);
                return 1;
            }
            if (argc >= 6 && is_asn_text(argv[3])) {
                fputs("ipv67cli: do not type an ASN number here; your ASN is derived from your identity key. Use: asn range <start> <end>\n", stderr);
                return 1;
            }
            memset(&asn_claim, 0, sizeof(asn_claim));
            derived_asn = 0;
            ret = derive_local_asn_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN claim requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            asn_claim.asn = derived_asn;
            if (ret < 0 || asn_claim.asn == 0 || parse_ipv67_addr(argv[3], &range_start) < 0 || parse_ipv67_addr(argv[4], &range_end) < 0) {
                fputs("ipv67cli: invalid ASN range\n", stderr);
                return 1;
            }
            if (addr_cmp_cli(&range_start, &range_end) > 0) {
                fputs("ipv67cli: invalid ASN range\n", stderr);
                return 1;
            }
            copy_token(asn_claim.start_str, IPV67_ADDR_STR_MAX, argv[3]);
            copy_token(asn_claim.end_str, IPV67_ADDR_STR_MAX, argv[4]);
            asn_claim.specificity = asn_range_specificity_cli(&range_start, &range_end);
            asn_claim.flags = IPV67_ASN_FLAG_LOCAL;
            if (asn_claim_is_metadata_only_cli(&asn_claim)) asn_claim.flags |= IPV67_ASN_FLAG_BROAD;
            if (apply_asn_options(&asn_claim, argc, argv, 5) < 0) {
                fputs("ipv67cli: unknown ASN range option; use country <cc>, name <text>, or label <text>\n", stderr);
                return 1;
            }
            ret = install_asn_claim(&asn_claim);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: failed to install ASN claim (%ld)\n", ret);
                return 1;
            }
            if (config_add_asn(&cfg, &asn_claim) < 0) {
                fputs("ipv67cli: too many configured ASN claims\n", stderr);
                return 1;
            }
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            if (asn_claim.flags & IPV67_ASN_FLAG_BROAD) puts("Warning: broad ASN range saved for metadata/conflicts only; use ranges inside one node1 value for addresses.");
            puts("ASN range saved.");
            return 0;
        }
        if (strcmp(argv[2], "use") == 0) {
            asn_use_mode = "random";
            if (argc >= 4) asn_use_mode = argv[3];
            memset(&asn_claim, 0, sizeof(asn_claim));
            if (argc > 4) {
                fputs("ipv67cli: asn use accepts at most one value: <ipv67addr>, auto, or random\n", stderr);
                return 1;
            }
            if (argc >= 4 && is_asn_text(asn_use_mode)) {
                fputs("ipv67cli: do not type an ASN number here; your ASN is derived from your identity key. Use: asn use [<ipv67addr>|auto|random]\n", stderr);
                return 1;
            }
            derived_asn = 0;
            ret = derive_local_asn_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN use requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            asn_claim.asn = derived_asn;
            if (asn_claim.asn == 0) {
                fputs("ipv67cli: invalid ASN\n", stderr);
                return 1;
            }
            memset(addr_buf, 0, sizeof(addr_buf));
            if (strcmp(asn_use_mode, "auto") == 0 || strcmp(asn_use_mode, "random") == 0) {
                for (i = 0; i < cfg.asn_count; i++) {
                    if ((cfg.asns[i].asn == asn_claim.asn || cfg.asns[i].asn == 0) && asn_claim_grants_ownership_cli(&cfg.asns[i])) {
                        if (strcmp(asn_use_mode, "random") == 0) ret = random_asn_address_from_claim_cli(&cfg.asns[i], addr_buf);
                        else ret = auto_asn_address_from_claim_cli(&cfg, asn_claim.asn, &cfg.asns[i], addr_buf);
                        if (ret == -2) {
                            if (strcmp(asn_use_mode, "random") == 0) fputs("ipv67cli: random ASN address requires getrandom\n", stderr);
                            else fprintf(stderr, "ipv67cli: auto ASN address requires an identity key at %s; run keygen first\n", cfg.keyfile);
                            return 1;
                        }
                        if (ret < 0) {
                            if (strcmp(asn_use_mode, "random") == 0) fputs("ipv67cli: failed to allocate random ASN address\n", stderr);
                            else fputs("ipv67cli: failed to derive automatic ASN address\n", stderr);
                            return 1;
                        }
                        break;
                    }
                }
                if (!addr_buf[0]) {
                    fputs("ipv67cli: no usable ASN ownership claim configured\n", stderr);
                    return 1;
                }
            } else {
                copy_token(addr_buf, IPV67_ADDR_STR_MAX, asn_use_mode);
            }
            ret = parse_ipv67_addr(addr_buf, &addr);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: invalid IPv67 address (%ld)\n", ret);
                return 1;
            }
            match = 0;
            for (i = 0; i < cfg.asn_count; i++) {
                if ((cfg.asns[i].asn == asn_claim.asn || cfg.asns[i].asn == 0) && asn_claim_grants_ownership_cli(&cfg.asns[i]) && addr_in_asn_claim(&addr, &cfg.asns[i])) match = 1;
            }
            if (!match) {
                fputs("ipv67cli: address is not inside a usable ownership claim for this ASN\n", stderr);
                return 1;
            }
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_SET_ADDR;
            req.arg1 = (unsigned long long)(long)&addr;
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: set address failed (%ld)\n", ret);
                return 1;
            }
            copy_token(cfg.address, sizeof(cfg.address), addr_buf);
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            printf("IPv67 address set from ASN %u: %s\n", asn_claim.asn, addr_buf);
            return 0;
        }
        if (strcmp(argv[2], "rm") == 0) {
            if (argc < 4) {
                fputs("ipv67cli: asn rm requires <prefix|start> [end]\n", stderr);
                return 1;
            }
            memset(&asn_claim, 0, sizeof(asn_claim));
            if (argc >= 5 && is_asn_text(argv[3])) {
                fputs("ipv67cli: do not type an ASN number here; your ASN is derived from your identity key. Use: asn rm <prefix|start> [end]\n", stderr);
                return 1;
            }
            derived_asn = 0;
            ret = derive_local_asn_cli(&cfg, &derived_asn);
            if (ret == -2) {
                fprintf(stderr, "ipv67cli: ASN remove requires an identity key at %s; run keygen first\n", cfg.keyfile);
                return 1;
            }
            asn_claim.asn = derived_asn;
            if (argc >= 5) {
                copy_token(asn_claim.start_str, IPV67_ADDR_STR_MAX, argv[3]);
                copy_token(asn_claim.end_str, IPV67_ADDR_STR_MAX, argv[4]);
            } else if (build_asn_prefix(argv[3], &asn_claim) < 0) {
                fputs("ipv67cli: invalid ASN claim\n", stderr);
                return 1;
            }
            if (config_remove_asn(&cfg, &asn_claim) < 0) {
                fputs("ipv67cli: ASN claim not configured\n", stderr);
                return 1;
            }
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_REMOVE_ASN;
            req.arg1 = (unsigned long long)(long)&asn_claim;
            ipv67_syscall(&req);
            if (save_config(config_path, &cfg) < 0) {
                fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
                return 1;
            }
            puts("ASN claim removed.");
            return 0;
        }
        if (strcmp(argv[2], "conflicts") != 0) {
            fputs("ipv67cli: unknown asn subcommand\n", stderr);
            return 1;
        }
    }

    if (strcmp(argv[1], "asns") == 0 || (strcmp(argv[1], "asn") == 0 && argc >= 3 && strcmp(argv[2], "conflicts") == 0)) {
        show_conflicts_only = strcmp(argv[1], "asn") == 0;
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_ASN_COUNT;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: asn_count failed (%ld)\n", ret);
            return 1;
        }
        count = (int)ret;
        if (count == 0) {
            puts(show_conflicts_only ? "No ASN conflicts." : "No known ASN claims.");
            return 0;
        }
        asns = (ipv67_asn_user_t *)malloc(sizeof(ipv67_asn_user_t) * count);
        if (!asns) {
            fputs("ipv67cli: out of memory\n", stderr);
            return 1;
        }
        memset(asns, 0, sizeof(ipv67_asn_user_t) * count);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_ASNS;
        req.arg1 = (unsigned long long)(long)asns;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: get_asns failed (%ld)\n", ret);
            free(asns);
            return 1;
        }
        if (ret > count) ret = count;
        for (i = 0; i < (int)ret; i++) {
            if (show_conflicts_only && asns[i].conflict_count == 0) continue;
            asns[i].name[IPV67_ASN_NAME_SIZE - 1] = '\0';
            asns[i].label[IPV67_ASN_LABEL_SIZE - 1] = '\0';
            asns[i].country[IPV67_ASN_COUNTRY_SIZE - 1] = '\0';
            asns[i].source[IPV67_ASN_SOURCE_SIZE - 1] = '\0';
            printf("  AS%u %s-%s country=%s name=%s label=%s source=%s specificity=%u",
                   asns[i].asn,
                   asns[i].start_str,
                   asns[i].end_str,
                   asns[i].country[0] ? asns[i].country : "XX",
                   asns[i].name[0] ? asns[i].name : "custom",
                   asns[i].label[0] ? asns[i].label : "claim",
                   asns[i].source[0] ? asns[i].source : "local",
                   (unsigned int)asns[i].specificity);
            if (asns[i].flags & IPV67_ASN_FLAG_LOCAL) printf(" local=yes");
            if (asns[i].flags & IPV67_ASN_FLAG_AUTH) printf(" auth=yes");
            if (asns[i].flags & IPV67_ASN_FLAG_RELAY) printf(" relay=yes");
            if (asns[i].flags & IPV67_ASN_FLAG_BROAD) printf(" broad=yes");
            if (asns[i].flags & IPV67_ASN_FLAG_QUARANTINE) printf(" usable=no reason=conflict");
            else if ((asns[i].flags & IPV67_ASN_FLAG_AUTH) && !(asns[i].flags & IPV67_ASN_FLAG_BROAD)) printf(" usable=yes");
            if (asns[i].conflict_count) printf(" conflicts=%u", (unsigned int)asns[i].conflict_count);
            putchar('\n');
        }
        free(asns);
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
                fputs("ipv67cli: no configured address; run ipv67cli asn prefix/range, then ipv67cli asn use\n", stderr);
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

    if (strcmp(argv[1], "pingpeer") == 0) {
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_PEER_COUNT;
        ret = ipv67_syscall(&req);
        if (ret <= 0) {
            fputs("ipv67cli: no authenticated verified peer available\n", stderr);
            return 1;
        }
        count = (int)ret;
        peers = (ipv67_peer_user_t *)malloc(sizeof(ipv67_peer_user_t) * count);
        if (!peers) {
            fputs("ipv67cli: out of memory\n", stderr);
            return 1;
        }
        memset(peers, 0, sizeof(ipv67_peer_user_t) * count);
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_GET_PEERS;
        req.arg1 = (unsigned long long)(long)peers;
        req.arg2 = (unsigned long long)((unsigned int)count);
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            free(peers);
            fprintf(stderr, "ipv67cli: get_peers failed (%ld)\n", ret);
            return 1;
        }
        if (ret > count) ret = count;
        addr_buf[0] = '\0';
        for (i = 0; i < (int)ret; i++) {
            peers[i].addr_str[IPV67_ADDR_STR_MAX - 1] = '\0';
            if (!peers[i].authenticated || !peers[i].session_established || !peers[i].addr_verified) continue;
            if (strcmp(peers[i].addr_str, "....") == 0 || peers[i].addr_str[0] == '.') continue;
            strncpy(addr_buf, peers[i].addr_str, sizeof(addr_buf) - 1);
            addr_buf[sizeof(addr_buf) - 1] = '\0';
            break;
        }
        free(peers);
        peers = NULL;
        if (!addr_buf[0]) {
            fputs("ipv67cli: no authenticated verified peer available\n", stderr);
            return 1;
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

    if (strcmp(argv[1], "trace") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: trace requires an IPv67 address\n", stderr);
            return 1;
        }
        strncpy(addr_buf, argv[2], sizeof(addr_buf) - 1);
        addr_buf[sizeof(addr_buf) - 1] = '\0';
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
        memset(&trace_result, 0, sizeof(trace_result));
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_TRACE;
        req.arg1 = (unsigned long long)(long)&addr;
        req.arg2 = (unsigned long long)(long)&trace_result;
        req.arg3 = 5000;
        ret = ipv67_syscall(&req);
        if (ret < 0 && trace_result.hop_count == 0) {
            if (ret == IPV67_ERR_TIMEOUT) {
                printf("Trace timeout for %s\n", addr_buf);
                return 1;
            }
            if (ret == -2) print_route_hint();
            fprintf(stderr, "ipv67cli: trace failed (%ld)\n", ret);
            return 1;
        }
        printf("Trace to %s%s:\n", addr_buf, trace_result.complete ? "" : " (partial)");
        for (i = 0; i < trace_result.hop_count && i < IPV67_TRACE_MAX_HOPS; i++) {
            trace_result.hops[i].addr[IPV67_ADDR_STR_MAX - 1] = '\0';
            trace_result.hops[i].alias[IPV67_ALIAS_SIZE - 1] = '\0';
            printf("  %u  %s", (unsigned int)trace_result.hops[i].hop, trace_result.hops[i].addr[0] ? trace_result.hops[i].addr : "(unknown)");
            if (trace_result.hops[i].alias[0]) printf("  alias=%s", trace_result.hops[i].alias);
            printf("  time=%u ms", trace_result.hops[i].rtt);
            if (trace_result.hops[i].final) printf("  final=yes");
            putchar('\n');
        }
        return trace_result.complete ? 0 : 1;
    }

    if (strcmp(argv[1], "punch") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: punch requires an IPv67 address\n", stderr);
            return 1;
        }
        strncpy(addr_buf, argv[2], sizeof(addr_buf) - 1);
        addr_buf[sizeof(addr_buf) - 1] = '\0';
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
        req.cmd = IPV67_CMD_PUNCH;
        req.arg1 = (unsigned long long)(long)&addr;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            if (ret == -2) print_route_hint();
            fprintf(stderr, "ipv67cli: punch failed (%ld)\n", ret);
            return 1;
        }
        printf("NAT punch requested for %s\n", addr_buf);
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
        ipv67_instance_port = (unsigned short)port_val;
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

    if (strcmp(argv[1], "probe-interval") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: probe-interval requires milliseconds\n", stderr);
            return 1;
        }
        port_val = atoi(argv[2]);
        if (port_val < 1000) {
            fputs("ipv67cli: probe interval must be at least 1000 ms\n", stderr);
            return 1;
        }
        cfg.probe_interval = port_val;
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("IPv67 probe interval set to %d ms\n", port_val);
        return 0;
    }

    if (strcmp(argv[1], "authkey") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: authkey requires a 64-character hex key or auto\n", stderr);
            return 1;
        }
        if (strcmp(argv[2], "auto") == 0) {
            gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)auth_key, (long)IPV67_AUTH_KEY_SIZE, 0);
            if (gr != IPV67_AUTH_KEY_SIZE) {
                fputs("ipv67cli: authkey auto: getrandom failed\n", stderr);
                return 1;
            }
            for (i = 0; i < IPV67_AUTH_KEY_SIZE; i++) {
                cfg.authkey[i * 2] = "0123456789abcdef"[auth_key[i] >> 4];
                cfg.authkey[i * 2 + 1] = "0123456789abcdef"[auth_key[i] & 15];
            }
            cfg.authkey[IPV67_AUTH_KEY_SIZE * 2] = '\0';
        } else {
            if (parse_hex_key(argv[2], auth_key, IPV67_AUTH_KEY_SIZE) < 0) {
                fputs("ipv67cli: invalid auth key\n", stderr);
                return 1;
            }
            copy_token(cfg.authkey, sizeof(cfg.authkey), argv[2]);
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_AUTH_KEY;
        req.arg1 = (unsigned long long)(long)auth_key;
        req.arg2 = IPV67_AUTH_KEY_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: authkey failed (%ld)\n", ret);
            return 1;
        }
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        puts("IPv67 shared authentication key configured.");
        return 0;
    }

    if (strcmp(argv[1], "auth-required") == 0) {
        if (argc < 3) {
            fputs("ipv67cli: auth-required requires on or off\n", stderr);
            return 1;
        }
        if (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "1") == 0 || strcmp(argv[2], "yes") == 0) {
            cfg.auth_required = 1;
            cfg.auth_required_set = 1;
        } else if (strcmp(argv[2], "off") == 0 || strcmp(argv[2], "0") == 0 || strcmp(argv[2], "no") == 0) {
            cfg.auth_required = 0;
            cfg.auth_required_set = 1;
        } else {
            fputs("ipv67cli: auth-required requires on or off\n", stderr);
            return 1;
        }
        if (cfg.auth_required && ipv67_load_seed_from(keyfile, seed) < 0) {
            fprintf(stderr, "ipv67cli: auth-required on needs an identity key at %s; run ipv67cli keygen first\n", keyfile);
            return 1;
        }
        if (cfg.auth_required) {
            memset(&req, 0, sizeof(req));
            req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
            req.arg1 = (unsigned long long)(long)seed;
            req.arg2 = IPV67_SEED_SIZE;
            ret = ipv67_syscall(&req);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: failed to install identity key (%ld)\n", ret);
                return 1;
            }
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_AUTH_REQUIRED;
        req.arg1 = cfg.auth_required ? 1 : 0;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: auth-required failed (%ld)\n", ret);
            return 1;
        }
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        printf("IPv67 authenticated packet requirement: %s\n", cfg.auth_required ? "on" : "off");
        return 0;
    }

    if (strcmp(argv[1], "keygen") == 0 || strcmp(argv[1], "keyensure") == 0) {
        ensure_key = strcmp(argv[1], "keyensure") == 0;
        key_generated = 0;
        if (!ensure_key || ipv67_load_seed_from(keyfile, seed) < 0) {
            key_generated = 1;
            gr = leb_syscall3(LEB_SYSCALL_GETRANDOM, (long)seed, (long)IPV67_SEED_SIZE, 0);
            if (gr != IPV67_SEED_SIZE) {
                fprintf(stderr, "ipv67cli: %s: getrandom failed\n", argv[1]);
                return 1;
            }
            if (ipv67_save_seed_to(keyfile, seed) < 0) {
                fprintf(stderr, "ipv67cli: %s: failed to save identity key to %s\n", argv[1], keyfile);
                return 1;
            }
        }
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_IDENTITY_KEY;
        req.arg1 = (unsigned long long)(long)seed;
        req.arg2 = IPV67_SEED_SIZE;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: %s: failed to install identity key (%ld)\n", argv[1], ret);
            return 1;
        }
        strncpy(cfg.keyfile, keyfile, sizeof(cfg.keyfile) - 1);
        cfg.keyfile[sizeof(cfg.keyfile) - 1] = '\0';
        cfg.address[0] = '\0';
        cfg.auth_required = 1;
        cfg.auth_required_set = 1;
        memset(&req, 0, sizeof(req));
        req.cmd = IPV67_CMD_SET_AUTH_REQUIRED;
        req.arg1 = 1;
        ret = ipv67_syscall(&req);
        if (ret < 0) {
            fprintf(stderr, "ipv67cli: %s: failed to enable authenticated packets (%ld)\n", argv[1], ret);
            return 1;
        }
        if (save_config(config_path, &cfg) < 0) {
            fprintf(stderr, "ipv67cli: failed to save config %s\n", config_path);
            return 1;
        }
        for (i = 0; i < cfg.peer_count; i++) {
            ret = add_peer_to_kernel(cfg.peer_endpoint[i]);
            if (ret < 0) {
                fprintf(stderr, "ipv67cli: %s: failed to reload peer %s (%ld)\n", argv[1], cfg.peer_endpoint[i], ret);
                return 1;
            }
        }
        if (ensure_key && !key_generated) printf("IPv67 identity key loaded.\n");
        else printf("IPv67 identity key generated.\n");
        puts("Address: not set; use ipv67cli asn prefix/range/self, then ipv67cli asn use");
        printf("Authenticated packets: on\n");
        return 0;
    }

    fprintf(stderr, "ipv67cli: unknown command '%s'\n", argv[1]);
    show_usage();
    return 1;
}

int main(int argc, char **argv) {
    return cmd_ipv67cli(argc, argv);
}
