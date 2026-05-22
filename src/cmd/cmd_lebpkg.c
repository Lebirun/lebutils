#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <lebirun.h>
#include "cu.h"

#define LEBPKG_CONF_DIR    "/etc/lebpkg"
#define LEBPKG_REPO_DIR    "/etc/lebpkg/repo"
#define LEBPKG_DB_DIR      "/etc/lebpkg/installed"
#define LEBPKG_IDX_DIR     "/etc/lebpkg/index"
#define MAX_RESP_SIZE      (128 * 1024)
#define MAX_URL_LEN        512
#define MAX_LINE_LEN       256
#define MAX_REPOS          8
#define MAX_PACKAGES       64
#define MAX_CATEGORIES     16
#define LPKG_MAGIC         "LPKG"
#define LPKG_VERSION       1
#define MAX_PKG_FILES      64
#define MAX_FILE_PATH      128

typedef struct {
    const char *url;
    uint8_t *buffer;
    uint64_t buffer_size;
    uint64_t *out_size;
    int *status_code;
    int max_redirects;
    uint8_t *headers_buf;
    uint64_t headers_buf_size;
    uint64_t *out_headers_len;
} __attribute__((packed)) http_get_req_t;

typedef struct {
    const char *url;
    uint64_t *out_buffer;
    uint64_t *out_size;
    int *status_code;
    int max_redirects;
    uint8_t *headers_buf;
    uint64_t headers_buf_size;
    uint64_t *out_headers_len;
} __attribute__((packed)) http_get_alloc_req_t;

typedef struct {
    char name[64];
    char version[32];
    char description[128];
    char author[64];
    char license[32];
    char depends[128];
    char arch[16];
    char category[32];
} pkg_entry_t;

typedef struct {
    char base_url[MAX_URL_LEN];
    char ver[32];
    char scope[32];
} repo_t;

static int load_all_index_pkgs(pkg_entry_t *pkgs, int max, char *buf, unsigned int bufsz);
static void ensure_parent_dirs(const char *filepath);

static int confirm_yn(const char *prompt) {
    char c;
    printf("%s [Y/n] ", prompt);
    fflush(stdout);
    c = (char)getchar();
    if (c == '\n' || c == 'Y' || c == 'y') return 1;
    while (c != '\n' && c != EOF) c = (char)getchar();
    return 0;
}

static int lookup_pkg_deps(const char *pkg, char *deps, int deps_size) {
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;

    deps[0] = '\0';
    buf = malloc(MAX_RESP_SIZE);
    if (!buf) return -1;
    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) { free(buf); return -1; }
    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    for (j = 0; j < npkgs; j++) {
        if (strcmp(pkgs[j].name, pkg) == 0) {
            if (pkgs[j].depends[0]) {
                strncpy(deps, pkgs[j].depends, (size_t)(deps_size - 1));
                deps[deps_size - 1] = '\0';
            }
            free(pkgs);
            free(buf);
            return 0;
        }
    }
    free(pkgs);
    free(buf);
    return -1;
}

static int get_installed_version(const char *pkg, char *ver, int ver_size);

static int lookup_pkg_version(const char *pkg, char *ver, int ver_size) {
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;

    ver[0] = '\0';
    buf = malloc(MAX_RESP_SIZE);
    if (!buf) return -1;
    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) { free(buf); return -1; }
    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    for (j = 0; j < npkgs; j++) {
        if (strcmp(pkgs[j].name, pkg) == 0) {
            strncpy(ver, pkgs[j].version, (size_t)(ver_size - 1));
            ver[ver_size - 1] = '\0';
            free(pkgs);
            free(buf);
            return 0;
        }
    }
    free(pkgs);
    free(buf);
    return -1;
}

static int lookup_pkg_entry(const char *pkg, pkg_entry_t *out) {
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;

    memset(out, 0, sizeof(*out));
    buf = malloc(MAX_RESP_SIZE);
    if (!buf) return -1;
    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) { free(buf); return -1; }
    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    for (j = 0; j < npkgs; j++) {
        if (strcmp(pkgs[j].name, pkg) == 0) {
            *out = pkgs[j];
            free(pkgs);
            free(buf);
            return 0;
        }
    }
    free(pkgs);
    free(buf);
    return -1;
}

static void print_usage(void) {
    fprintf(stderr, "Usage: lebpkg <command> [options]\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  install <pkg>     Install a package\n");
    fprintf(stderr, "  install -f <file> Install from local .lpkg file\n");
    fprintf(stderr, "  remove <pkg>      Remove an installed package\n");
    fprintf(stderr, "  upgrade           Upgrade outdated packages\n");
    fprintf(stderr, "  list              List installed packages\n");
    fprintf(stderr, "  update            Update package index\n");
    fprintf(stderr, "  search <term>     Search for packages\n");
    fprintf(stderr, "  info <pkg>        Show package details\n");
    fprintf(stderr, "  help              Show this help\n");
}

static int http_get(const char *url, uint8_t *buf, uint64_t bufsz,
                    uint64_t *got, int *status) {
    http_get_req_t req;
    uint64_t hdr_len;

    hdr_len = 0;
    req.url = url;
    req.buffer = buf;
    req.buffer_size = bufsz;
    req.out_size = got;
    req.status_code = status;
    req.max_redirects = 5;
    req.headers_buf = NULL;
    req.headers_buf_size = 0;
    req.out_headers_len = &hdr_len;
    return (int)leb_syscall1(LEB_SYSCALL_NET_HTTP_GET, (long)&req);
}

static int http_get_alloc(const char *url, uint8_t **out_buf, uint64_t *out_size,
                          int *status) {
    http_get_alloc_req_t req;
    uint64_t buf_addr;

    buf_addr = 0;
    req.url = url;
    req.out_buffer = &buf_addr;
    req.out_size = out_size;
    req.status_code = status;
    req.max_redirects = 5;
    req.headers_buf = NULL;
    req.headers_buf_size = 0;
    req.out_headers_len = NULL;
    if ((int)leb_syscall1(LEB_SYSCALL_NET_HTTP_GET_ALLOC, (long)&req) < 0)
        return -1;
    *out_buf = (uint8_t *)buf_addr;
    return 0;
}

static int read_file_contents(const char *path, char *buf, unsigned int bufsz) {
    int fd;
    int rd;
    unsigned int total;

    fd = vfs_open(path, 0);
    if (fd < 0) return -1;
    total = 0;
    while (total < bufsz - 1) {
        rd = vfs_read_fd(fd, buf + total, bufsz - 1 - total);
        if (rd <= 0) break;
        total += rd;
    }
    buf[total] = '\0';
    vfs_close_fd(fd);
    return (int)total;
}

static int parse_db_version(const char *buf, char *ver, int ver_size) {
    const char *p;
    int len;

    ver[0] = '\0';
    p = buf;
    while (*p) {
        if (strncmp(p, "VERSION:", 8) == 0) {
            p += 8;
            break;
        }
        if (strncmp(p, "Version:", 8) == 0) {
            p += 8;
            while (*p == ' ' || *p == '\t') p++;
            break;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (!*p) return -1;
    len = 0;
    while (p[len] && p[len] != '\n' && p[len] != '\r' && len < ver_size - 1) len++;
    if (len <= 0) return -1;
    memcpy(ver, p, (size_t)len);
    ver[len] = '\0';
    return 0;
}

static int write_file_contents(const char *path, const char *data, unsigned int len) {
    int fd;
    int wr;
    unsigned int total;

    ensure_parent_dirs(path);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    total = 0;
    while (total < len) {
        wr = vfs_write_fd(fd, data + total, len - total);
        if (wr <= 0) break;
        total += (unsigned int)wr;
    }
    close(fd);
    if (total != len) return -1;
    return (int)total;
}

static int read_u16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void ensure_parent_dirs(const char *filepath) {
    char tmp[MAX_URL_LEN];
    int i;

    strncpy(tmp, filepath, MAX_URL_LEN - 1);
    tmp[MAX_URL_LEN - 1] = '\0';
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            vfs_mkdir(tmp, 7);
            tmp[i] = '/';
        }
    }
}

static int extract_lpkg(const uint8_t *data, uint32_t len, const pkg_entry_t *meta) {
    uint16_t file_count;
    uint32_t off;
    int i;
    uint16_t path_len;
    uint32_t file_size;
    char filepath[MAX_URL_LEN];
    char db_path[MAX_URL_LEN];
    char file_list[8192];
    char db_buf[8704];
    int list_off;
    int db_off;

    if (len < 7) {
        fprintf(stderr, "lebpkg: package too small\n");
        return -1;
    }
    if (memcmp(data, LPKG_MAGIC, 4) != 0) {
        fprintf(stderr, "lebpkg: not a valid .lpkg file (bad magic)\n");
        return -1;
    }
    if (data[4] != LPKG_VERSION) {
        fprintf(stderr, "lebpkg: unsupported .lpkg version %d\n", data[4]);
        return -1;
    }

    file_count = (uint16_t)read_u16(data + 5);
    off = 7;
    list_off = 0;

    for (i = 0; i < file_count; i++) {
        if (off + 2 > len) {
            fprintf(stderr, "lebpkg: truncated package (path len)\n");
            return -1;
        }
        path_len = (uint16_t)read_u16(data + off);
        off += 2;

        if (path_len == 0 || path_len >= MAX_FILE_PATH || off + path_len > len) {
            fprintf(stderr, "lebpkg: invalid file path in package\n");
            return -1;
        }
        memcpy(filepath, data + off, path_len);
        filepath[path_len] = '\0';
        off += path_len;

        if (off + 4 > len) {
            fprintf(stderr, "lebpkg: truncated package (file size)\n");
            return -1;
        }
        file_size = read_u32(data + off);
        off += 4;

        if (off + file_size > len) {
            fprintf(stderr, "lebpkg: truncated package (file data)\n");
            return -1;
        }

        if (filepath[0] != '/') {
            fprintf(stderr, "lebpkg: skipping relative path '%s'\n", filepath);
            off += file_size;
            continue;
        }

        ensure_parent_dirs(filepath);
        vfs_create(filepath, 7);
        write_file_contents(filepath, (const char *)(data + off), file_size);
        printf("  %s (%u bytes)\n", filepath, file_size);

        if (list_off > 0 && list_off < (int)sizeof(file_list) - 1) {
            file_list[list_off++] = '\n';
        }
        if (list_off + (int)path_len < (int)sizeof(file_list) - 1) {
            memcpy(file_list + list_off, filepath, path_len);
            list_off += path_len;
        }
        off += file_size;
    }

    file_list[list_off] = '\0';

    vfs_mkdir(LEBPKG_CONF_DIR, 7);
    vfs_mkdir(LEBPKG_DB_DIR, 7);
    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, meta->name);
    db_off = snprintf(db_buf, sizeof(db_buf),
                      "Package: %s\n"
                      "Status: install ok installed\n"
                      "Version: %s\n",
                      meta->name,
                      meta->version[0] ? meta->version : "unknown");
    if (meta->depends[0] && db_off < (int)sizeof(db_buf))
        db_off += snprintf(db_buf + db_off, sizeof(db_buf) - db_off, "Depends: %s\n", meta->depends);
    if (meta->category[0] && db_off < (int)sizeof(db_buf))
        db_off += snprintf(db_buf + db_off, sizeof(db_buf) - db_off, "Section: %s\n", meta->category);
    if (meta->license[0] && db_off < (int)sizeof(db_buf))
        db_off += snprintf(db_buf + db_off, sizeof(db_buf) - db_off, "License: %s\n", meta->license);
    if (meta->description[0] && db_off < (int)sizeof(db_buf))
        db_off += snprintf(db_buf + db_off, sizeof(db_buf) - db_off, "Description: %s\n", meta->description);
    if (db_off < (int)sizeof(db_buf))
        db_off += snprintf(db_buf + db_off, sizeof(db_buf) - db_off, "Files:\n");
    if (db_off + list_off < (int)sizeof(db_buf)) {
        memcpy(db_buf + db_off, file_list, list_off);
        db_off += list_off;
    }
    if (db_off < (int)sizeof(db_buf) - 1)
        db_buf[db_off++] = '\n';
    write_file_contents(db_path, db_buf, (unsigned int)db_off);

    return 0;
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *parse_json_string(const char *p, char *out, int maxlen) {
    int i;

    if (*p != '"') return NULL;
    p++;
    i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n' && i < maxlen - 1) out[i++] = '\n';
            else if (*p == 't' && i < maxlen - 1) out[i++] = '\t';
            else if (*p == '\\' && i < maxlen - 1) out[i++] = '\\';
            else if (*p == '"' && i < maxlen - 1) out[i++] = '"';
            else if (i < maxlen - 1) out[i++] = *p;
            p++;
        } else {
            if (i < maxlen - 1) out[i++] = *p;
            p++;
        }
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

static const char *skip_json_value(const char *p) {
    int depth;

    p = skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) p++;
            p++;
        }
        if (*p == '"') p++;
        return p;
    }
    if (*p == '[' || *p == '{') {
        depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '[' || *p == '{') depth++;
            else if (*p == ']' || *p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) p++;
                    p++;
                }
            }
            if (*p) p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

static const char *parse_pkg_object(const char *p, pkg_entry_t *pkg) {
    char key[64];

    memset(pkg, 0, sizeof(*pkg));
    p = skip_ws(p);
    if (*p != '{') return NULL;
    p++;
    p = skip_ws(p);
    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == ',') { p++; p = skip_ws(p); }
        if (*p == '}') break;

        p = parse_json_string(p, key, sizeof(key));
        if (!p) return NULL;

        p = skip_ws(p);
        if (*p != ':') return NULL;
        p++;
        p = skip_ws(p);

        if (*p == '"') {
            if (strcmp(key, "name") == 0)
                p = parse_json_string(p, pkg->name, sizeof(pkg->name));
            else if (strcmp(key, "version") == 0)
                p = parse_json_string(p, pkg->version, sizeof(pkg->version));
            else if (strcmp(key, "description") == 0)
                p = parse_json_string(p, pkg->description, sizeof(pkg->description));
            else if (strcmp(key, "author") == 0)
                p = parse_json_string(p, pkg->author, sizeof(pkg->author));
            else if (strcmp(key, "license") == 0)
                p = parse_json_string(p, pkg->license, sizeof(pkg->license));
            else if (strcmp(key, "depends") == 0)
                p = parse_json_string(p, pkg->depends, sizeof(pkg->depends));
            else if (strcmp(key, "arch") == 0)
                p = parse_json_string(p, pkg->arch, sizeof(pkg->arch));
            else if (strcmp(key, "category") == 0)
                p = parse_json_string(p, pkg->category, sizeof(pkg->category));
            else
                p = skip_json_value(p);
        } else if (*p == '[' && strcmp(key, "depends") == 0) {
            int doff;
            char tmp[64];
            doff = 0;
            pkg->depends[0] = '\0';
            p++;
            p = skip_ws(p);
            while (*p && *p != ']') {
                if (*p == ',') { p++; p = skip_ws(p); }
                if (*p == ']') break;
                if (*p == '"') {
                    p = parse_json_string(p, tmp, sizeof(tmp));
                    if (!p) return NULL;
                    if (tmp[0]) {
                        int tlen;
                        tlen = strlen(tmp);
                        if (doff > 0 && doff < (int)sizeof(pkg->depends) - 1)
                            pkg->depends[doff++] = ',';
                        if (doff + tlen < (int)sizeof(pkg->depends) - 1) {
                            memcpy(pkg->depends + doff, tmp, tlen);
                            doff += tlen;
                        }
                    }
                } else {
                    p = skip_json_value(p);
                    if (!p) return NULL;
                }
                p = skip_ws(p);
            }
            pkg->depends[doff] = '\0';
            if (*p == ']') p++;
        } else {
            p = skip_json_value(p);
        }

        if (!p) return NULL;
        p = skip_ws(p);
    }
    if (*p == '}') p++;
    return p;
}

static int parse_pkg_index(const char *json, pkg_entry_t *pkgs, int max) {
    const char *p;
    int count;
    char key[64];

    p = skip_ws(json);
    if (*p == '{') {
        p++;
        p = skip_ws(p);
        while (*p && *p != '}') {
            if (*p == ',') { p++; p = skip_ws(p); }
            if (*p == '}') break;
            p = parse_json_string(p, key, sizeof(key));
            if (!p) return 0;
            p = skip_ws(p);
            if (*p != ':') return 0;
            p++;
            p = skip_ws(p);
            if (strcmp(key, "packages") == 0 && *p == '[')
                break;
            p = skip_json_value(p);
            if (!p) return 0;
            p = skip_ws(p);
        }
    }
    if (*p != '[') return 0;
    p++;
    p = skip_ws(p);
    count = 0;
    while (*p && *p != ']' && count < max) {
        if (*p == ',') { p++; p = skip_ws(p); }
        if (*p == ']') break;
        p = parse_pkg_object(p, &pkgs[count]);
        if (!p) return count;
        count++;
        p = skip_ws(p);
    }
    return count;
}

static int load_repos(repo_t *repos, int max) {
    int fd;
    char name[64];
    unsigned int type;
    unsigned int idx;
    int count;
    char filepath[MAX_URL_LEN];
    char line[MAX_LINE_LEN];
    int rd;
    char *p;
    char *url_start;
    char *ver_start;
    char *scope_start;

    fd = vfs_open(LEBPKG_REPO_DIR, 0);
    if (fd < 0) return 0;

    count = 0;
    idx = 0;
    while (count < max) {
        if (vfs_readdir(fd, name, &type, idx) < 0) break;
        idx++;
        if (type != 1) continue;

        rd = strlen(name);
        if (rd < 6) continue;
        if (strcmp(name + rd - 6, ".lrepo") != 0) continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", LEBPKG_REPO_DIR, name);
        rd = read_file_contents(filepath, line, sizeof(line));
        if (rd <= 0) continue;

        p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "repo ", 5) != 0) continue;
        p += 5;
        while (*p == ' ') p++;

        url_start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        if (*p) { *p = '\0'; p++; }
        while (*p == ' ') p++;

        ver_start = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        if (*p) { *p = '\0'; p++; }
        while (*p == ' ') p++;

        scope_start = p;
        while (*p && *p != ' ' && *p != '\n' && *p != '\r') p++;
        *p = '\0';

        strncpy(repos[count].base_url, url_start, MAX_URL_LEN - 1);
        repos[count].base_url[MAX_URL_LEN - 1] = '\0';
        strncpy(repos[count].ver, ver_start, 31);
        repos[count].ver[31] = '\0';
        strncpy(repos[count].scope, scope_start, 31);
        repos[count].scope[31] = '\0';
        count++;
    }
    vfs_close_fd(fd);
    return count;
}

static int cmd_lebpkg_list(void) {
    int fd;
    char name[64];
    unsigned int type;
    unsigned int idx;
    int count;
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;
    char inst_ver[32];

    fd = vfs_open(LEBPKG_DB_DIR, 0);
    if (fd < 0) {
        printf("No packages installed.\n");
        return 0;
    }

    buf = malloc(MAX_RESP_SIZE);
    pkgs = NULL;
    npkgs = 0;
    if (buf) {
        pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
        if (pkgs)
            npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
        free(buf);
    }

    count = 0;
    idx = 0;
    for (;;) {
        if (vfs_readdir(fd, name, &type, idx) < 0) break;
        idx++;
        if (type != 1) continue;
        inst_ver[0] = '\0';
        get_installed_version(name, inst_ver, sizeof(inst_ver));
        for (j = 0; j < npkgs; j++) {
            if (strcmp(pkgs[j].name, name) == 0) break;
        }
        if (inst_ver[0])
            printf("%s/%s [installed]", name, inst_ver);
        else
            printf("%s [installed]", name);
        if (j < npkgs && pkgs[j].description[0])
            printf("\n  %s\n", pkgs[j].description);
        else
            putchar('\n');
        count++;
    }
    vfs_close_fd(fd);
    if (pkgs) free(pkgs);
    if (count == 0)
        printf("No packages installed.\n");
    return 0;
}

static int parse_manifest_categories(const char *json, char cats[][64], int max) {
    const char *p;
    const char *arr;
    int count;
    char key[64];
    char val[64];

    p = skip_ws(json);
    if (*p != '{') return 0;
    p++;
    p = skip_ws(p);

    arr = NULL;
    while (*p && *p != '}') {
        if (*p == ',') { p++; p = skip_ws(p); }
        if (*p == '}') break;
        p = parse_json_string(p, key, sizeof(key));
        if (!p) return 0;
        p = skip_ws(p);
        if (*p != ':') return 0;
        p++;
        p = skip_ws(p);
        if (strcmp(key, "categories") == 0 && *p == '[') {
            arr = p;
            break;
        }
        p = skip_json_value(p);
        if (!p) return 0;
        p = skip_ws(p);
    }

    if (!arr) return 0;
    p = arr + 1;
    p = skip_ws(p);
    count = 0;
    while (*p && *p != ']' && count < max) {
        if (*p == ',') { p++; p = skip_ws(p); }
        if (*p == ']') break;
        if (*p != '{') break;
        p++;
        p = skip_ws(p);
        val[0] = '\0';
        while (*p && *p != '}') {
            if (*p == ',') { p++; p = skip_ws(p); }
            if (*p == '}') break;
            p = parse_json_string(p, key, sizeof(key));
            if (!p) return count;
            p = skip_ws(p);
            if (*p != ':') return count;
            p++;
            p = skip_ws(p);
            if (strcmp(key, "file") == 0 && *p == '"')
                p = parse_json_string(p, val, sizeof(val));
            else
                p = skip_json_value(p);
            if (!p) return count;
            p = skip_ws(p);
        }
        if (*p == '}') p++;
        p = skip_ws(p);
        if (val[0]) {
            strncpy(cats[count], val, 63);
            cats[count][63] = '\0';
            count++;
        }
    }
    return count;
}

static int cmd_lebpkg_update(void) {
    repo_t repos[MAX_REPOS];
    int nrepos;
    int i;
    int c;
    uint8_t *buf;
    uint64_t got;
    int status;
    char url[MAX_URL_LEN];
    char idx_path[MAX_URL_LEN];
    int ret;
    char cat_files[MAX_CATEGORIES][64];
    int ncat;

    nrepos = load_repos(repos, MAX_REPOS);
    if (nrepos == 0) {
        fprintf(stderr, "lebpkg: no repositories configured\n");
        return 1;
    }

    buf = malloc(MAX_RESP_SIZE);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    vfs_mkdir(LEBPKG_CONF_DIR, 0755);
    vfs_mkdir(LEBPKG_IDX_DIR, 0755);

    for (i = 0; i < nrepos; i++) {
        snprintf(url, sizeof(url), "%s/%s/%s/index/index.json",
                 repos[i].base_url, repos[i].ver, repos[i].scope);
        printf("Fetching index from %s...\n", url);

        got = 0;
        status = 0;
        ret = http_get(url, buf, MAX_RESP_SIZE - 1, &got, &status);
        if (ret < 0 || status != 200) {
            fprintf(stderr, "  Failed (status %d)\n", status);
            continue;
        }
        buf[got] = '\0';

        ncat = parse_manifest_categories((char *)buf, cat_files, MAX_CATEGORIES);
        if (ncat == 0) {
            fprintf(stderr, "  No categories in manifest\n");
            continue;
        }

        for (c = 0; c < ncat; c++) {
            snprintf(url, sizeof(url), "%s/%s/%s/index/%s",
                     repos[i].base_url, repos[i].ver, repos[i].scope,
                     cat_files[c]);
            printf("  Fetching %s...\n", cat_files[c]);

            got = 0;
            status = 0;
            ret = http_get(url, buf, MAX_RESP_SIZE - 1, &got, &status);
            if (ret < 0 || status != 200) {
                fprintf(stderr, "    Failed (status %d)\n", status);
                continue;
            }
            buf[got] = '\0';

            snprintf(idx_path, sizeof(idx_path), "%s/index/%d_%s",
                     LEBPKG_CONF_DIR, i, cat_files[c]);
            if (write_file_contents(idx_path, (char *)buf, got) != (int)got) {
                fprintf(stderr, "    Failed to save %s\n", idx_path);
                continue;
            }
            printf("    Saved (%u bytes)\n", got);
        }
    }

    free(buf);
    return 0;
}

static int load_all_index_pkgs(pkg_entry_t *pkgs, int max, char *buf, unsigned int bufsz) {
    int dirfd;
    char name[64];
    unsigned int type;
    unsigned int idx;
    int total;
    char path[MAX_URL_LEN];
    int nlen;
    int n;

    dirfd = vfs_open(LEBPKG_IDX_DIR, 0);
    if (dirfd < 0) return 0;

    total = 0;
    idx = 0;
    while (total < max) {
        if (vfs_readdir(dirfd, name, &type, idx) < 0)
            break;
        idx++;
        if (type != 1) continue;
        nlen = strlen(name);
        if (nlen < 5 || strcmp(name + nlen - 5, ".json") != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", LEBPKG_IDX_DIR, name);
        if (read_file_contents(path, buf, bufsz) <= 0)
            continue;
        n = parse_pkg_index(buf, pkgs + total, max - total);
        total += n;
    }
    vfs_close_fd(dirfd);

    return total;
}

static int cmd_lebpkg_search(const char *term) {
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;
    int found;

    buf = malloc(MAX_RESP_SIZE);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) {
        fprintf(stderr, "lebpkg: out of memory\n");
        free(buf);
        return 1;
    }

    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    found = 0;
    for (j = 0; j < npkgs; j++) {
        if (strstr(pkgs[j].name, term) ||
            strstr(pkgs[j].description, term)) {
            printf("%s/%s\n  %s\n",
                   pkgs[j].name, pkgs[j].version, pkgs[j].description);
            found++;
        }
    }

    if (found == 0) {
        printf("No packages matching '%s' found.\n", term);
        printf("Try running 'lebpkg update' first.\n");
    }
    free(pkgs);
    free(buf);
    return 0;
}

static int is_pkg_installed(const char *pkg) {
    char db_path[MAX_URL_LEN];
    int fd;

    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);
    fd = vfs_open(db_path, 0);
    if (fd < 0) return 0;
    vfs_close_fd(fd);
    return 1;
}

static int get_installed_version(const char *pkg, char *ver, int ver_size) {
    char db_path[MAX_URL_LEN];
    char buf[256];
    int rd;

    ver[0] = '\0';
    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);
    rd = read_file_contents(db_path, buf, sizeof(buf));
    if (rd <= 0) return -1;
    return parse_db_version(buf, ver, ver_size);
}

static int download_and_install_pkg(const char *pkg, repo_t *repos, int nrepos) {
    int i;
    char url[MAX_URL_LEN];
    uint8_t *buf;
    uint64_t got;
    int status;
    int ret;
    pkg_entry_t meta;
    uint64_t map_len;

    if (lookup_pkg_entry(pkg, &meta) < 0) {
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.name, pkg, sizeof(meta.name) - 1);
        meta.name[sizeof(meta.name) - 1] = '\0';
    }

    for (i = 0; i < nrepos; i++) {
        snprintf(url, sizeof(url), "%s/%s/%s/packages/%s.lpkg",
                 repos[i].base_url, repos[i].ver, repos[i].scope, pkg);
        printf("Downloading %s...\n", url);

        buf = NULL;
        got = 0;
        status = 0;
        ret = http_get_alloc(url, &buf, &got, &status);
        if (ret < 0 || status != 200 || !buf || got == 0)
            continue;

        printf("Downloaded %u bytes, extracting...\n", (unsigned int)got);

        if (extract_lpkg(buf, (uint32_t)got, &meta) < 0) {
            fprintf(stderr, "lebpkg: failed to extract package '%s'\n", pkg);
            map_len = (got + 0xFFFu) & ~0xFFFu;
            if (map_len > 0) munmap(buf, (size_t)map_len);
            return -1;
        }

        map_len = (got + 0xFFFu) & ~0xFFFu;
        if (map_len > 0) munmap(buf, (size_t)map_len);
        printf("Package '%s' installed successfully.\n", pkg);
        return 0;
    }

    fprintf(stderr, "lebpkg: package '%s' not found in any repository\n", pkg);
    return -1;
}

static int pkg_exists_in_index(const char *pkg) {
    char *buf;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;

    buf = malloc(MAX_RESP_SIZE);
    if (!buf) return 0;
    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) { free(buf); return 0; }
    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    for (j = 0; j < npkgs; j++) {
        if (strcmp(pkgs[j].name, pkg) == 0) {
            free(pkgs);
            free(buf);
            return 1;
        }
    }
    free(pkgs);
    free(buf);
    return 0;
}

static int cmd_lebpkg_install(const char *pkg) {
    repo_t repos[MAX_REPOS];
    int nrepos;
    char deps[128];
    char dep_list[8][64];
    int ndeps;
    char *p;
    char *tok;
    int i;

    if (is_pkg_installed(pkg)) {
        printf("Package '%s' is already installed.\n", pkg);
        return 0;
    }

    nrepos = load_repos(repos, MAX_REPOS);
    if (nrepos == 0) {
        fprintf(stderr, "lebpkg: no repositories configured\n");
        return 1;
    }

    if (!pkg_exists_in_index(pkg)) {
        fprintf(stderr, "lebpkg: package '%s' not found in any repository\n", pkg);
        fprintf(stderr, "Try running 'lebpkg update' first.\n");
        return 1;
    }

    ndeps = 0;
    if (lookup_pkg_deps(pkg, deps, sizeof(deps)) == 0 && deps[0]) {
        p = deps;
        while (*p && ndeps < 8) {
            while (*p == ' ' || *p == ',') p++;
            if (!*p) break;
            tok = p;
            while (*p && *p != ',' && *p != ' ') p++;
            if (p > tok) {
                i = (int)(p - tok);
                if (i >= 64) i = 63;
                memcpy(dep_list[ndeps], tok, (size_t)i);
                dep_list[ndeps][i] = '\0';
                ndeps++;
            }
        }
    }

    printf("Package: %s\n", pkg);
    if (ndeps > 0) {
        printf("The following dependencies will also be installed:\n");
        for (i = 0; i < ndeps; i++) {
            if (is_pkg_installed(dep_list[i]))
                printf("  %s (already installed)\n", dep_list[i]);
            else
                printf("  %s\n", dep_list[i]);
        }
    }
    if (!confirm_yn("Install?")) {
        printf("Aborted.\n");
        return 1;
    }

    if (download_and_install_pkg(pkg, repos, nrepos) < 0)
        return 1;

    for (i = 0; i < ndeps; i++) {
        if (is_pkg_installed(dep_list[i])) {
            printf("Dependency '%s' is already installed, skipping.\n", dep_list[i]);
            continue;
        }
        if (download_and_install_pkg(dep_list[i], repos, nrepos) < 0)
            fprintf(stderr, "lebpkg: warning: failed to install dependency '%s'\n", dep_list[i]);
    }

    return 0;
}

static int cmd_lebpkg_install_file(const char *path) {
    int rd;
    uint64_t fsize;
    uint64_t ftype;
    int fd;
    uint8_t *buf;
    unsigned int total;
    int n;
    const char *base;
    const char *dot;
    char pkg_name[64];
    int name_len;
    pkg_entry_t meta;

    rd = strlen(path);
    if (rd < 6 || strcmp(path + rd - 5, ".lpkg") != 0) {
        fprintf(stderr, "lebpkg: file must have .lpkg extension\n");
        return 1;
    }

    fd = vfs_open(path, 0);
    if (fd < 0) {
        fprintf(stderr, "lebpkg: cannot open '%s'\n", path);
        return 1;
    }

    if (vfs_stat(fd, &fsize, &ftype) < 0) {
        fprintf(stderr, "lebpkg: cannot stat '%s'\n", path);
        vfs_close_fd(fd);
        return 1;
    }

    if (fsize == 0 || fsize > MAX_RESP_SIZE) {
        fprintf(stderr, "lebpkg: file too large or empty\n");
        vfs_close_fd(fd);
        return 1;
    }

    buf = malloc(fsize);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        vfs_close_fd(fd);
        return 1;
    }

    total = 0;
    while (total < fsize) {
        n = vfs_read_fd(fd, buf + total, fsize - total);
        if (n <= 0) break;
        total += (unsigned int)n;
    }
    vfs_close_fd(fd);

    if (total < 7) {
        fprintf(stderr, "lebpkg: file too small to be a valid package\n");
        free(buf);
        return 1;
    }

    base = path + rd - 1;
    while (base > path && *(base - 1) != '/') base--;
    dot = base + strlen(base) - 5;
    name_len = (int)(dot - base);
    if (name_len <= 0 || name_len >= (int)sizeof(pkg_name)) name_len = (int)sizeof(pkg_name) - 1;
    memcpy(pkg_name, base, (size_t)name_len);
    pkg_name[name_len] = '\0';
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.name, pkg_name, sizeof(meta.name) - 1);
    meta.name[sizeof(meta.name) - 1] = '\0';
    strncpy(meta.version, "unknown", sizeof(meta.version) - 1);
    meta.version[sizeof(meta.version) - 1] = '\0';

    printf("Installing from %s (%u bytes)...\n", path, total);
    if (extract_lpkg(buf, total, &meta) < 0) {
        fprintf(stderr, "lebpkg: failed to extract package\n");
        free(buf);
        return 1;
    }

    printf("Package '%s' installed successfully.\n", pkg_name);
    free(buf);
    return 0;
}

static int remove_pkg_files(const char *pkg) {
    char db_path[MAX_URL_LEN];
    char *buf;
    char *line;
    char *next;
    int rd;
    int removed;
    int ret;

    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);

    buf = malloc(MAX_RESP_SIZE);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    rd = read_file_contents(db_path, buf, MAX_RESP_SIZE);
    if (rd <= 0) {
        fprintf(stderr, "lebpkg: package '%s' is not installed\n", pkg);
        free(buf);
        return 1;
    }

    removed = 0;
    line = buf;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) { *next = '\0'; next++; }
        if (*line && *line == '/') {
            if (vfs_unlink(line) >= 0) {
                printf("  removed %s\n", line);
                removed++;
            }
        }
        line = next;
    }

    ret = vfs_unlink(db_path);
    if (ret < 0)
        ret = unlink(db_path);
    if (ret < 0) {
        fprintf(stderr, "lebpkg: failed to remove package database entry for '%s'\n", pkg);
        free(buf);
        return 1;
    }
    printf("Package '%s' removed (%d files).\n", pkg, removed);
    free(buf);
    return 0;
}

static int cmd_lebpkg_remove(const char *pkg) {
    char db_path[MAX_URL_LEN];
    int fd;

    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);
    fd = vfs_open(db_path, 0);
    if (fd < 0) {
        fprintf(stderr, "lebpkg: package '%s' is not installed\n", pkg);
        return 1;
    }
    vfs_close_fd(fd);

    printf("Package: %s\n", pkg);
    if (!confirm_yn("Remove?")) {
        printf("Aborted.\n");
        return 1;
    }

    return remove_pkg_files(pkg);
}

static int cmd_lebpkg_info(const char *pkg) {
    char db_path[MAX_URL_LEN];
    char *buf;
    int rd;
    int installed;
    pkg_entry_t *pkgs;
    int npkgs;
    int j;

    installed = 0;
    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);

    buf = malloc(MAX_RESP_SIZE > 4096 ? MAX_RESP_SIZE : 4096);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    pkgs = malloc(MAX_PACKAGES * sizeof(pkg_entry_t));
    if (!pkgs) {
        free(buf);
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    rd = read_file_contents(db_path, buf, 4096);
    if (rd > 0) installed = 1;

    npkgs = load_all_index_pkgs(pkgs, MAX_PACKAGES, buf, MAX_RESP_SIZE);
    for (j = 0; j < npkgs; j++) {
        if (strcmp(pkgs[j].name, pkg) == 0) {
            printf("Name:        %s\n", pkgs[j].name);
            printf("Version:     %s\n", pkgs[j].version);
            printf("Description: %s\n", pkgs[j].description);
            if (pkgs[j].author[0])
                printf("Author:      %s\n", pkgs[j].author);
            if (pkgs[j].license[0])
                printf("License:     %s\n", pkgs[j].license);
            if (pkgs[j].depends[0])
                printf("Depends:     %s\n", pkgs[j].depends);
            if (pkgs[j].arch[0])
                printf("Arch:        %s\n", pkgs[j].arch);
            printf("Installed:   %s\n", installed ? "yes" : "no");
            if (installed) {
                rd = read_file_contents(db_path, buf, 4096);
                if (rd > 0)
                    printf("Files:\n%s\n", buf);
            }
            free(pkgs);
            free(buf);
            return 0;
        }
    }

    if (installed) {
        printf("Package: %s (installed)\n", pkg);
        rd = read_file_contents(db_path, buf, 4096);
        if (rd > 0)
            printf("Files:\n%s\n", buf);
        free(pkgs);
        free(buf);
        return 0;
    }

    fprintf(stderr, "lebpkg: package '%s' not found\n", pkg);
    free(pkgs);
    free(buf);
    return 1;
}

static int cmd_lebpkg_upgrade(void) {
    int fd;
    char name[64];
    unsigned int type;
    unsigned int idx;
    char installed_ver[32];
    char index_ver[32];
    char *upgrade_list[64];
    char upgrade_vers[64][32];
    int nupgrade;
    int i;
    repo_t repos[MAX_REPOS];
    int nrepos;

    fd = vfs_open(LEBPKG_DB_DIR, 0);
    if (fd < 0) {
        printf("No packages installed.\n");
        return 0;
    }

    nupgrade = 0;
    idx = 0;
    for (;;) {
        if (vfs_readdir(fd, name, &type, idx) < 0) break;
        idx++;
        if (type != 1) continue;
        if (get_installed_version(name, installed_ver, sizeof(installed_ver)) < 0)
            installed_ver[0] = '\0';
        if (lookup_pkg_version(name, index_ver, sizeof(index_ver)) < 0)
            continue;
        if (installed_ver[0] && strcmp(installed_ver, index_ver) == 0)
            continue;
        if (nupgrade < 64) {
            upgrade_list[nupgrade] = malloc(64);
            if (!upgrade_list[nupgrade]) continue;
            strncpy(upgrade_list[nupgrade], name, 63);
            upgrade_list[nupgrade][63] = '\0';
            strncpy(upgrade_vers[nupgrade], index_ver, 31);
            upgrade_vers[nupgrade][31] = '\0';
            nupgrade++;
        }
    }
    vfs_close_fd(fd);

    if (nupgrade == 0) {
        printf("All packages are up to date.\n");
        return 0;
    }

    printf("The following packages will be upgraded:\n");
    for (i = 0; i < nupgrade; i++) {
        if (get_installed_version(upgrade_list[i], installed_ver, sizeof(installed_ver)) == 0)
            printf("  %s (%s -> %s)\n", upgrade_list[i], installed_ver, upgrade_vers[i]);
        else
            printf("  %s (unknown -> %s)\n", upgrade_list[i], upgrade_vers[i]);
    }

    if (!confirm_yn("Upgrade?")) {
        printf("Aborted.\n");
        for (i = 0; i < nupgrade; i++) free(upgrade_list[i]);
        return 1;
    }

    nrepos = load_repos(repos, MAX_REPOS);
    if (nrepos == 0) {
        fprintf(stderr, "lebpkg: no repositories configured\n");
        for (i = 0; i < nupgrade; i++) free(upgrade_list[i]);
        return 1;
    }

    for (i = 0; i < nupgrade; i++) {
        printf("Upgrading %s...\n", upgrade_list[i]);
        if (download_and_install_pkg(upgrade_list[i], repos, nrepos) < 0)
            fprintf(stderr, "lebpkg: warning: failed to upgrade '%s'\n", upgrade_list[i]);
    }

    for (i = 0; i < nupgrade; i++) free(upgrade_list[i]);
    return 0;
}

int cmd_lebpkg(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (getuid() != 0) {
            fprintf(stderr, "lebpkg: install requires root\n");
            return 1;
        }
        if (argc < 3) {
            fprintf(stderr, "Usage: lebpkg install <package>\n");
            fprintf(stderr, "       lebpkg install -f <file.lpkg>\n");
            return 1;
        }
        if (strcmp(argv[2], "-f") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: lebpkg install -f <file.lpkg>\n");
                return 1;
            }
            return cmd_lebpkg_install_file(argv[3]);
        }
        return cmd_lebpkg_install(argv[2]);
    }

    if (strcmp(argv[1], "remove") == 0) {
        if (getuid() != 0) {
            fprintf(stderr, "lebpkg: remove requires root\n");
            return 1;
        }
        if (argc < 3) {
            fprintf(stderr, "Usage: lebpkg remove <package>\n");
            return 1;
        }
        return cmd_lebpkg_remove(argv[2]);
    }

    if (strcmp(argv[1], "upgrade") == 0) {
        if (getuid() != 0) {
            fprintf(stderr, "lebpkg: upgrade requires root\n");
            return 1;
        }
        return cmd_lebpkg_upgrade();
    }

    if (strcmp(argv[1], "list") == 0)
        return cmd_lebpkg_list();

    if (strcmp(argv[1], "update") == 0)
        return cmd_lebpkg_update();

    if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: lebpkg search <term>\n");
            return 1;
        }
        return cmd_lebpkg_search(argv[2]);
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: lebpkg info <package>\n");
            return 1;
        }
        return cmd_lebpkg_info(argv[2]);
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    fprintf(stderr, "lebpkg: unknown command '%s'\n", argv[1]);
    print_usage();
    return 1;
}
