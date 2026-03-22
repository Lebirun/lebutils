#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
} pkg_entry_t;

typedef struct {
    char base_url[MAX_URL_LEN];
    char ver[32];
    char scope[32];
} repo_t;

static int load_all_index_pkgs(pkg_entry_t *pkgs, int max, char *buf, unsigned int bufsz);

static void print_usage(void) {
    fprintf(stderr, "Usage: lebpkg <command> [options]\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  install <pkg>     Install a package\n");
    fprintf(stderr, "  install -f <file> Install from local .lpkg file\n");
    fprintf(stderr, "  remove <pkg>      Remove an installed package\n");
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

static int write_file_contents(const char *path, const char *data, unsigned int len) {
    int fd;
    int wr;
    unsigned int total;

    vfs_create(path, 6);
    fd = vfs_open(path, 1);
    if (fd < 0) return -1;
    total = 0;
    while (total < len) {
        wr = vfs_write_fd(fd, data + total, len - total);
        if (wr <= 0) break;
        total += (unsigned int)wr;
    }
    vfs_close_fd(fd);
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

static int extract_lpkg(const uint8_t *data, uint32_t len, const char *pkg_name) {
    uint16_t file_count;
    uint32_t off;
    int i;
    uint16_t path_len;
    uint32_t file_size;
    char filepath[MAX_URL_LEN];
    char db_path[MAX_URL_LEN];
    char file_list[4096];
    int list_off;

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
    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg_name);
    write_file_contents(db_path, file_list, (unsigned int)list_off);

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
            else
                p = skip_json_value(p);
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
        for (j = 0; j < npkgs; j++) {
            if (strcmp(pkgs[j].name, name) == 0) break;
        }
        if (j < npkgs)
            printf("%s/%s [installed]\n  %s\n", name, pkgs[j].version, pkgs[j].description);
        else
            printf("%s [installed]\n", name);
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

    vfs_mkdir(LEBPKG_IDX_DIR, 7);

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
            write_file_contents(idx_path, (char *)buf, got);
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

static int cmd_lebpkg_install(const char *pkg) {
    repo_t repos[MAX_REPOS];
    int nrepos;
    int i;
    uint8_t *buf;
    uint64_t got;
    int status;
    char url[MAX_URL_LEN];
    int ret;

    nrepos = load_repos(repos, MAX_REPOS);
    if (nrepos == 0) {
        fprintf(stderr, "lebpkg: no repositories configured\n");
        return 1;
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

        if (extract_lpkg(buf, (uint32_t)got, pkg) < 0) {
            fprintf(stderr, "lebpkg: failed to extract package '%s'\n", pkg);
            return 1;
        }

        printf("Package '%s' installed successfully.\n", pkg);
        return 0;
    }

    fprintf(stderr, "lebpkg: package '%s' not found in any repository\n", pkg);
    return 1;
}

static int cmd_lebpkg_install_file(const char *path) {
    int rd;
    unsigned int fsize;
    unsigned int ftype;
    int fd;
    uint8_t *buf;
    unsigned int total;
    int n;
    const char *base;
    const char *dot;
    char pkg_name[64];
    int name_len;

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

    printf("Installing from %s (%u bytes)...\n", path, total);
    if (extract_lpkg(buf, total, pkg_name) < 0) {
        fprintf(stderr, "lebpkg: failed to extract package\n");
        free(buf);
        return 1;
    }

    printf("Package '%s' installed successfully.\n", pkg_name);
    free(buf);
    return 0;
}

static int cmd_lebpkg_remove(const char *pkg) {
    char db_path[MAX_URL_LEN];
    char *buf;
    char *line;
    char *next;
    int rd;
    int removed;

    snprintf(db_path, sizeof(db_path), "%s/%s", LEBPKG_DB_DIR, pkg);

    buf = malloc(4096);
    if (!buf) {
        fprintf(stderr, "lebpkg: out of memory\n");
        return 1;
    }

    rd = read_file_contents(db_path, buf, 4096);
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

    vfs_unlink(db_path);
    printf("Package '%s' removed (%d files).\n", pkg, removed);
    free(buf);
    return 0;
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

int cmd_lebpkg(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "install") == 0) {
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
        if (argc < 3) {
            fprintf(stderr, "Usage: lebpkg remove <package>\n");
            return 1;
        }
        return cmd_lebpkg_remove(argv[2]);
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
