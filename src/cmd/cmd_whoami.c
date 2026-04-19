#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "cu.h"

int cmd_whoami(int argc, char **argv) {
    uid_t uid;
    struct passwd *pw;

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: whoami");
        puts("Print the current user name.");
        return 0;
    }

    uid = getuid();
    pw = getpwuid(uid);
    if (pw) {
        puts(pw->pw_name);
    } else {
        printf("%u\n", (unsigned)uid);
    }
    return 0;
}
