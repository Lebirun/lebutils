#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cu.h"

struct tr_set {
    unsigned char *c;
    size_t n;
    size_t cap;
};

static const struct {
    const char *name;
    int (*fn)(int);
} tr_classes[] = {
    {"alnum", isalnum}, {"alpha", isalpha}, {"blank", isblank}, {"cntrl", iscntrl},
    {"digit", isdigit}, {"graph", isgraph}, {"lower", islower}, {"print", isprint},
    {"punct", ispunct}, {"space", isspace}, {"upper", isupper}, {"xdigit", isxdigit},
    {NULL, NULL}
};

static int tr_push(struct tr_set *t, int c, size_t count) {
    unsigned char *p;
    size_t cap;

    if (t->n + count > t->cap) {
        cap = t->cap ? t->cap : 64;
        while (cap < t->n + count) cap *= 2;
        p = realloc(t->c, cap);
        if (!p) {
            fprintf(stderr, "tr: out of memory\n");
            return -1;
        }
        t->c = p;
        t->cap = cap;
    }
    memset(t->c + t->n, c, count);
    t->n += count;
    return 0;
}

static int tr_char(const char **s) {
    int c, v, i;

    c = (unsigned char)*(*s)++;
    if (c != '\\' || !**s) return c;
    c = (unsigned char)*(*s)++;
    if (c >= '0' && c <= '7') {
        v = c - '0';
        for (i = 0; i < 2 && **s >= '0' && **s <= '7'; i++) v = v * 8 + *(*s)++ - '0';
        return v & 0xff;
    }
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    }
    return c;
}

static int tr_parse(const char *s, struct tr_set *t, size_t *fill_at, int *fill_c) {
    const char *e, *p;
    char *end;
    unsigned long count;
    int c, hi, i;

    while (*s) {
        if (s[0] == '[' && s[1] == ':' && (e = strstr(s + 2, ":]"))) {
            for (i = 0; tr_classes[i].name; i++) {
                if (strlen(tr_classes[i].name) == (size_t)(e - s - 2) &&
                    strncmp(tr_classes[i].name, s + 2, e - s - 2) == 0) break;
            }
            if (!tr_classes[i].name) {
                fprintf(stderr, "tr: invalid character class '%.*s'\n", (int)(e - s - 2), s + 2);
                return -1;
            }
            for (c = 0; c < 256; c++) {
                if (tr_classes[i].fn(c) && tr_push(t, c, 1) < 0) return -1;
            }
            s = e + 2;
            continue;
        }
        if (s[0] == '[' && s[1] == '=' && s[2] && s[3] == '=' && s[4] == ']') {
            if (tr_push(t, (unsigned char)s[2], 1) < 0) return -1;
            s += 5;
            continue;
        }
        if (s[0] == '[' && s[1]) {
            p = s + 1;
            c = tr_char(&p);
            if (*p == '*') {
                count = strtoul(p + 1, &end, p[1] == '0' ? 8 : 10);
                if (*end == ']') {
                    if (end == p + 1 || count == 0) {
                        if (!fill_at) {
                            fprintf(stderr, "tr: [c*] repeat construct may not appear in SET1\n");
                            return -1;
                        }
                        *fill_at = t->n;
                        *fill_c = c;
                    } else if (tr_push(t, c, count) < 0) {
                        return -1;
                    }
                    s = end + 1;
                    continue;
                }
            }
        }
        c = tr_char(&s);
        if (s[0] == '-' && s[1]) {
            s++;
            hi = tr_char(&s);
            if (hi < c) {
                fprintf(stderr, "tr: range-endpoints of '%c-%c' are in reverse order\n", c, hi);
                return -1;
            }
            for (; c <= hi; c++) {
                if (tr_push(t, c, 1) < 0) return -1;
            }
        } else if (tr_push(t, c, 1) < 0) {
            return -1;
        }
    }
    return 0;
}

int cmd_tr(int argc, char **argv) {
    unsigned char map[256], del[256], sq[256], in[256];
    char buf[1024];
    struct tr_set a = {0}, b = {0};
    const char *p, *set1, *set2;
    size_t fill_at, rd, j;
    int i, c, last, fill_c, comp, del_mode, sq_mode, trunc, nsets, rc;

    comp = del_mode = sq_mode = trunc = 0;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: tr [-cdst] SET1 [SET2]");
            puts("Translate, squeeze and/or delete characters from standard input.");
            puts("");
            puts("  -c, -C  use the complement of SET1");
            puts("  -d      delete characters in SET1");
            puts("  -s      squeeze repeated characters in the last SET into one");
            puts("  -t      truncate SET1 to the length of SET2");
            puts("");
            puts("SETs are specified as strings of characters:");
            puts("  \\NNN, \\\\, \\a, \\b, \\f, \\n, \\r, \\t, \\v  escapes");
            puts("  CHAR1-CHAR2   all characters from CHAR1 to CHAR2");
            puts("  [CHAR*]       in SET2, copies of CHAR until length of SET1");
            puts("  [CHAR*REPEAT] REPEAT copies of CHAR (octal if starting with 0)");
            puts("  [:CLASS:]     alnum alpha blank cntrl digit graph lower print");
            puts("                punct space upper xdigit");
            puts("  [=CHAR=]      the character CHAR");
            return 0;
        }
        for (p = argv[i] + 1; *p; p++) {
            if (*p == 'c' || *p == 'C') comp = 1;
            else if (*p == 'd') del_mode = 1;
            else if (*p == 's') sq_mode = 1;
            else if (*p == 't') trunc = 1;
            else {
                fprintf(stderr, "tr: invalid option -- '%c'\n", *p);
                return 1;
            }
        }
    }

    nsets = argc - i;
    set1 = nsets > 0 ? argv[i] : NULL;
    set2 = nsets > 1 ? argv[i + 1] : NULL;
    if (!set1 || nsets > 2 || (!del_mode && !sq_mode && !set2) ||
        (del_mode && sq_mode && !set2) || (del_mode && !sq_mode && set2)) {
        fprintf(stderr, "tr: %s operand\nTry 'tr --help' for more information.\n",
                nsets > 2 || (del_mode && !sq_mode && set2) ? "extra" : "missing");
        return 1;
    }

    rc = 1;
    fill_at = (size_t)-1;
    fill_c = 0;
    if (tr_parse(set1, &a, NULL, NULL) < 0) goto out;
    if (comp) {
        memset(in, 0, sizeof(in));
        for (j = 0; j < a.n; j++) in[a.c[j]] = 1;
        a.n = 0;
        for (c = 0; c < 256; c++) {
            if (!in[c] && tr_push(&a, c, 1) < 0) goto out;
        }
    }
    if (set2) {
        if (tr_parse(set2, &b, &fill_at, &fill_c) < 0) goto out;
        if (fill_at != (size_t)-1) {
            j = b.n;
            rd = a.n > b.n ? a.n - b.n : 0;
            if (rd && tr_push(&b, fill_c, rd) < 0) goto out;
            memmove(b.c + fill_at + rd, b.c + fill_at, j - fill_at);
            memset(b.c + fill_at, fill_c, rd);
        }
    }

    for (c = 0; c < 256; c++) map[c] = (unsigned char)c;
    memset(del, 0, sizeof(del));
    memset(sq, 0, sizeof(sq));

    if (del_mode) {
        for (j = 0; j < a.n; j++) del[a.c[j]] = 1;
        if (sq_mode) {
            for (j = 0; j < b.n; j++) sq[b.c[j]] = 1;
        }
    } else if (set2) {
        if (trunc && a.n > b.n) a.n = b.n;
        if (a.n && !b.n) {
            fprintf(stderr, "tr: when not truncating SET1, SET2 must be non-empty\n");
            goto out;
        }
        for (j = 0; j < a.n; j++) map[a.c[j]] = b.c[j < b.n ? j : b.n - 1];
        if (sq_mode) {
            for (j = 0; j < b.n; j++) sq[b.c[j]] = 1;
        }
    } else {
        for (j = 0; j < a.n; j++) sq[a.c[j]] = 1;
    }
    free(a.c);
    free(b.c);
    a.c = b.c = NULL;

    last = -1;
    while ((rd = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        for (j = 0; j < rd; j++) {
            c = (unsigned char)buf[j];
            if (del[c]) continue;
            c = map[c];
            if (sq[c] && c == last) continue;
            putchar(c);
            last = c;
        }
    }
    rc = 0;
out:
    free(a.c);
    free(b.c);
    return rc;
}
