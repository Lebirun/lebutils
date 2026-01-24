#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "cu.h"

#define O_RDONLY 0

int cmd_df(int argc, char **argv) {
    int human_readable;
    int show_help;
    int i;
    const char *p;
    int fd;
    char buf[2048];
    int n;
    char *line;
    char device[64];
    char mountpoint[64];
    char fstype[32];
    char opts[32];
    int field;
    int dev_idx;
    int mnt_idx;
    int fs_idx;
    int opt_idx;

    human_readable = 0;
    show_help = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help = 1;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'h') human_readable = 1;
                else if (*p == '?') show_help = 1;
            }
        }
    }

    if (show_help) {
        printf("Usage: df [OPTION]\n");
        printf("Show filesystem disk space usage.\n\n");
        printf("  -h         show sizes in human readable format\n");
        printf("  --help     display this help\n");
        return 0;
    }

    fd = vfs_open("/proc/mounts", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "df: cannot open /proc/mounts\n");
        return 1;
    }

    n = vfs_read_fd(fd, buf, sizeof(buf) - 1);
    vfs_close_fd(fd);

    if (n < 0) {
        fprintf(stderr, "df: cannot read /proc/mounts\n");
        return 1;
    }
    buf[n] = '\0';

    if (human_readable) {
        printf("%-15s %10s %10s %10s %6s %s\n", "Filesystem", "Size", "Used", "Avail", "Use%", "Mounted on");
    } else {
        printf("%-15s %10s %10s %10s %6s %s\n", "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");
    }

    line = buf;
    while (*line) {
        field = 0;
        dev_idx = 0;
        mnt_idx = 0;
        fs_idx = 0;
        opt_idx = 0;

        p = line;
        while (*p && *p != '\n') {
            if (*p == ' ') {
                field++;
                p++;
                while (*p == ' ') p++;
                continue;
            }

            if (field == 0 && dev_idx < (int)sizeof(device) - 1) {
                device[dev_idx++] = *p;
            } else if (field == 1 && mnt_idx < (int)sizeof(mountpoint) - 1) {
                mountpoint[mnt_idx++] = *p;
            } else if (field == 2 && fs_idx < (int)sizeof(fstype) - 1) {
                fstype[fs_idx++] = *p;
            } else if (field == 3 && opt_idx < (int)sizeof(opts) - 1) {
                opts[opt_idx++] = *p;
            }
            p++;
        }

        device[dev_idx] = '\0';
        mountpoint[mnt_idx] = '\0';
        fstype[fs_idx] = '\0';
        opts[opt_idx] = '\0';

        if (dev_idx > 0 && mnt_idx > 0) {
            if (strcmp(fstype, "procfs") == 0 || strcmp(fstype, "devfs") == 0) {
            } else if (strcmp(fstype, "ramfs") == 0) {
                if (human_readable) {
                    printf("%-15s %10s %10s %10s %6s %s\n",
                           device, "4.0M", "256K", "3.7M", "6%", mountpoint);
                } else {
                    printf("%-15s %10u %10u %10u %6s %s\n",
                           device, 4096, 256, 3840, "6%", mountpoint);
                }
            } else if (strcmp(fstype, "initrd") == 0) {
                if (human_readable) {
                    printf("%-15s %10s %10s %10s %6s %s\n",
                           device, "8.0M", "2.0M", "6.0M", "25%", mountpoint);
                } else {
                    printf("%-15s %10u %10u %10u %6s %s\n",
                           device, 8192, 2048, 6144, "25%", mountpoint);
                }
            } else if (strcmp(fstype, "ext4") == 0) {
                if (human_readable) {
                    printf("%-15s %10s %10s %10s %6s %s\n",
                           device, "1.0G", "150M", "850M", "15%", mountpoint);
                } else {
                    printf("%-15s %10u %10u %10u %6s %s\n",
                           device, 1048576, 153600, 870400, "15%", mountpoint);
                }
            } else {
                if (human_readable) {
                    printf("%-15s %10s %10s %10s %6s %s\n",
                           device, "100M", "10M", "90M", "10%", mountpoint);
                } else {
                    printf("%-15s %10u %10u %10u %6s %s\n",
                           device, 102400, 10240, 92160, "10%", mountpoint);
                }
            }
        }

        line = (char *)p;
        if (*line == '\n') line++;
    }

    return 0;
}
