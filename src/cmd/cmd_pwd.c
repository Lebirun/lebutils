#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cu.h"

int cmd_pwd(int argc, char **argv) {
    int i;
    char buf[256];

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: pwd");
            puts("Print the current working directory.");
            puts("");
            puts("  -h, --help  display this help and exit");
            return 0;
        }
    }

    if (!getcwd(buf, sizeof(buf))) return 1;
    puts(buf);
    return 0;
}
