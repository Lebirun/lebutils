#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cu.h"

static int sort_rev, sort_num, sort_uniq, sort_fold;
static char *sort_buf;
static size_t sort_len, sort_cap;

static int sort_cmp(const void *a, const void *b) {
    const char *x, *y;
    long long vx, vy;
    int r;

    x = *(char *const *)a;
    y = *(char *const *)b;
    r = 0;
    if (sort_num) {
        vx = strtoll(x, NULL, 10);
        vy = strtoll(y, NULL, 10);
        r = (vx > vy) - (vx < vy);
    }
    if (!r) r = sort_fold ? strcasecmp(x, y) : strcmp(x, y);
    return sort_rev ? -r : r;
}

static int sort_load(const char *path) {
    FILE *f;
    size_t rd;
    char *p;

    f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
    if (!f) {
        fprintf(stderr, "sort: cannot open '%s'\n", path);
        return 1;
    }
    for (;;) {
        if (sort_cap - sort_len < 512) {
            p = realloc(sort_buf, sort_cap ? sort_cap * 2 : 4096);
            if (!p) {
                fprintf(stderr, "sort: out of memory\n");
                if (f != stdin) fclose(f);
                return 1;
            }
            sort_buf = p;
            sort_cap = sort_cap ? sort_cap * 2 : 4096;
        }
        rd = fread(sort_buf + sort_len, 1, sort_cap - sort_len, f);
        if (!rd) break;
        sort_len += rd;
    }
    if (sort_len && sort_buf[sort_len - 1] != '\n') sort_buf[sort_len++] = '\n';
    if (f != stdin) fclose(f);
    return 0;
}

int cmd_sort(int argc, char **argv) {
    char **lines;
    const char *p;
    size_t n, i, start;
    int files;

    files = 0;
    for (i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: sort [-rnuf] [FILE]...");
            puts("Write sorted concatenation of all FILEs to standard output.");
            puts("With no FILE, or when FILE is -, read standard input.");
            puts("");
            puts("  -r  reverse the result of comparisons");
            puts("  -n  compare according to numerical value");
            puts("  -u  output only the first of equal lines");
            puts("  -f  ignore case");
            return 0;
        }
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            files++;
            continue;
        }
        for (p = argv[i] + 1; *p; p++) {
            if (*p == 'r') sort_rev = 1;
            else if (*p == 'n') sort_num = 1;
            else if (*p == 'u') sort_uniq = 1;
            else if (*p == 'f') sort_fold = 1;
            else {
                fprintf(stderr, "sort: invalid option -- '%c'\n", *p);
                return 2;
            }
        }
    }

    if (!files && sort_load("-")) return 2;
    for (i = 1; i < (size_t)argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') continue;
        if (sort_load(argv[i])) return 2;
    }

    n = 0;
    for (i = 0; i < sort_len; i++) {
        if (sort_buf[i] == '\n') n++;
    }
    if (!n) return 0;
    lines = malloc(n * sizeof(*lines));
    if (!lines) {
        fprintf(stderr, "sort: out of memory\n");
        return 2;
    }
    n = 0;
    start = 0;
    for (i = 0; i < sort_len; i++) {
        if (sort_buf[i] != '\n') continue;
        sort_buf[i] = '\0';
        lines[n++] = sort_buf + start;
        start = i + 1;
    }

    qsort(lines, n, sizeof(*lines), sort_cmp);
    for (i = 0; i < n; i++) {
        if (sort_uniq && i && sort_cmp(&lines[i - 1], &lines[i]) == 0) continue;
        puts(lines[i]);
    }
    free(lines);
    free(sort_buf);
    return 0;
}
