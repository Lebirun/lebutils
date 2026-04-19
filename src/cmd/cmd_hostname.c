#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "cu.h"

int cmd_hostname(int argc, char **argv) {
    struct utsname name;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: hostname");
        puts("Print the system hostname.");
        return 0;
    }

    if (uname(&name) < 0) {
        fprintf(stderr, "hostname: cannot get hostname\n");
        return 1;
    }

    puts(name.nodename);
    return 0;
}
