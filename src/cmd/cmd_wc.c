#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cu.h"

enum { WC_LINES, WC_WORDS, WC_CHARS, WC_BYTES, WC_MAXLEN, WC_N };

static int wc_show[WC_N];
static unsigned long wc_total[WC_N];

static void wc_print(const unsigned long *n, const char *name) {
    const char *sep;
    int i;

    sep = "";
    for (i = 0; i < WC_N; i++) {
        if (!wc_show[i]) continue;
        printf("%s%7lu", sep, n[i]);
        sep = " ";
    }
    if (name) printf(" %s", name);
    putchar('\n');
}

static int wc_file(const char *path, const char *name) {
    FILE *f;
    unsigned long n[WC_N] = {0};
    unsigned long col;
    char buf[1024];
    size_t rd, i;
    int inword;
    unsigned char c;

    f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
    if (!f) {
        fprintf(stderr, "wc: cannot open '%s'\n", path);
        return 1;
    }
    inword = 0;
    col = 0;
    while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
        n[WC_BYTES] += rd;
        for (i = 0; i < rd; i++) {
            c = (unsigned char)buf[i];
            if ((c & 0xc0) != 0x80) n[WC_CHARS]++;
            if (isspace(c)) inword = 0;
            else if (!inword) {
                inword = 1;
                n[WC_WORDS]++;
            }
            if (c == '\n' || c == '\r' || c == '\f') {
                if (c == '\n') n[WC_LINES]++;
                if (col > n[WC_MAXLEN]) n[WC_MAXLEN] = col;
                col = 0;
            } else if (c == '\t') {
                col = (col / 8 + 1) * 8;
            } else if ((c & 0xc0) != 0x80 && (c >= 0x80 || isprint(c))) {
                col++;
            }
        }
    }
    if (col > n[WC_MAXLEN]) n[WC_MAXLEN] = col;
    if (ferror(f)) fprintf(stderr, "wc: read error on '%s'\n", path);
    if (f != stdin) fclose(f);
    for (i = 0; i < WC_MAXLEN; i++) wc_total[i] += n[i];
    if (n[WC_MAXLEN] > wc_total[WC_MAXLEN]) wc_total[WC_MAXLEN] = n[WC_MAXLEN];
    wc_print(n, name);
    return 0;
}

int cmd_wc(int argc, char **argv) {
    const char *p;
    int i, files, rc, opts;

    files = 0;
    opts = 1;
    for (i = 1; i < argc; i++) {
        if (!opts || argv[i][0] != '-' || argv[i][1] == '\0') {
            files++;
            continue;
        }
        if (strcmp(argv[i], "--") == 0) {
            opts = 0;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: wc [-clmwL] [FILE]...");
            puts("Print newline, word and byte counts for each FILE, and a total line if");
            puts("more than one FILE is specified. With no FILE, or when FILE is -, read");
            puts("standard input.");
            puts("");
            puts("  -c, --bytes            print the byte counts");
            puts("  -m, --chars            print the character counts");
            puts("  -l, --lines            print the newline counts");
            puts("  -w, --words            print the word counts");
            puts("  -L, --max-line-length  print the maximum display width");
            return 0;
        }
        if (strcmp(argv[i], "--bytes") == 0) wc_show[WC_BYTES] = 1;
        else if (strcmp(argv[i], "--chars") == 0) wc_show[WC_CHARS] = 1;
        else if (strcmp(argv[i], "--lines") == 0) wc_show[WC_LINES] = 1;
        else if (strcmp(argv[i], "--words") == 0) wc_show[WC_WORDS] = 1;
        else if (strcmp(argv[i], "--max-line-length") == 0) wc_show[WC_MAXLEN] = 1;
        else if (argv[i][1] == '-') {
            fprintf(stderr, "wc: unrecognized option '%s'\n", argv[i]);
            return 1;
        } else {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'l') wc_show[WC_LINES] = 1;
                else if (*p == 'w') wc_show[WC_WORDS] = 1;
                else if (*p == 'm') wc_show[WC_CHARS] = 1;
                else if (*p == 'c') wc_show[WC_BYTES] = 1;
                else if (*p == 'L') wc_show[WC_MAXLEN] = 1;
                else {
                    fprintf(stderr, "wc: invalid option -- '%c'\n", *p);
                    return 1;
                }
            }
        }
    }
    for (i = 0; i < WC_N && !wc_show[i]; i++);
    if (i == WC_N) wc_show[WC_LINES] = wc_show[WC_WORDS] = wc_show[WC_BYTES] = 1;

    if (!files) return wc_file("-", NULL);
    rc = 0;
    opts = 1;
    for (i = 1; i < argc; i++) {
        if (opts && strcmp(argv[i], "--") == 0) {
            opts = 0;
            continue;
        }
        if (opts && argv[i][0] == '-' && argv[i][1] != '\0') continue;
        rc |= wc_file(argv[i], argv[i]);
    }
    if (files > 1) wc_print(wc_total, "total");
    return rc;
}
