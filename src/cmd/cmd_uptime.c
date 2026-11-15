#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include "cu.h"

static int uptime_read(const char *path, char *buf, int size) {
    int fd, n;

    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -1;
    n = vfs_read_fd(fd, buf, size - 1);
    vfs_close_fd(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
}

int cmd_uptime(int argc, char **argv) {
    char buf[64], *p, *load[3];
    struct tm *tm;
    time_t now;
    unsigned long up, d, h, m;
    const char *sep;
    int pretty, i;

    pretty = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: uptime [-p]");
            puts("Show how long the system has been running.");
            puts("");
            puts("  -p, --pretty  show uptime in pretty format");
            return 0;
        }
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pretty") == 0) pretty = 1;
        else {
            fprintf(stderr, "uptime: invalid option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (uptime_read("/proc/uptime", buf, sizeof(buf)) < 0) {
        fprintf(stderr, "uptime: cannot read /proc/uptime\n");
        return 1;
    }
    up = strtoul(buf, NULL, 10);
    d = up / 86400;
    h = up / 3600 % 24;
    m = up / 60 % 60;

    if (pretty) {
        sep = "";
        fputs("up", stdout);
        if (d) {
            printf(" %lu day%s", d, d == 1 ? "" : "s");
            sep = ",";
        }
        if (h) {
            printf("%s %lu hour%s", sep, h, h == 1 ? "" : "s");
            sep = ",";
        }
        if (m || !*sep) printf("%s %lu minute%s", sep, m, m == 1 ? "" : "s");
        putchar('\n');
        return 0;
    }

    now = time(NULL);
    tm = localtime(&now);
    if (tm) printf(" %02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    fputs(" up ", stdout);
    if (d) printf("%lu day%s, ", d, d == 1 ? "" : "s");
    if (h) printf("%2lu:%02lu", h, m);
    else printf("%lu min", m);
    if (uptime_read("/proc/loadavg", buf, sizeof(buf)) == 0) {
        p = buf;
        for (i = 0; i < 3; i++) {
            while (*p == ' ') p++;
            load[i] = p;
            while (*p && *p != ' ' && *p != '\n') p++;
            if (*p) *p++ = '\0';
        }
        printf(",  load average: %s, %s, %s", load[0], load[1], load[2]);
    }
    putchar('\n');
    return 0;
}
