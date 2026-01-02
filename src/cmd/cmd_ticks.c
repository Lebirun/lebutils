#include <stdio.h>
#include <unistd.h>
#include "cu.h"

int cmd_ticks(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("%u\n", getticks());
    return 0;
}
