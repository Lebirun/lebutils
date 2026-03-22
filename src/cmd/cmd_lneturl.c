#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lebirun.h>

#include "cu.h"

#define MAX_RESPONSE_SIZE (64 * 1024)
#define MAX_POST_BODY     (64 * 1024)
#define MAX_HEADERS_SIZE  4096

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
    const char *content_type;
    const uint8_t *post_body;
    uint64_t post_body_len;
    uint8_t *buffer;
    uint64_t buffer_size;
    uint64_t *out_size;
    int *status_code;
} __attribute__((packed)) http_post_req_t;

static void print_usage(void) {
    fprintf(stderr, "Usage: lneturl [options] <url>\n");
    fprintf(stderr, "  -X <method>       HTTP method (GET or POST)\n");
    fprintf(stderr, "  -d <data>         POST body data\n");
    fprintf(stderr, "  -H <header>       Content-Type header value\n");
    fprintf(stderr, "  -o <file>         Write output to file\n");
    fprintf(stderr, "  -v                Verbose output\n");
    fprintf(stderr, "  -s                Silent mode (no output except data)\n");
    fprintf(stderr, "  -u <user:pass>    Basic authentication\n");
    fprintf(stderr, "  -L [max]          Follow redirects (default: 5)\n");
    fprintf(stderr, "  -I                Show response headers only\n");
    fprintf(stderr, "  -A <agent>        Set User-Agent string\n");
    fprintf(stderr, "  -p                Show progress bar\n");
    fprintf(stderr, "  --help            Show this help\n");
}

static int b64_encode(const char *in, unsigned int in_len, char *out, unsigned int out_size) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int i;
    unsigned int o;
    unsigned int val;

    o = 0;
    for (i = 0; i < in_len; i += 3) {
        val = ((unsigned char)in[i]) << 16;
        if (i + 1 < in_len) val |= ((unsigned char)in[i + 1]) << 8;
        if (i + 2 < in_len) val |= ((unsigned char)in[i + 2]);

        if (o + 4 >= out_size) return -1;
        out[o++] = tbl[(val >> 18) & 0x3F];
        out[o++] = tbl[(val >> 12) & 0x3F];
        out[o++] = (i + 1 < in_len) ? tbl[(val >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < in_len) ? tbl[val & 0x3F] : '=';
    }
    out[o] = '\0';
    return (int)o;
}

static void print_progress(uint32_t received, uint32_t total) {
    int bar_width;
    int filled;
    int i;

    bar_width = 40;
    if (total > 0) {
        filled = (int)((uint64_t)received * bar_width / total);
        if (filled > bar_width) filled = bar_width;
        fprintf(stderr, "\r[");
        for (i = 0; i < bar_width; i++) {
            if (i < filled)
                fputc('#', stderr);
            else
                fputc('-', stderr);
        }
        fprintf(stderr, "] %u/%u bytes (%u%%)",
                received, total,
                (unsigned int)((uint64_t)received * 100 / total));
    } else {
        fprintf(stderr, "\r%u bytes received", received);
    }
}

static int do_get(const char *url, uint8_t *buf, uint64_t bufsz,
                  uint64_t *got, int *status, int max_redirects,
                  uint8_t *hdr_buf, uint64_t hdr_buf_sz, uint64_t *hdr_len) {
    http_get_req_t req;
    int ret;

    req.url = url;
    req.buffer = buf;
    req.buffer_size = bufsz;
    req.out_size = got;
    req.status_code = status;
    req.max_redirects = max_redirects;
    req.headers_buf = hdr_buf;
    req.headers_buf_size = hdr_buf_sz;
    req.out_headers_len = hdr_len;
    ret = (int)leb_syscall1(LEB_SYSCALL_NET_HTTP_GET, (long)&req);
    return ret;
}

static int do_post(const char *url, const char *ct,
                   const uint8_t *body, uint64_t body_len,
                   uint8_t *buf, uint64_t bufsz,
                   uint64_t *got, int *status) {
    http_post_req_t req;
    int ret;

    req.url = url;
    req.content_type = ct;
    req.post_body = body;
    req.post_body_len = body_len;
    req.buffer = buf;
    req.buffer_size = bufsz;
    req.out_size = got;
    req.status_code = status;
    ret = (int)leb_syscall1(LEB_SYSCALL_NET_HTTP_POST, (long)&req);
    return ret;
}

int cmd_lneturl(int argc, char **argv) {
    const char *url;
    const char *method;
    const char *postdata;
    const char *content_type;
    const char *outfile;
    const char *auth;
    const char *user_agent;
    int verbose;
    int silent;
    int show_headers;
    int show_progress;
    int max_redirects;
    int i;
    uint8_t *resp_buf;
    uint8_t *hdr_buf;
    uint64_t downloaded;
    uint64_t hdr_len;
    int status_code;
    int ret;
    char auth_hdr[256];
    char b64[192];
    char full_url[768];
    unsigned int ulen;
    unsigned int url_len;
    unsigned int hdr_offset;
    int fd;

    url = NULL;
    method = "GET";
    postdata = NULL;
    content_type = NULL;
    outfile = NULL;
    auth = NULL;
    user_agent = NULL;
    verbose = 0;
    silent = 0;
    show_headers = 0;
    show_progress = 0;
    max_redirects = 5;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-X") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -X requires an argument\n");
                return 1;
            }
            method = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -d requires an argument\n");
                return 1;
            }
            postdata = argv[++i];
            method = "POST";
        } else if (strcmp(argv[i], "-H") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -H requires an argument\n");
                return 1;
            }
            content_type = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -o requires an argument\n");
                return 1;
            }
            outfile = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            silent = 1;
        } else if (strcmp(argv[i], "-I") == 0) {
            show_headers = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            show_progress = 1;
        } else if (strcmp(argv[i], "-L") == 0) {
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                max_redirects = 0;
                {
                    const char *p = argv[++i];
                    while (*p >= '0' && *p <= '9') {
                        max_redirects = max_redirects * 10 + (*p - '0');
                        p++;
                    }
                }
            } else {
                max_redirects = 5;
            }
        } else if (strcmp(argv[i], "-A") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -A requires an argument\n");
                return 1;
            }
            user_agent = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "lneturl: -u requires an argument\n");
                return 1;
            }
            auth = argv[++i];
        } else if (argv[i][0] != '-') {
            url = argv[i];
        } else {
            fprintf(stderr, "lneturl: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!url) {
        fprintf(stderr, "lneturl: no URL specified\n");
        print_usage();
        return 1;
    }

    if (auth) {
        ulen = (unsigned int)strlen(auth);
        if (b64_encode(auth, ulen, b64, sizeof(b64)) < 0) {
            fprintf(stderr, "lneturl: auth string too long\n");
            return 1;
        }
        hdr_offset = (unsigned int)strlen("Authorization: Basic ");
        memcpy(auth_hdr, "Authorization: Basic ", hdr_offset);
        memcpy(auth_hdr + hdr_offset, b64, strlen(b64) + 1);
    }

    (void)auth_hdr;
    (void)full_url;
    (void)user_agent;

    if (verbose && !silent) {
        fprintf(stderr, "> %s %s\n", method, url);
        if (max_redirects > 0) {
            fprintf(stderr, "> Follow redirects: up to %d\n", max_redirects);
        }
        if (postdata) {
            fprintf(stderr, "> Content-Length: %u\n", (unsigned int)strlen(postdata));
        }
        if (content_type) {
            fprintf(stderr, "> Content-Type: %s\n", content_type);
        }
        if (user_agent) {
            fprintf(stderr, "> User-Agent: %s\n", user_agent);
        }
    }

    resp_buf = (uint8_t *)malloc(MAX_RESPONSE_SIZE);
    if (!resp_buf) {
        fprintf(stderr, "lneturl: out of memory\n");
        return 1;
    }

    hdr_buf = NULL;
    if (show_headers || verbose) {
        hdr_buf = (uint8_t *)malloc(MAX_HEADERS_SIZE);
    }

    downloaded = 0;
    status_code = 0;
    hdr_len = 0;

    if (strcmp(method, "POST") == 0) {
        ret = do_post(url, content_type,
                      postdata ? (const uint8_t *)postdata : NULL,
                      postdata ? (uint64_t)strlen(postdata) : 0,
                      resp_buf, MAX_RESPONSE_SIZE,
                      &downloaded, &status_code);
    } else if (strcmp(method, "GET") == 0) {
        ret = do_get(url, resp_buf, MAX_RESPONSE_SIZE,
                     &downloaded, &status_code, max_redirects,
                     hdr_buf, hdr_buf ? MAX_HEADERS_SIZE : 0, &hdr_len);
    } else {
        fprintf(stderr, "lneturl: unsupported method '%s'\n", method);
        free(resp_buf);
        if (hdr_buf) free(hdr_buf);
        return 1;
    }

    if (ret < 0) {
        if (!silent)
            fprintf(stderr, "lneturl: request failed\n");
        free(resp_buf);
        if (hdr_buf) free(hdr_buf);
        return 1;
    }

    if (verbose && !silent) {
        fprintf(stderr, "< HTTP status: %d\n", status_code);
        fprintf(stderr, "< Received: %lu bytes\n", (unsigned long)downloaded);
    }

    if (show_headers && hdr_buf && hdr_len > 0) {
        fwrite(hdr_buf, 1, hdr_len, stdout);
        free(hdr_buf);
        hdr_buf = NULL;
        free(resp_buf);
        return 0;
    }
    if (hdr_buf) {
        free(hdr_buf);
        hdr_buf = NULL;
    }

    if (show_progress) {
        print_progress((uint32_t)downloaded, (uint32_t)downloaded);
        fprintf(stderr, "\n");
    }

    if (outfile) {
        fd = vfs_open(outfile, 0);
        if (fd < 0) {
            vfs_create(outfile, 0644);
            fd = vfs_open(outfile, 0);
        }
        if (fd < 0) {
            if (!silent)
                fprintf(stderr, "lneturl: cannot open '%s'\n", outfile);
            free(resp_buf);
            return 1;
        }
        vfs_write_fd(fd, resp_buf, downloaded);
        vfs_close_fd(fd);
        if (verbose && !silent) {
            fprintf(stderr, "Saved to %s\n", outfile);
        }
    } else if (!silent) {
        url_len = downloaded;
        if (url_len > 0) {
            fwrite(resp_buf, 1, url_len, stdout);
            if (resp_buf[url_len - 1] != '\n') {
                putchar('\n');
            }
        }
    }

    free(resp_buf);
    return 0;
}
