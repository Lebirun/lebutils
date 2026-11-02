#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <lebirun.h>

#define SECT_SIZE 512
#define FAT_OFF 128

static void put(uint8_t *p, uint64_t v, int n) {
    while (n--) {
        *p++ = (uint8_t)v;
        v >>= 8;
    }
}

static int write_at(int fd, uint64_t offset, const void *data, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t done = 0;
    int ret;

    if (lseek(fd, (off_t)offset, SEEK_SET) != (off_t)offset) {
        fprintf(stderr, "lformat.exfat: seek failed\n");
        return -1;
    }
    while (done < size) {
        ret = vfs_write_fd(fd, bytes + done, size - done);
        if (ret <= 0 || (uint32_t)ret > size - done) {
            fprintf(stderr, "lformat.exfat: write failed\n");
            return -1;
        }
        done += (uint32_t)ret;
    }
    return 0;
}

static int write_zero(int fd, uint64_t offset, uint64_t size) {
    uint8_t zero[SECT_SIZE];
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

static uint32_t sum32(uint32_t s, const uint8_t *p, uint32_t n, int boot) {
    uint32_t i;

    for (i = 0; i < n; i++)
        if (!boot || (i != 106 && i != 107 && i != 112))
            s = (s >> 1 | s << 31) + p[i];
    return s;
}

int cmd_lformat_exfat(int argc, char **argv) {
    const char *devpath;
    const char *label = "";
    uint64_t dev_size;
    uint64_t dev_type;
    uint64_t total;
    uint64_t count;
    uint64_t heap;
    uint64_t bpc;
    uint64_t bm_bytes;
    uint64_t bm_cl;
    uint64_t root_off;
    uint32_t fat_len;
    uint32_t chk = 0;
    uint32_t c;
    uint16_t up[30];
    uint8_t sec[SECT_SIZE];
    uint8_t shift;
    int fd;
    int i;

    if (argc < 2) {
        printf("Usage: lformat.exfat <device> [-L label]\n");
        return 1;
    }
    devpath = argv[1];
    for (i = 2; i < argc; i++)
        if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) label = argv[++i];

    fd = vfs_open(devpath, 0);
    if (fd < 0) {
        printf("lformat.exfat: cannot open %s\n", devpath);
        return 1;
    }
    if (vfs_stat(fd, &dev_size, &dev_type) != 0 || dev_size == 0) {
        vfs_close_fd(fd);
        printf("lformat.exfat: cannot determine size of %s\n", devpath);
        return 1;
    }
    vfs_close_fd(fd);

    total = dev_size / SECT_SIZE;
    shift = total <= (256ULL << 11) ? 3 : total <= (32ULL << 21) ? 6 : 8;
    bpc = (uint64_t)SECT_SIZE << shift;
    count = (total - FAT_OFF) / (1u << shift);
    fat_len = (uint32_t)(((count + 2) * 4 + SECT_SIZE - 1) / SECT_SIZE);
    heap = (FAT_OFF + fat_len + (1u << shift) - 1) & ~(uint64_t)((1u << shift) - 1);
    count = total > heap ? (total - heap) >> shift : 0;
    bm_bytes = (count + 7) / 8;
    bm_cl = (bm_bytes + bpc - 1) / bpc;
    if (total < 2048 || count < bm_cl + 2 || count > 0xFFFFFFF5) {
        printf("lformat.exfat: device has no valid exFAT layout\n");
        return 1;
    }
    root_off = (heap + ((bm_cl + 1) << shift)) * SECT_SIZE;

    fd = vfs_open(devpath, 2);
    if (fd < 0) {
        printf("lformat.exfat: cannot open %s for writing\n", devpath);
        return 1;
    }

    up[0] = 0xFFFF;
    up[1] = 'a';
    for (i = 0; i < 26; i++) up[2 + i] = (uint16_t)('A' + i);
    up[28] = 0xFFFF;
    up[29] = (uint16_t)(0x10000 - 'z' - 1);

    for (i = 0; i < 12; i++) {
        memset(sec, 0, sizeof(sec));
        if (i == 0) {
            sec[0] = 0xEB;
            sec[1] = 0x76;
            sec[2] = 0x90;
            memcpy(sec + 3, "EXFAT   ", 8);
            put(sec + 72, total, 8);
            put(sec + 80, FAT_OFF, 4);
            put(sec + 84, fat_len, 4);
            put(sec + 88, heap, 4);
            put(sec + 92, count, 4);
            put(sec + 96, bm_cl + 3, 4);
            put(sec + 100, getticks() * 2654435761u, 4);
            put(sec + 104, 0x0100, 2);
            sec[108] = 9;
            sec[109] = shift;
            sec[110] = 1;
            sec[111] = 0x80;
            sec[112] = (uint8_t)((bm_cl + 2) * 100 / count);
            memset(sec + 120, 0xF4, 390);
        }
        if (i < 9) {
            sec[510] = 0x55;
            sec[511] = 0xAA;
        }
        if (i == 11)
            for (c = 0; c < SECT_SIZE; c += 4) put(sec + c, chk, 4);
        else
            chk = sum32(chk, sec, SECT_SIZE, i == 0);
        if (write_at(fd, (uint64_t)i * SECT_SIZE, sec, SECT_SIZE) != 0 ||
            write_at(fd, (uint64_t)(i + 12) * SECT_SIZE, sec, SECT_SIZE) != 0)
            goto out;
    }

    memset(sec, 0, sizeof(sec));
    for (c = 0; c < bm_cl + 4; c++) {
        put(sec + c * 4 % SECT_SIZE, !c ? 0xFFFFFFF8 :
            c >= 2 && c <= bm_cl ? c + 1 : 0xFFFFFFFF, 4);
        if ((c + 1) % (SECT_SIZE / 4) == 0 || c + 1 == bm_cl + 4) {
            if (write_at(fd, (FAT_OFF + c / (SECT_SIZE / 4)) * SECT_SIZE, sec,
                         SECT_SIZE) != 0)
                goto out;
            memset(sec, 0, sizeof(sec));
        }
    }

    for (c = 0; c < bm_cl + 2; c++) sec[c / 8] |= (uint8_t)(1 << (c % 8));
    if (write_at(fd, heap * SECT_SIZE, sec, SECT_SIZE) != 0 ||
        write_zero(fd, (heap + 1) * SECT_SIZE,
                   bm_bytes > SECT_SIZE ? bm_bytes - SECT_SIZE : 0) != 0)
        goto out;

    memset(sec, 0, sizeof(sec));
    for (i = 0; i < 30; i++) put(sec + i * 2, up[i], 2);
    if (write_at(fd, root_off - bpc, sec, sizeof(up)) != 0) goto out;
    chk = sum32(0, sec, sizeof(up), 0);

    memset(sec, 0, sizeof(sec));
    sec[0] = 0x83;
    for (i = 0; i < 11 && label[i]; i++) put(sec + 2 + i * 2, (uint8_t)label[i], 2);
    sec[1] = (uint8_t)i;
    sec[32] = 0x81;
    put(sec + 52, 2, 4);
    put(sec + 56, bm_bytes, 8);
    sec[64] = 0x82;
    put(sec + 68, chk, 4);
    put(sec + 84, bm_cl + 2, 4);
    put(sec + 88, sizeof(up), 8);
    if (write_zero(fd, root_off, bpc) != 0 ||
        write_at(fd, root_off, sec, 96) != 0)
        goto out;

    if (vfs_close_fd(fd) != 0) {
        fprintf(stderr, "lformat.exfat: close failed\n");
        return 1;
    }
    printf("lformat.exfat: formatted %s as exFAT (%llu clusters of %llu KB)\n",
           devpath, (unsigned long long)count, (unsigned long long)(bpc / 1024));
    return 0;

out:
    vfs_close_fd(fd);
    return 1;
}
