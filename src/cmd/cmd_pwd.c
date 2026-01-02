#include <stdio.h>
#include <unistd.h>
#include "cu.h"

int cmd_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char buf[256];
    if (!getcwd(buf, sizeof(buf))) return 1;
    puts(buf);
    return 0;
}
