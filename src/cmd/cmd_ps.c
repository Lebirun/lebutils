#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

#define O_RDONLY 0
#define PS_MAXPROCS 256
#define PS_BUFSZ 512

struct ps_entry {
    int pid;
    int ppid;
    char name[32];
    char state;
    unsigned long vsize;
};

static int ps_read_file(const char *path, char *buf, int bufsz) {
    int fd;
    int rd;
    int total;

    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -1;

    total = 0;
    while (total < bufsz - 1 && (rd = vfs_read_fd(fd, buf + total, bufsz - 1 - total)) > 0) {
        total += rd;
    }
    buf[total] = '\0';
    vfs_close_fd(fd);
    return total;
}

static void ps_parse_stat(const char *buf, struct ps_entry *e) {
    const char *p;
    const char *name_start;
    const char *name_end;
    int field;
    int ni;

    p = buf;
    e->pid = 0;
    while (*p >= '0' && *p <= '9') {
        e->pid = e->pid * 10 + (*p - '0');
        p++;
    }
    while (*p == ' ') p++;

    if (*p == '(') {
        p++;
        name_start = p;
        name_end = p;
        while (*p && *p != ')') {
            name_end = p;
            p++;
        }
        ni = 0;
        while (name_start <= name_end && ni < 31) {
            e->name[ni++] = *name_start++;
        }
        e->name[ni] = '\0';
        if (*p == ')') p++;
        while (*p == ' ') p++;
    }

    e->state = *p ? *p : '?';
    if (*p) p++;
    while (*p == ' ') p++;

    e->ppid = 0;
    while (*p >= '0' && *p <= '9') {
        e->ppid = e->ppid * 10 + (*p - '0');
        p++;
    }

    field = 4;
    while (*p && field < 22) {
        while (*p == ' ') p++;
        while (*p && *p != ' ') p++;
        field++;
    }
    while (*p == ' ') p++;

    e->vsize = 0;
    while (*p >= '0' && *p <= '9') {
        e->vsize = e->vsize * 10 + (*p - '0');
        p++;
    }
}

int cmd_ps(int argc, char **argv) {
    int i;
    int fd;
    char name[64];
    unsigned int type;
    unsigned int idx;
    char path[128];
    char statbuf[PS_BUFSZ];
    struct ps_entry entries[PS_MAXPROCS];
    int count;
    int show_all;

    show_all = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: ps [-a]");
            puts("List running processes.");
            puts("  -a  show all processes");
            return 0;
        }
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-e") == 0) {
            show_all = 1;
        }
    }

    fd = vfs_open("/proc", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ps: cannot open /proc\n");
        return 1;
    }

    count = 0;
    idx = 0;
    while (vfs_readdir(fd, name, &type, idx) >= 0) {
        int is_pid;
        const char *np;

        is_pid = 1;
        np = name;
        if (*np == '\0') is_pid = 0;
        while (*np) {
            if (*np < '0' || *np > '9') {
                is_pid = 0;
                break;
            }
            np++;
        }

        if (is_pid && count < PS_MAXPROCS) {
            snprintf(path, sizeof(path), "/proc/%s/stat", name);
            if (ps_read_file(path, statbuf, PS_BUFSZ) > 0) {
                memset(&entries[count], 0, sizeof(entries[count]));
                ps_parse_stat(statbuf, &entries[count]);
                count++;
            }
        }
        idx++;
    }
    vfs_close_fd(fd);

    printf("  PID  PPID ST  VSZ COMMAND\n");
    for (i = 0; i < count; i++) {
        printf("%5d %5d  %c %4lu %s\n",
            entries[i].pid,
            entries[i].ppid,
            entries[i].state,
            entries[i].vsize / 1024,
            entries[i].name);
    }

    (void)show_all;
    return 0;
}
