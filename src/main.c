#include <stdio.h>
#include <string.h>
#include "cu.h"

static void cu_print_help(void) {
    puts("Lebirun Coreutils Help");
    puts("");
    puts("Commands:");
    cu_print_commands();
    puts("");
    puts("Command Options:");
#ifdef CONFIG_CMD_ECHO
    puts("  echo [-n] [-e]    -n: no newline, -e: interpret escapes");
#endif
#ifdef CONFIG_CMD_LS
    puts("  ls [-a] [-l]      -a: show all, -l: long format");
#endif
#ifdef CONFIG_CMD_CAT
    puts("  cat [-n]          -n: show line numbers");
#endif
#ifdef CONFIG_CMD_RM
    puts("  rm [-f]           -f: force (ignore errors)");
#endif
#ifdef CONFIG_CMD_MKDIR
    puts("  mkdir [-p]        -p: create parents");
#endif
#ifdef CONFIG_CMD_TOUCH
    puts("  touch [-c]        -c: do not create");
#endif
#ifdef CONFIG_CMD_WRITE
    puts("  write [-a]        -a: append mode");
#endif
#ifdef CONFIG_CMD_FREE
    puts("  free [-h]         -h: human-readable output");
#endif
#ifdef CONFIG_CMD_DF
    puts("  df [-h]           -h: human-readable output");
#endif
#ifdef CONFIG_CMD_UNAME
    puts("  uname [-asnrvm]   -a: all, -s: sysname, -n: nodename");
#endif
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
