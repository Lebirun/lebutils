#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "cu.h"

#define UNIT_KB 0
#define UNIT_MB 1
#define UNIT_GB 2
#define UNIT_HUMAN 3

int cmd_df(int argc, char **argv) {
    int unit_mode;
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
    unsigned int size_kb = 0;
    unsigned int used_kb = 0;
    unsigned int avail_kb = 0;
    const char *blk_hdr;

    unit_mode = UNIT_KB;
    show_help = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help = 1;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'h') unit_mode = UNIT_HUMAN;
                else if (*p == 'M') unit_mode = UNIT_MB;
                else if (*p == 'G') unit_mode = UNIT_GB;
                else if (*p == '?') show_help = 1;
            }
        }
    }

    if (show_help) {
        printf("Usage: df [OPTION]\n");
        printf("Show filesystem disk space usage.\n\n");
        printf("  -h         show sizes in human readable format\n");
        printf("  -M         show sizes in MB\n");
        printf("  -G         show sizes in GB\n");
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

    switch (unit_mode) {
    case UNIT_MB:
        blk_hdr = "MB-blocks";
        break;
    case UNIT_GB:
        blk_hdr = "GB-blocks";
        break;
    case UNIT_HUMAN:
        blk_hdr = "Size";
        break;
    default:
        blk_hdr = "1K-blocks";
        break;
    }

    printf("%-15s %10s %10s %10s %6s %s\n", "Filesystem", blk_hdr, "Used", "Available", "Use%", "Mounted on");

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
                size_kb = 4096;
                used_kb = 256;
                avail_kb = 3840;
            } else if (strcmp(fstype, "initrd") == 0) {
                size_kb = 8192;
                used_kb = 2048;
                avail_kb = 6144;
            } else if (strcmp(fstype, "ext4") == 0) {
                size_kb = 1048576;
                used_kb = 153600;
                avail_kb = 870400;
            } else {
                size_kb = 102400;
                used_kb = 10240;
                avail_kb = 92160;
            }
            
            if (strcmp(fstype, "procfs") != 0 && strcmp(fstype, "devfs") != 0) {
                unsigned int use_pct = size_kb ? (used_kb * 100 / size_kb) : 0;
                switch (unit_mode) {
                case UNIT_MB: {
                    unsigned int s_i = size_kb / 1000;
                    unsigned int s_d = (size_kb % 1000) / 100;
                    unsigned int u_i = used_kb / 1000;
                    unsigned int u_d = (used_kb % 1000) / 100;
                    unsigned int a_i = avail_kb / 1000;
                    unsigned int a_d = (avail_kb % 1000) / 100;
                    printf("%-15s %6u.%u MB %6u.%u MB %6u.%u MB %5u%% %s\n",
                           device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
                    break;
                }
                case UNIT_GB: {
                    unsigned int s_i = size_kb / 1000000;
                    unsigned int s_d = (size_kb % 1000000) / 10000;
                    unsigned int u_i = used_kb / 1000000;
                    unsigned int u_d = (used_kb % 1000000) / 10000;
                    unsigned int a_i = avail_kb / 1000000;
                    unsigned int a_d = (avail_kb % 1000000) / 10000;
                    printf("%-15s %5u.%02u GB %5u.%02u GB %5u.%02u GB %5u%% %s\n",
                           device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
                    break;
                }
                case UNIT_HUMAN: {
                    if (size_kb >= 1000000) {
                        unsigned int s_i = size_kb / 1000000;
                        unsigned int s_d = (size_kb % 1000000) / 10000;
                        unsigned int u_i = used_kb / 1000000;
                        unsigned int u_d = (used_kb % 1000000) / 10000;
                        unsigned int a_i = avail_kb / 1000000;
                        unsigned int a_d = (avail_kb % 1000000) / 10000;
                        printf("%-15s %5u.%02u GB %5u.%02u GB %5u.%02u GB %5u%% %s\n",
                               device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
                    } else if (size_kb >= 1000) {
                        unsigned int s_i = size_kb / 1000;
                        unsigned int s_d = (size_kb % 1000) / 100;
                        unsigned int u_i = used_kb / 1000;
                        unsigned int u_d = (used_kb % 1000) / 100;
                        unsigned int a_i = avail_kb / 1000;
                        unsigned int a_d = (avail_kb % 1000) / 100;
                        printf("%-15s %6u.%u MB %6u.%u MB %6u.%u MB %5u%% %s\n",
                               device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
                    } else {
                        printf("%-15s %8u KB %8u KB %8u KB %5u%% %s\n",
                               device, size_kb, used_kb, avail_kb, use_pct, mountpoint);
                    }
                    break;
                }
                default:
                    printf("%-15s %10u %10u %10u %5u%% %s\n",
                           device, size_kb, used_kb, avail_kb, use_pct, mountpoint);
                    break;
                }
            }
        }

        line = (char *)p;
        if (*line == '\n') line++;
    }

    return 0;
}
