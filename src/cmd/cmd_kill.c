#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "cu.h"

static int parse_int(const char *s, int *out) {
    int sign;
    int val;
    int i;

    if (!s || !out || !s[0]) return -1;
    sign = 1;
    val = 0;
    i = 0;
    if (s[i] == '-') {
        sign = -1;
        i++;
    }
    if (!s[i]) return -1;
    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') return -1;
        val = val * 10 + (s[i] - '0');
        i++;
    }
    *out = val * sign;
    return 0;
}

int cmd_kill(int argc, char **argv) {
    int sig;
    int pid;
    int argi;
    int ret;

    sig = SIGTERM;
    pid = 0;
    argi = 1;
    if (argc < 2) {
        fprintf(stderr, "Usage: kill [-signal] pid\n");
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: kill [-signal] pid");
        return 0;
    }
    if (argv[1][0] == '-' && argv[1][1]) {
        if (parse_int(argv[1] + 1, &sig) < 0 || sig <= 0) {
            fprintf(stderr, "kill: invalid signal\n");
            return 1;
        }
        argi = 2;
    }
    if (argi >= argc || parse_int(argv[argi], &pid) < 0) {
        fprintf(stderr, "kill: invalid pid\n");
        return 1;
    }
    ret = kill(pid, sig);
    if (ret < 0) {
        fprintf(stderr, "kill: failed\n");
        return 1;
    }
    return 0;
}
