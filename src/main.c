#include <stdio.h>
#include <string.h>
#include "cu.h"

static void cu_print_help(void) {
    puts("Lebirun Coreutils Help");
    puts("");
    puts("Commands: echo pwd ls cat touch mkdir rm write ticks");
    puts("");
    puts("Command Options:");
    puts("  echo [-n] [-e]    -n: no newline, -e: interpret escapes");
    puts("  ls [-a] [-l]      -a: show all, -l: long format");
    puts("  cat [-n]          -n: show line numbers");
    puts("  rm [-f]           -f: force (ignore errors)");
    puts("  mkdir [-p]        -p: create parents");
    puts("  touch [-c]        -c: do not create");
    puts("  write [-a]        -a: append mode");
    puts("");
    puts("Usage: lebcu <command> [args...]");
}

int cu_main(int argc, char **argv) {
    const char *applet = cu_basename((argc > 0 && argv) ? argv[0] : "");

    if (!applet || !*applet) return 1;

    if (strcmp(applet, "lebcu") == 0 || strcmp(applet, "lebcu.bin") == 0) {
        if (argc < 2) {
            cu_print_help();
            return 1;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            cu_print_help();
            return 0;
        }
        return cu_dispatch(argc - 1, argv + 1);
    }

    if (strcmp(applet, "--help") == 0 || strcmp(applet, "-h") == 0) {
        cu_print_help();
        return 0;
    }

    return cu_dispatch(argc, argv);
}
