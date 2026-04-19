#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cu.h"

#define O_RDONLY 0
#define FILE_HDR_SZ 264

struct file_magic {
    const unsigned char *magic;
    int len;
    const char *desc;
};

static const unsigned char mag_elf[] = {0x7f, 'E', 'L', 'F'};
static const unsigned char mag_png[] = {0x89, 'P', 'N', 'G'};
static const unsigned char mag_gz[]  = {0x1f, 0x8b};
static const unsigned char mag_bz2[] = {'B', 'Z', 'h'};
static const unsigned char mag_xz[]  = {0xfd, '7', 'z', 'X', 'Z'};
static const unsigned char mag_zip[] = {'P', 'K', 0x03, 0x04};
static const unsigned char mag_pdf[] = {'%', 'P', 'D', 'F'};
static const unsigned char mag_sh[]  = {'#', '!'};
static const unsigned char mag_sqsh[] = {'h', 's', 'q', 's'};
static const unsigned char mag_tar[] = {'u', 's', 't', 'a', 'r'};

static const struct file_magic magics[] = {
    {mag_elf,  4, "ELF executable"},
    {mag_png,  4, "PNG image"},
    {mag_gz,   2, "gzip compressed data"},
    {mag_bz2,  3, "bzip2 compressed data"},
    {mag_xz,   5, "XZ compressed data"},
    {mag_zip,  4, "Zip archive"},
    {mag_pdf,  4, "PDF document"},
    {mag_sh,   2, "script"},
    {mag_sqsh, 4, "Squashfs filesystem"},
    {NULL, 0, NULL}
};

static int is_text(const unsigned char *buf, int len) {
    int i;

    for (i = 0; i < len; i++) {
        if (buf[i] == 0) return 0;
        if (buf[i] < 0x20 && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t' && buf[i] != 0x1b) {
            return 0;
        }
    }
    return 1;
}

int cmd_file(int argc, char **argv) {
    int i;
    int fd;
    int rd;
    char path[256];
    unsigned char hdr[FILE_HDR_SZ];
    uint64_t size;
    uint64_t type;
    const char *desc;
    int j;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: file <path>...");
            puts("Determine file type.");
            return 0;
        }
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: file <path>...\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (cu_path_abs(argv[i], path, sizeof(path)) < 0) {
            printf("%s: cannot resolve path\n", argv[i]);
            continue;
        }

        fd = vfs_open(path, O_RDONLY);
        if (fd < 0) {
            printf("%s: cannot open\n", argv[i]);
            continue;
        }

        size = 0;
        type = 0;
        vfs_stat(fd, &size, &type);

        if (type & 0x02) {
            printf("%s: directory\n", argv[i]);
            vfs_close_fd(fd);
            continue;
        }

        if (size == 0) {
            printf("%s: empty\n", argv[i]);
            vfs_close_fd(fd);
            continue;
        }

        rd = vfs_read_fd(fd, hdr, FILE_HDR_SZ);
        vfs_close_fd(fd);

        if (rd <= 0) {
            printf("%s: cannot read\n", argv[i]);
            continue;
        }

        desc = NULL;
        for (j = 0; magics[j].magic; j++) {
            if (rd >= magics[j].len && memcmp(hdr, magics[j].magic, magics[j].len) == 0) {
                desc = magics[j].desc;
                break;
            }
        }

        if (!desc && rd >= 262) {
            if (memcmp(hdr + 257, mag_tar, 5) == 0) {
                desc = "tar archive";
            }
        }

        if (!desc) {
            if (is_text(hdr, rd)) {
                desc = "ASCII text";
            } else {
                desc = "data";
            }
        }

        printf("%s: %s\n", argv[i], desc);
    }

    return 0;
}
