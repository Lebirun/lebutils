#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <lebirun.h>

#define SECT_SIZE 512

static void put16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int write_at(int fd, uint64_t offset, const void *data, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t done = 0;
    int ret;

    if (lseek(fd, (off_t)offset, SEEK_SET) != (off_t)offset) {
        fprintf(stderr, "lformat.fat: seek failed\n");
        return -1;
    }
    while (done < size) {
        ret = vfs_write_fd(fd, bytes + done, size - done);
        if (ret <= 0 || (uint32_t)ret > size - done) {
            fprintf(stderr, "lformat.fat: write failed\n");
            return -1;
        }
        done += (uint32_t)ret;
    }
    return 0;
}

static int write_zero(int fd, uint64_t offset, uint64_t size) {
    static uint8_t zero[SECT_SIZE];
    uint64_t done = 0;
    uint32_t chunk;

    memset(zero, 0, sizeof(zero));
    while (done < size) {
        chunk = sizeof(zero);
        if (chunk > size - done) chunk = (uint32_t)(size - done);
        if (write_at(fd, offset + done, zero, chunk) != 0) return -1;
        done += chunk;
    }
    return 0;
}

static void sfn_label(const char *label, uint8_t out[11]) {
    uint64_t i;
    uint64_t ni = 0;
    char c;

    memset(out, ' ', 11);
    if (!label) return;
    for (i = 0; label[i] && ni < 11; i++) {
        c = label[i];
        if (c == '.') continue;
        if (c >= 'a' && c <= 'z') c -= 32;
        out[ni++] = (uint8_t)c;
    }
}

static uint64_t fat_size(uint64_t sectors, uint64_t reserved,
                         uint64_t entries, uint64_t cluster_sectors) {
    uint64_t low = 1;
    uint64_t high;
    uint64_t mid;
    uint64_t clusters;

    high = ((sectors - reserved) / cluster_sectors + 2 + entries - 1) / entries;
    while (low < high) {
        mid = low + (high - low) / 2;
        clusters = (sectors - reserved - 2 * mid) / cluster_sectors;
        if (mid * entries >= clusters + 2)
            high = mid;
        else
            low = mid + 1;
    }
    return low;
}

int cmd_lformat_fat(int argc, char **argv) {
    const char *devpath;
    const char *label = "";
    uint64_t dev_size;
    uint64_t dev_type;
    uint64_t total_sectors;
    uint64_t clusters;
    uint64_t cluster_sectors = 8;
    uint64_t fat_sectors;
    uint64_t data_start;
    uint64_t reserved;
    uint64_t root_lba;
    uint64_t free_clusters;
    uint8_t boot[SECT_SIZE];
    uint8_t sfn[11];
    uint8_t entry[32];
    int fd;
    int rfd;
    int is_fat32;
    int i;

    if (argc < 2) {
        printf("Usage: lformat.fat <device> [-L label]\n");
        return 1;
    }
    devpath = argv[1];
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            label = argv[++i];
        }
    }

    rfd = vfs_open(devpath, 0);
    if (rfd < 0) {
        printf("lformat.fat: cannot open %s\n", devpath);
        return 1;
    }
    if (vfs_stat(rfd, &dev_size, &dev_type) != 0 || dev_size == 0) {
        vfs_close_fd(rfd);
        printf("lformat.fat: cannot determine size of %s\n", devpath);
        return 1;
    }
    vfs_close_fd(rfd);

    total_sectors = dev_size / SECT_SIZE;
    if (total_sectors > UINT32_MAX) {
        printf("lformat.fat: device exceeds FAT sector count range\n");
        return 1;
    }
    if (total_sectors < 36 + 2 + 4085 * 8) {
        printf("lformat.fat: device too small (%llu sectors)\n",
               (unsigned long long)total_sectors);
        return 1;
    }
    for (;;) {
        fat_sectors = fat_size(total_sectors, 32, SECT_SIZE / 4,
                               cluster_sectors);
        data_start = 32 + 2 * fat_sectors;
        clusters = (total_sectors - data_start) / cluster_sectors;
        if (clusters <= 0x0FFFFFEE) break;
        cluster_sectors *= 2;
    }
    is_fat32 = clusters >= 65525;
    reserved = is_fat32 ? 32 : 4;
    if (!is_fat32) {
        fat_sectors = fat_size(total_sectors, 36, SECT_SIZE / 2,
                               cluster_sectors);
        data_start = 36 + 2 * fat_sectors;
        clusters = (total_sectors - data_start) / 8;
        if (clusters >= 65525) {
            fat_sectors = (total_sectors - 36 - 65525 * 8) / 2 + 1;
            data_start = 36 + 2 * fat_sectors;
            clusters = (total_sectors - data_start) / 8;
        }
    }
    if (!is_fat32 && (clusters < 4085 || fat_sectors > UINT16_MAX)) {
        printf("lformat.fat: device has no valid FAT layout\n");
        return 1;
    }

    fd = vfs_open(devpath, 2);
    if (fd < 0) {
        printf("lformat.fat: cannot open %s for writing\n", devpath);
        return 1;
    }

    memset(boot, 0, sizeof(boot));
    boot[0] = 0xEB;
    boot[1] = 0x58;
    boot[2] = 0x90;
    memcpy(boot + 3, "LEBIRUN ", 8);
    put16(boot + 11, SECT_SIZE);
    boot[13] = (uint8_t)cluster_sectors;
    if (is_fat32) {
        put16(boot + 14, 32);
        boot[16] = 2;
        put16(boot + 17, 0);
        put16(boot + 19, 0);
        boot[21] = 0xF8;
        put16(boot + 22, 0);
        put32(boot + 32, (uint32_t)total_sectors);
        put32(boot + 36, (uint32_t)fat_sectors);
        put16(boot + 40, 0);
        put16(boot + 42, 0);
        put32(boot + 44, 2);
        put16(boot + 48, 1);
        put16(boot + 50, 6);
        boot[64] = 0x80;
        boot[66] = 0x29;
        put32(boot + 67, 0x12345678);
        sfn_label(label, sfn);
        memcpy(boot + 71, sfn, 11);
        memcpy(boot + 82, "FAT32   ", 8);
    } else {
        put16(boot + 14, 4);
        boot[16] = 2;
        put16(boot + 17, 512);
        if (total_sectors < 65536)
            put16(boot + 19, (uint16_t)total_sectors);
        else
            put32(boot + 32, (uint32_t)total_sectors);
        boot[21] = 0xF8;
        put16(boot + 22, (uint16_t)fat_sectors);
        boot[36] = 0x80;
        boot[38] = 0x29;
        put32(boot + 39, 0x12345678);
        sfn_label(label, sfn);
        memcpy(boot + 43, sfn, 11);
        memcpy(boot + 54, "FAT16   ", 8);
    }
    boot[510] = 0x55;
    boot[511] = 0xAA;
    if (write_zero(fd, reserved * SECT_SIZE,
                   2 * fat_sectors * SECT_SIZE) != 0) goto out;
    if (write_at(fd, 0, boot, sizeof(boot)) != 0) goto out;

    if (is_fat32) {
        if (write_at(fd, 6 * SECT_SIZE, boot, sizeof(boot)) != 0) goto out;
        memset(boot, 0, sizeof(boot));
        put32(boot, 0x41615252);
        put32(boot + 484, 0x61417272);
        free_clusters = clusters - 1;
        put32(boot + 488, (uint32_t)free_clusters);
        put32(boot + 492, 3);
        boot[510] = 0x55;
        boot[511] = 0xAA;
        if (write_at(fd, 1 * SECT_SIZE, boot, sizeof(boot)) != 0) goto out;
        if (write_at(fd, 7 * SECT_SIZE, boot, sizeof(boot)) != 0) goto out;
        memset(boot, 0, sizeof(boot));
        put32(boot, 0x0FFFFFF8);
        put32(boot + 4, 0x0FFFFFFF);
        put32(boot + 8, 0x0FFFFFFF);
        if (write_at(fd, 32 * SECT_SIZE, boot, 12) != 0) goto out;
        if (write_at(fd, (32 + fat_sectors) * SECT_SIZE, boot, 12) != 0)
            goto out;
        if (write_zero(fd, (uint64_t)data_start * SECT_SIZE,
                       cluster_sectors * SECT_SIZE) != 0) goto out;
        if (sfn[0] != ' ') {
            memset(entry, 0, sizeof(entry));
            memcpy(entry, sfn, 11);
            entry[11] = 0x08;
            if (write_at(fd, (uint64_t)data_start * SECT_SIZE, entry,
                         sizeof(entry)) != 0) goto out;
        }
    } else {
        memset(boot, 0, sizeof(boot));
        boot[0] = 0xF8;
        boot[1] = 0xFF;
        boot[2] = 0xFF;
        boot[3] = 0xFF;
        if (write_at(fd, 4 * SECT_SIZE, boot, 4) != 0) goto out;
        if (write_at(fd, (4 + fat_sectors) * SECT_SIZE, boot, 4) != 0)
            goto out;
        root_lba = 4 + 2 * fat_sectors;
        if (write_zero(fd, root_lba * SECT_SIZE,
                       32 * SECT_SIZE) != 0) goto out;
        if (sfn[0] != ' ') {
            memset(entry, 0, sizeof(entry));
            memcpy(entry, sfn, 11);
            entry[11] = 0x08;
            if (write_at(fd, root_lba * SECT_SIZE, entry,
                         sizeof(entry)) != 0) goto out;
        }
    }

    if (vfs_close_fd(fd) != 0) {
        fprintf(stderr, "lformat.fat: close failed\n");
        return 1;
    }
    printf("lformat.fat: formatted %s as %s (%llu sectors)\n", devpath,
           is_fat32 ? "FAT32" : "FAT16",
           (unsigned long long)total_sectors);
    return 0;

out:
    vfs_close_fd(fd);
    return 1;
}
