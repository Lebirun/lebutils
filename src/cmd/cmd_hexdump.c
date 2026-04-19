#include <stdio.h>
#include <string.h>
#include "cu.h"

#define O_RDONLY 0
#define HEX_BUFSZ 256
#define HEX_COLS 16

int cmd_hexdump(int argc, char **argv) {
    int i;
    int fd;
    int rd;
    int j;
    int offset;
    char path[256];
    unsigned char buf[HEX_BUFSZ];
    int canonical;

    canonical = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: hexdump [-C] <file>");
            puts("Display file contents in hexadecimal.");
            puts("  -C  canonical hex+ASCII display");
            return 0;
        }
        if (strcmp(argv[i], "-C") == 0) {
            canonical = 1;
        }
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;

        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            fprintf(stderr, "hexdump: invalid path '%s'\n", argv[i]);
            return 1;
        }

        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "hexdump: cannot open '%s'\n", path);
            return 1;
        }

        offset = 0;
        while ((rd = vfs_read_fd(fd, buf, HEX_BUFSZ)) > 0) {
            for (j = 0; j < rd; j += HEX_COLS) {
                int k;
                int cols;

                cols = rd - j;
                if (cols > HEX_COLS) cols = HEX_COLS;

                printf("%08x  ", offset + j);

                for (k = 0; k < HEX_COLS; k++) {
                    if (k == 8) putchar(' ');
                    if (k < cols) {
                        printf("%02x ", buf[j + k]);
                    } else {
                        printf("   ");
                    }
                }

                if (canonical) {
                    printf(" |");
                    for (k = 0; k < cols; k++) {
                        unsigned char c;
                        c = buf[j + k];
                        if (c >= 0x20 && c <= 0x7e) {
                            putchar(c);
                        } else {
                            putchar('.');
                        }
                    }
                    putchar('|');
                }

                putchar('\n');
            }
            offset += rd;
        }

        vfs_close_fd(fd);
        return 0;
    }

    fprintf(stderr, "hexdump: missing file operand\n");
    return 1;
}
