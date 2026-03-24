#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "cu.h"

static int parse_owner_group(const char *s, int *owner, int *group) {
    const char *colon;
    char buf[32];
    unsigned int len;

    *owner = -1;
    *group = -1;

    colon = strchr(s, ':');
    if (!colon) {
        *owner = atoi(s);
        return 0;
    }

    len = (unsigned int)(colon - s);
    if (len > 0) {
        if (len >= sizeof(buf)) return -1;
        memcpy(buf, s, len);
        buf[len] = '\0';
        *owner = atoi(buf);
    }
    if (colon[1] != '\0') {
        *group = atoi(colon + 1);
    }
    return 0;
}

int cmd_chown(int argc, char **argv) {
    int i, owner, group, rc;
    char path[256];

    if (argc < 3) {
        fprintf(stderr, "Usage: chown OWNER[:GROUP] FILE...\n");
        return 1;
    }

    if (parse_owner_group(argv[1], &owner, &group) != 0) {
        fprintf(stderr, "chown: invalid owner '%s'\n", argv[1]);
        return 1;
    }

    rc = 0;
    for (i = 2; i < argc; i++) {
        if (cu_path_abs(argv[i], path, sizeof(path)) != 0) {
            fprintf(stderr, "chown: path too long: %s\n", argv[i]);
            rc = 1;
            continue;
        }
        if (chown(path, owner, group) != 0) {
            fprintf(stderr, "chown: cannot change owner of '%s'\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
