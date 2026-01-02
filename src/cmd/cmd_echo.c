#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

static void cu_expand(const char *in) {
    const char *p = in;
    while (*p) {
        if (*p != '$') {
            putchar(*p++);
            continue;
        }
        p++;
        if (*p == '{') {
            p++;
            char name[64];
            unsigned int ni = 0;
            while (*p && *p != '}' && ni + 1 < sizeof(name)) name[ni++] = *p++;
            name[ni] = '\0';
            if (*p == '}') p++;
            const char *val = getenv(name);
            if (val) fputs(val, stdout);
            continue;
        }
        if (*p == '$') {
            putchar('$');
            p++;
            continue;
        }
        char name[64];
        unsigned int ni = 0;
        while (*p && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_') && ni + 1 < sizeof(name)) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        if (ni == 0) {
            putchar('$');
            continue;
        }
        const char *val = getenv(name);
        if (val) fputs(val, stdout);
    }
}

int cmd_echo(int argc, char **argv) {
    int no_newline = 0;
    int interpret_escapes = 0;
    int first_arg = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && first_arg) {
            int valid_flag = 1;
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p == 'n') no_newline = 1;
                else if (*p == 'e') interpret_escapes = 1;
                else { valid_flag = 0; break; }
            }
            if (valid_flag && argv[i][1]) continue;
        }
        first_arg = 0;
    }

    first_arg = 1;
    int printed = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && first_arg) {
            int valid_flag = 1;
            for (const char *p = argv[i] + 1; *p; p++) {
                if (*p != 'n' && *p != 'e') { valid_flag = 0; break; }
            }
            if (valid_flag && argv[i][1]) continue;
        }
        first_arg = 0;
        if (printed) putchar(' ');
        if (argv[i]) {
            if (interpret_escapes) {
                for (const char *p = argv[i]; *p; p++) {
                    if (*p == '\\' && *(p + 1)) {
                        p++;
                        switch (*p) {
                            case 'n': putchar('\n'); break;
                            case 't': putchar('\t'); break;
                            case 'r': putchar('\r'); break;
                            case '\\': putchar('\\'); break;
                            default: putchar('\\'); putchar(*p); break;
                        }
                    } else {
                        putchar(*p);
                    }
                }
            } else {
                cu_expand(argv[i]);
            }
        }
        printed = 1;
    }
    if (!no_newline) putchar('\n');
    return 0;
}
