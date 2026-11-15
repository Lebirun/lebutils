#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "cu.h"

int cmd_chroot(int argc, char **argv) {
    char *shell[3];

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: chroot NEWROOT [COMMAND [ARG]...]");
        puts("Run COMMAND with root directory set to NEWROOT.");
        puts("If no command is given, run '$SHELL -i' (default: /bin/sh -i).");
        return argc < 2;
    }
    if (chroot(argv[1]) < 0) {
        fprintf(stderr, "chroot: cannot change root directory to '%s': %s\n", argv[1], strerror(errno));
        return 125;
    }
    if (chdir("/") < 0) {
        fprintf(stderr, "chroot: cannot change directory to '/': %s\n", strerror(errno));
        return 125;
    }
    if (argc > 2) {
        execvp(argv[2], argv + 2);
        fprintf(stderr, "chroot: failed to run command '%s': %s\n", argv[2], strerror(errno));
        return errno == ENOENT ? 127 : 126;
    }
    shell[0] = getenv("SHELL");
    if (!shell[0] || !*shell[0]) shell[0] = "/bin/sh";
    shell[1] = "-i";
    shell[2] = NULL;
    execv(shell[0], shell);
    fprintf(stderr, "chroot: failed to run command '%s': %s\n", shell[0], strerror(errno));
    return errno == ENOENT ? 127 : 126;
}
