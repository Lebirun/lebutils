#include <stdio.h>
#include <string.h>
#include "cu.h"

static void cu_print_help(void) {
    puts("Lebirun Lebutils Help");
    puts("");
    puts("Commands:");
    cu_print_commands();
    puts("");
    puts("Lebu Options:");
    puts("  -h, --help       show this help");
    puts("");
    puts("Usage: lebu <command> [args...]");
    puts("Run <command> --help for command-specific help.");
}

int cu_main(int argc, char **argv) {
    const char *applet = cu_basename((argc > 0 && argv) ? argv[0] : "");

    if (!applet || !*applet) return 1;

    if (strcmp(applet, "lebu") == 0 || strcmp(applet, "lebu.bin") == 0) {
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
