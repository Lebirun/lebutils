#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

#define O_RDONLY 0
#define GREP_BUFSZ 1024
#define GREP_MAXLINE 2048

static int grep_match(const char *line, const char *pattern, int ignore_case) {
    const char *l;
    const char *p;
    const char *ls;
    char lc;
    char pc;

    for (ls = line; *ls; ls++) {
        l = ls;
        p = pattern;
        while (*p) {
            lc = *l;
            pc = *p;
            if (ignore_case) {
                if (lc >= 'A' && lc <= 'Z') lc += 32;
                if (pc >= 'A' && pc <= 'Z') pc += 32;
            }
            if (lc != pc) break;
            l++;
            p++;
        }
        if (*p == '\0') return 1;
    }
    return 0;
}

int cmd_grep(int argc, char **argv) {
    int i;
    int ignore_case;
    int invert;
    int show_line_num;
    int count_only;
    const char *pattern;
    char path[256];
    int fd;
    int rd;
    char filebuf[GREP_BUFSZ];
    char linebuf[GREP_MAXLINE];
    int linepos;
    int linenum;
    int match_count;
    int matched;
    int j;
    const char *p;

    ignore_case = 0;
    invert = 0;
    show_line_num = 0;
    count_only = 0;
    pattern = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: grep [-invc] PATTERN [file]");
            puts("Search for PATTERN in file.");
            puts("  -i  ignore case");
            puts("  -n  show line numbers");
            puts("  -v  invert match");
            puts("  -c  count matches only");
            return 0;
        }
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'i') ignore_case = 1;
                else if (*p == 'n') show_line_num = 1;
                else if (*p == 'v') invert = 1;
                else if (*p == 'c') count_only = 1;
            }
        } else if (!pattern) {
            pattern = argv[i];
        }
    }

    if (!pattern) {
        fprintf(stderr, "grep: missing pattern\n");
        return 2;
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        if (argv[i] == pattern) continue;

        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            fprintf(stderr, "grep: invalid path '%s'\n", argv[i]);
            return 2;
        }

        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "grep: cannot open '%s'\n", path);
            return 2;
        }

        linepos = 0;
        linenum = 1;
        match_count = 0;

        while ((rd = vfs_read_fd(fd, filebuf, GREP_BUFSZ)) > 0) {
            for (j = 0; j < rd; j++) {
                if (filebuf[j] == '\n' || linepos >= GREP_MAXLINE - 1) {
                    linebuf[linepos] = '\0';
                    matched = grep_match(linebuf, pattern, ignore_case);
                    if (invert) matched = !matched;
                    if (matched) {
                        match_count++;
                        if (!count_only) {
                            if (show_line_num) printf("%d:", linenum);
                            puts(linebuf);
                        }
                    }
                    linepos = 0;
                    linenum++;
                } else {
                    linebuf[linepos++] = filebuf[j];
                }
            }
        }

        if (linepos > 0) {
            linebuf[linepos] = '\0';
            matched = grep_match(linebuf, pattern, ignore_case);
            if (invert) matched = !matched;
            if (matched) {
                match_count++;
                if (!count_only) {
                    if (show_line_num) printf("%d:", linenum);
                    puts(linebuf);
                }
            }
        }

        vfs_close_fd(fd);

        if (count_only) printf("%d\n", match_count);
        return (match_count > 0) ? 0 : 1;
    }

    linepos = 0;
    linenum = 1;
    match_count = 0;

    while ((rd = read(0, filebuf, GREP_BUFSZ)) > 0) {
        for (j = 0; j < rd; j++) {
            if (filebuf[j] == '\n' || linepos >= GREP_MAXLINE - 1) {
                linebuf[linepos] = '\0';
                matched = grep_match(linebuf, pattern, ignore_case);
                if (invert) matched = !matched;
                if (matched) {
                    match_count++;
                    if (!count_only) {
                        if (show_line_num) printf("%d:", linenum);
                        puts(linebuf);
                    }
                }
                linepos = 0;
                linenum++;
            } else {
                linebuf[linepos++] = filebuf[j];
            }
        }
    }

    if (linepos > 0) {
        linebuf[linepos] = '\0';
        matched = grep_match(linebuf, pattern, ignore_case);
        if (invert) matched = !matched;
        if (matched) {
            match_count++;
            if (!count_only) {
                if (show_line_num) printf("%d:", linenum);
                puts(linebuf);
            }
        }
    }

    if (count_only) printf("%d\n", match_count);
    return (match_count > 0) ? 0 : 1;
}
