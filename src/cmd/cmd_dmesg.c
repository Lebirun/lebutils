#include <stdio.h>
#include <string.h>
#include "cu.h"

#define O_RDONLY 0
#define DMESG_BUFSZ 4096

int cmd_dmesg(int argc, char **argv) {
    int fd;
    int rd;
    char buf[DMESG_BUFSZ];

    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: dmesg");
        puts("Print kernel ring buffer messages.");
        return 0;
    }

    fd = vfs_open("/proc/kmsg", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "dmesg: cannot open /proc/kmsg\n");
        return 1;
    }

    while ((rd = vfs_read_fd(fd, buf, DMESG_BUFSZ)) > 0) {
        fwrite(buf, 1, rd, stdout);
    }

    vfs_close_fd(fd);
    return 0;
}
