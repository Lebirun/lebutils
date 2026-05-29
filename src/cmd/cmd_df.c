#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include "cu.h"

#define UNIT_KB 0
#define UNIT_MB 1
#define UNIT_GB 2
#define UNIT_HUMAN 3

static unsigned long long blocks_to_kb(unsigned long long blocks, unsigned long frsize)
{
    unsigned long long scale;

    if (frsize >= 1024) {
        scale = frsize / 1024;
        if (scale == 0) scale = 1;
        return blocks * scale;
    }

    return (blocks * (unsigned long long)frsize + 1023ULL) / 1024ULL;
}

static void fmt_human(char *out, int outsz, unsigned long long kb)
{
    unsigned long long i;
    unsigned long long d;

    if (kb >= 1073741824ULL) {
        i = kb / 1073741824ULL;
        d = (kb % 1073741824ULL) * 100ULL / 1073741824ULL;
        snprintf(out, outsz, "%llu.%02llu TB", i, d);
    } else if (kb >= 1048576ULL) {
        i = kb / 1048576ULL;
        d = (kb % 1048576ULL) * 100ULL / 1048576ULL;
        snprintf(out, outsz, "%llu.%02llu GB", i, d);
    } else if (kb >= 1024ULL) {
        i = kb / 1024ULL;
        d = (kb % 1024ULL) * 10ULL / 1024ULL;
        snprintf(out, outsz, "%llu.%llu MB", i, d);
    } else {
        snprintf(out, outsz, "%llu KB", kb);
    }
}

static void print_usage(void)
{
    printf("Usage: df [OPTION] [FILE]\n");
    printf("Show filesystem disk space usage.\n\n");
    printf("  -h         show sizes in human readable format\n");
    printf("  -M         show sizes in MB\n");
    printf("  -G         show sizes in GB\n");
    printf("  --help     display this help\n");
}

static void print_row(const char *device, const char *mountpoint, int unit_mode,
                      unsigned long long size_kb,
                      unsigned long long used_kb,
                      unsigned long long avail_kb)
{
    unsigned long long use_pct;
    unsigned long long s_i;
    unsigned long long s_d;
    unsigned long long u_i;
    unsigned long long u_d;
    unsigned long long a_i;
    unsigned long long a_d;
    char s_buf[32];
    char u_buf[32];
    char a_buf[32];

    use_pct = size_kb ? (used_kb * 100ULL / size_kb) : 0;

    switch (unit_mode) {
    case UNIT_MB:
        s_i = size_kb / 1024ULL;
        s_d = (size_kb % 1024ULL) * 10ULL / 1024ULL;
        u_i = used_kb / 1024ULL;
        u_d = (used_kb % 1024ULL) * 10ULL / 1024ULL;
        a_i = avail_kb / 1024ULL;
        a_d = (avail_kb % 1024ULL) * 10ULL / 1024ULL;
        printf("%-15s %6llu.%llu MB %6llu.%llu MB %6llu.%llu MB %5llu%% %s\n",
               device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
        break;
    case UNIT_GB:
        s_i = size_kb / 1048576ULL;
        s_d = (size_kb % 1048576ULL) * 100ULL / 1048576ULL;
        u_i = used_kb / 1048576ULL;
        u_d = (used_kb % 1048576ULL) * 100ULL / 1048576ULL;
        a_i = avail_kb / 1048576ULL;
        a_d = (avail_kb % 1048576ULL) * 100ULL / 1048576ULL;
        printf("%-15s %5llu.%02llu GB %5llu.%02llu GB %5llu.%02llu GB %5llu%% %s\n",
               device, s_i, s_d, u_i, u_d, a_i, a_d, use_pct, mountpoint);
        break;
    case UNIT_HUMAN:
        fmt_human(s_buf, sizeof(s_buf), size_kb);
        fmt_human(u_buf, sizeof(u_buf), used_kb);
        fmt_human(a_buf, sizeof(a_buf), avail_kb);
        printf("%-15s %10s %10s %10s %5llu%% %s\n",
               device, s_buf, u_buf, a_buf, use_pct, mountpoint);
        break;
    default:
        printf("%-15s %10llu %10llu %10llu %5llu%% %s\n",
               device, size_kb, used_kb, avail_kb, use_pct, mountpoint);
        break;
    }
}

static int print_statvfs_path(const char *device, const char *mountpoint,
                              int unit_mode)
{
    struct statvfs stfs;
    unsigned long long size_kb;
    unsigned long long used_kb;
    unsigned long long avail_kb;

    if (statvfs(mountpoint, &stfs) != 0 ||
        stfs.f_frsize == 0 || stfs.f_blocks == 0) {
        return -1;
    }

    size_kb = blocks_to_kb((unsigned long long)stfs.f_blocks, stfs.f_frsize);
    avail_kb = blocks_to_kb((unsigned long long)stfs.f_bavail, stfs.f_frsize);
    used_kb = 0;
    if (avail_kb <= size_kb) {
        used_kb = size_kb - avail_kb;
    }

    print_row(device, mountpoint, unit_mode, size_kb, used_kb, avail_kb);
    return 0;
}

int cmd_df(int argc, char **argv)
{
    int unit_mode;
    int show_help;
    int i;
    const char *p;
    const char *path_arg;
    const char *blk_hdr;
    int fd;
    char buf[2048];
    int n;
    char *line;
    char device[128];
    char mountpoint[128];
    char fstype[32];
    char opts[32];
    int field;
    int dev_idx;
    int mnt_idx;
    int fs_idx;
    int opt_idx;
    struct statvfs stfs;
    unsigned long long size_kb;
    unsigned long long used_kb;
    unsigned long long avail_kb;

    unit_mode = UNIT_KB;
    show_help = 0;
    path_arg = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help = 1;
            continue;
        }
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'h') unit_mode = UNIT_HUMAN;
                else if (*p == 'M') unit_mode = UNIT_MB;
                else if (*p == 'G') unit_mode = UNIT_GB;
                else if (*p == '?') show_help = 1;
            }
        } else {
            path_arg = argv[i];
        }
    }

    if (show_help) {
        print_usage();
        return 0;
    }

    if (!path_arg) {
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
    }

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

    printf("%-15s %10s %10s %10s %6s %s\n",
           "Filesystem", blk_hdr, "Used", "Available", "Use%", "Mounted on");

    if (path_arg) {
        if (print_statvfs_path(path_arg, path_arg, unit_mode) == 0) {
            return 0;
        }
        fprintf(stderr, "df: %s: statvfs failed\n", path_arg);
        return 1;
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
            if (*p == ' ' || *p == '\t') {
                field++;
                p++;
                while (*p == ' ' || *p == '\t') p++;
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

        if (dev_idx > 0 && mnt_idx > 0 && strcmp(fstype, "squashfs") != 0) {
            size_kb = 0;
            used_kb = 0;
            avail_kb = 0;
            if (statvfs(mountpoint, &stfs) == 0 &&
                stfs.f_frsize > 0 && stfs.f_blocks > 0) {
                size_kb = blocks_to_kb((unsigned long long)stfs.f_blocks,
                                       stfs.f_frsize);
                avail_kb = blocks_to_kb((unsigned long long)stfs.f_bavail,
                                        stfs.f_frsize);
                if (avail_kb <= size_kb) {
                    used_kb = size_kb - avail_kb;
                }
            }
            print_row(device, mountpoint, unit_mode, size_kb, used_kb, avail_kb);
        }

        line = (char *)p;
        if (*line == '\n') line++;
    }

    return 0;
}
