#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <lebirun.h>
#include "cu.h"

#define SECTOR_SIZE 512
#define MAX_PARTS 16
#define MBR_SIG 0xAA55
#define GPT_SIG_VAL 0x5452415020494645ULL

#define LD_KEY_UP    1000
#define LD_KEY_DOWN  1001
#define LD_KEY_LEFT  1002
#define LD_KEY_RIGHT 1003
#define LD_KEY_HOME  1004
#define LD_KEY_END   1005
#define LD_KEY_ENTER 1010

#define LD_ACT_NEW     0
#define LD_ACT_DELETE  1
#define LD_ACT_TYPE    2
#define LD_ACT_RESIZE  3
#define LD_ACT_WRITE   4
#define LD_ACT_WIPE    5
#define LD_ACT_DUMBBR  6
#define LD_ACT_DGPT    7
#define LD_ACT_HELP    8
#define LD_ACT_QUIT    9
#define LD_ACT_COUNT   10

typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) ld_mbr_entry_t;

typedef struct {
    uint8_t  bootstrap[446];
    ld_mbr_entry_t parts[4];
    uint16_t signature;
} __attribute__((packed)) ld_mbr_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
} __attribute__((packed)) ld_gpt_header_t;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint16_t name[36];
} __attribute__((packed)) ld_gpt_entry_t;

typedef struct {
    int      valid;
    int      number;
    uint64_t start_lba;
    uint64_t sector_count;
    uint8_t  mbr_type;
    uint8_t  gpt_type_guid[16];
    char     gpt_name[72];
} ld_part_info_t;

typedef struct {
    char     devname[16];
    char     devpath[32];
    int      is_gpt;
    int      part_count;
    uint64_t disk_sectors;
    ld_part_info_t parts[MAX_PARTS];
    int      modified;
    uint8_t  mbr_bootstrap[446];
    uint8_t  gpt_disk_guid[16];
} ld_disk_t;

static struct {
    ld_disk_t disk;
    int       screen_rows;
    int       screen_cols;
    int       sel_part;
    int       sel_action;
    int       running;
    char      msg[128];
    int       prompt_active;
    char      prompt_title[64];
    char      prompt_buf[64];
    int       prompt_len;
    int       prompt_result;
} S;

static struct termios ld_orig_termios;
static int ld_raw_on;

static const uint8_t LD_GPT_LINUX_FS[16] = {
    0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47,
    0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4
};

static const uint8_t LD_GPT_LINUX_SWAP[16] = {
    0x6D, 0xFD, 0x57, 0x06, 0xAB, 0xA4, 0xC4, 0x43,
    0x84, 0xE5, 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F
};

static const uint8_t LD_GPT_EFI_SYSTEM[16] = {
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

static const char *ld_action_labels[LD_ACT_COUNT] = {
    " New ", " Delete ", " Type ", " Resize ", " Write ", " Wipe ", " DOS ", " GPT ", " Help ", " Quit "
};

static void ld_write_str(const char *s, int len) {
    write(STDOUT_FILENO, s, len);
}

static void ld_puts(const char *s) {
    ld_write_str(s, strlen(s));
}

static void ld_enable_raw(void) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &ld_orig_termios);
    raw = ld_orig_termios;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, 0, &raw);
    ld_raw_on = 1;
}

static void ld_disable_raw(void) {
    if (ld_raw_on) {
        tcsetattr(STDIN_FILENO, 0, &ld_orig_termios);
        ld_raw_on = 0;
    }
}

static int ld_read_key(void) {
    char c;
    int n;

    n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return -1;

    if (c == '\x1b') {
        char seq[4];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] == 'A') return LD_KEY_UP;
            if (seq[1] == 'B') return LD_KEY_DOWN;
            if (seq[1] == 'C') return LD_KEY_RIGHT;
            if (seq[1] == 'D') return LD_KEY_LEFT;
            if (seq[1] == 'H') return LD_KEY_HOME;
            if (seq[1] == 'F') return LD_KEY_END;
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    if (seq[1] == '1') return LD_KEY_HOME;
                    if (seq[1] == '4') return LD_KEY_END;
                }
            }
        }
        return '\x1b';
    }

    if (c == '\r' || c == '\n') return LD_KEY_ENTER;
    return (int)(unsigned char)c;
}

static int ld_guid_is_zero(const uint8_t *g) {
    int i;
    for (i = 0; i < 16; i++) {
        if (g[i] != 0) return 0;
    }
    return 1;
}

static int ld_guid_equal(const uint8_t *a, const uint8_t *b) {
    int i;
    for (i = 0; i < 16; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static const char *ld_mbr_type_name(uint8_t t) {
    switch (t) {
    case 0x00: return "Free Space";
    case 0x01: return "FAT12";
    case 0x04: return "FAT16 <32M";
    case 0x05: return "Extended";
    case 0x06: return "FAT16";
    case 0x07: return "NTFS/HPFS";
    case 0x0B: return "FAT32";
    case 0x0C: return "FAT32 LBA";
    case 0x0E: return "FAT16 LBA";
    case 0x0F: return "Extended LBA";
    case 0x82: return "Linux Swap";
    case 0x83: return "Linux";
    case 0x8E: return "Linux LVM";
    case 0xEE: return "GPT Protective";
    default:   return "Unknown";
    }
}

static const char *ld_gpt_type_name(const uint8_t *guid) {
    if (ld_guid_is_zero(guid)) return "Free Space";
    if (ld_guid_equal(guid, LD_GPT_LINUX_FS)) return "Linux filesystem";
    if (ld_guid_equal(guid, LD_GPT_LINUX_SWAP)) return "Linux swap";
    if (ld_guid_equal(guid, LD_GPT_EFI_SYSTEM)) return "EFI System";
    return "Unknown";
}

typedef struct {
    uint8_t     code;
    const char *name;
} ld_mbr_type_entry_t;

static const ld_mbr_type_entry_t ld_mbr_types[] = {
    { 0x83, "Linux" },
    { 0x82, "Linux Swap" },
    { 0x8E, "Linux LVM" },
    { 0x0B, "FAT32" },
    { 0x0C, "FAT32 LBA" },
    { 0x07, "NTFS/HPFS" },
    { 0x01, "FAT12" },
    { 0x04, "FAT16 <32M" },
    { 0x06, "FAT16" },
    { 0x0E, "FAT16 LBA" },
    { 0x05, "Extended" },
    { 0x0F, "Extended LBA" },
    { 0xEE, "GPT Protective" },
    { 0xEF, "EFI System" },
};

#define LD_MBR_TYPE_COUNT (int)(sizeof(ld_mbr_types) / sizeof(ld_mbr_types[0]))

typedef struct {
    const char    *name;
    const uint8_t *guid;
} ld_gpt_type_entry_t;

static const ld_gpt_type_entry_t ld_gpt_types[] = {
    { "Linux filesystem", LD_GPT_LINUX_FS },
    { "Linux swap",       LD_GPT_LINUX_SWAP },
    { "EFI System",       LD_GPT_EFI_SYSTEM },
};

#define LD_GPT_TYPE_COUNT (int)(sizeof(ld_gpt_types) / sizeof(ld_gpt_types[0]))

static void ld_format_size(uint64_t sectors, char *buf, int bufsz) {
    uint64_t bytes;
    uint64_t mb;
    uint64_t gb;
    uint64_t tb;

    bytes = sectors * SECTOR_SIZE;
    tb = bytes / (1024ULL * 1024 * 1024 * 1024);
    gb = bytes / (1024ULL * 1024 * 1024);
    mb = bytes / (1024ULL * 1024);

    if (tb >= 1) {
        snprintf(buf, bufsz, "%llu.%llu TiB",
                 (unsigned long long)tb,
                 (unsigned long long)((gb % 1024) * 10 / 1024));
    } else if (gb >= 1) {
        snprintf(buf, bufsz, "%llu.%llu GiB",
                 (unsigned long long)gb,
                 (unsigned long long)((mb % 1024) * 10 / 1024));
    } else {
        snprintf(buf, bufsz, "%llu MiB", (unsigned long long)mb);
    }
}

static int ld_disk_read(const char *devpath, uint32_t lba, uint32_t count, void *buf) {
    int fd;
    uint32_t total;
    int ret;
    off_t offset;

    fd = vfs_open(devpath, 0);
    if (fd < 0) return -1;

    total = count * SECTOR_SIZE;
    offset = (off_t)lba * SECTOR_SIZE;

    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) < 0) {
            vfs_close_fd(fd);
            return -1;
        }
    }

    ret = vfs_read_fd(fd, buf, total);
    vfs_close_fd(fd);
    if (ret < 0) return -1;
    return ret;
}

static int ld_disk_write(const char *devpath, uint32_t lba, uint32_t count, const void *buf) {
    int fd;
    uint32_t total;
    int ret;
    off_t offset;

    fd = vfs_open(devpath, 2);
    if (fd < 0) return -1;

    total = count * SECTOR_SIZE;
    offset = (off_t)lba * SECTOR_SIZE;

    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) < 0) {
            vfs_close_fd(fd);
            return -1;
        }
    }

    ret = vfs_write_fd(fd, buf, total);
    vfs_close_fd(fd);
    if (ret < 0) return -1;
    return ret;
}

static int ld_scan_disk(ld_disk_t *disk) {
    static uint8_t sector0[SECTOR_SIZE];
    static uint8_t sector1[SECTOR_SIZE];
    static uint8_t entries_buf[SECTOR_SIZE * 32];
    ld_mbr_t *mbr;
    ld_gpt_header_t *gpt;
    int ret;
    int i;
    int stat_fd;
    uint64_t dev_size;
    uint64_t dev_type;

    memset(disk->parts, 0, sizeof(disk->parts));
    disk->part_count = 0;
    disk->is_gpt = 0;
    disk->disk_sectors = 0;
    disk->modified = 0;

    stat_fd = vfs_open(disk->devpath, 0);
    if (stat_fd >= 0) {
        dev_size = 0;
        dev_type = 0;
        if (vfs_stat(stat_fd, &dev_size, &dev_type) == 0 && dev_size > 0) {
            disk->disk_sectors = (uint64_t)dev_size / SECTOR_SIZE;
        }
        vfs_close_fd(stat_fd);
    }

    ret = ld_disk_read(disk->devpath, 0, 1, sector0);
    if (ret < SECTOR_SIZE) return -1;

    mbr = (ld_mbr_t *)sector0;
    memcpy(disk->mbr_bootstrap, mbr->bootstrap, 446);

    if (mbr->signature != MBR_SIG) return 0;

    ret = ld_disk_read(disk->devpath, 1, 1, sector1);
    if (ret >= SECTOR_SIZE) {
        gpt = (ld_gpt_header_t *)sector1;
        if (gpt->signature == GPT_SIG_VAL) {
            ld_gpt_entry_t *entry;
            uint32_t entry_lba;
            uint32_t entry_sectors;
            int nr;
            int j;

            disk->is_gpt = 1;
            disk->disk_sectors = gpt->alternate_lba + 1;
            memcpy(disk->gpt_disk_guid, gpt->disk_guid, 16);

            entry_lba = (uint32_t)gpt->partition_entry_lba;
            nr = (int)gpt->num_partition_entries;
            if (nr > 128) nr = 128;

            entry_sectors = (uint32_t)((nr * gpt->partition_entry_size + 511) / 512);
            if (entry_sectors > 32) entry_sectors = 32;

            ret = ld_disk_read(disk->devpath, entry_lba, entry_sectors, entries_buf);
            if (ret > 0) {
                j = 0;
                for (i = 0; i < nr && j < MAX_PARTS; i++) {
                    entry = (ld_gpt_entry_t *)(entries_buf + i * gpt->partition_entry_size);
                    if (ld_guid_is_zero(entry->type_guid)) continue;
                    disk->parts[j].valid = 1;
                    disk->parts[j].number = i + 1;
                    disk->parts[j].start_lba = entry->starting_lba;
                    disk->parts[j].sector_count = entry->ending_lba - entry->starting_lba + 1;
                    memcpy(disk->parts[j].gpt_type_guid, entry->type_guid, 16);
                    disk->parts[j].mbr_type = 0;
                    {
                        int k;
                        for (k = 0; k < 36 && entry->name[k]; k++) {
                            disk->parts[j].gpt_name[k] = (char)(entry->name[k] & 0x7F);
                        }
                        disk->parts[j].gpt_name[k] = '\0';
                    }
                    j++;
                }
                disk->part_count = j;
            }
            return 0;
        }
    }

    {
        uint64_t max_end;
        max_end = 0;
        for (i = 0; i < 4; i++) {
            if (mbr->parts[i].type == 0) continue;
            if (disk->part_count >= MAX_PARTS) break;
            disk->parts[disk->part_count].valid = 1;
            disk->parts[disk->part_count].number = i + 1;
            disk->parts[disk->part_count].start_lba = mbr->parts[i].lba_start;
            disk->parts[disk->part_count].sector_count = mbr->parts[i].sector_count;
            disk->parts[disk->part_count].mbr_type = mbr->parts[i].type;
            memset(disk->parts[disk->part_count].gpt_type_guid, 0, 16);
            disk->parts[disk->part_count].gpt_name[0] = '\0';
            {
                uint64_t end;
                end = (uint64_t)mbr->parts[i].lba_start + mbr->parts[i].sector_count;
                if (end > max_end) max_end = end;
            }
            disk->part_count++;
        }
        if (max_end > 0) disk->disk_sectors = max_end;
    }

    return 0;
}

static uint32_t ld_crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc;
    uint32_t i;
    int j;

    crc = 0xFFFFFFFF;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    return ~crc;
}

static void ld_generate_guid(uint8_t *guid) {
    int fd;
    int ret;
    int i;

    fd = vfs_open("/dev/urandom", 0);
    if (fd >= 0) {
        ret = vfs_read_fd(fd, guid, 16);
        vfs_close_fd(fd);
        if (ret == 16) {
            guid[6] = (guid[6] & 0x0F) | 0x40;
            guid[8] = (guid[8] & 0x3F) | 0x80;
            return;
        }
    }
    for (i = 0; i < 16; i++)
        guid[i] = (uint8_t)(getticks() ^ (i * 37));
    guid[6] = (guid[6] & 0x0F) | 0x40;
    guid[8] = (guid[8] & 0x3F) | 0x80;
}

static uint64_t ld_find_free_start(ld_disk_t *disk) {
    uint64_t start;
    int i;
    int overlap;

    start = 2048;
    for (;;) {
        overlap = 0;
        for (i = 0; i < disk->part_count; i++) {
            if (!disk->parts[i].valid) continue;
            if (start >= disk->parts[i].start_lba &&
                start < disk->parts[i].start_lba + disk->parts[i].sector_count) {
                start = disk->parts[i].start_lba + disk->parts[i].sector_count;
                if (start % 2048 != 0) start = ((start / 2048) + 1) * 2048;
                overlap = 1;
                break;
            }
        }
        if (!overlap) break;
    }
    return start;
}

static uint64_t ld_find_max_sectors(ld_disk_t *disk, uint64_t start) {
    uint64_t max_end;
    uint64_t nearest;
    int i;

    if (disk->is_gpt)
        max_end = disk->disk_sectors > 34 ? disk->disk_sectors - 34 : disk->disk_sectors;
    else
        max_end = disk->disk_sectors;

    nearest = max_end;
    for (i = 0; i < disk->part_count; i++) {
        if (!disk->parts[i].valid) continue;
        if (disk->parts[i].start_lba > start && disk->parts[i].start_lba < nearest)
            nearest = disk->parts[i].start_lba;
    }
    if (nearest <= start) return 0;
    return nearest - start;
}

static uint64_t ld_parse_size_input(const char *s, uint64_t max_sectors) {
    uint64_t val;
    int len;
    char suffix;

    len = (int)strlen(s);
    if (len == 0) return max_sectors;

    suffix = s[len - 1];
    val = strtoull(s, NULL, 10);

    if (suffix == 'M' || suffix == 'm')
        val = val * 1024 * 1024 / SECTOR_SIZE;
    else if (suffix == 'G' || suffix == 'g')
        val = val * 1024 * 1024 * 1024 / SECTOR_SIZE;
    else if (suffix == 'K' || suffix == 'k')
        val = val * 1024 / SECTOR_SIZE;

    if (val > max_sectors) val = max_sectors;
    return val;
}

static void ld_move_cursor(int row, int col) {
    char buf[32];
    int len;
    len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    ld_write_str(buf, len);
}

static void ld_clear_screen(void) {
    ld_puts("\x1b[2J");
    ld_move_cursor(1, 1);
}

static void ld_set_color(int fg, int bg) {
    char buf[32];
    int len;
    len = snprintf(buf, sizeof(buf), "\x1b[%d;%dm", fg, bg);
    ld_write_str(buf, len);
}

static void ld_reset_color(void) {
    ld_puts("\x1b[0m");
}

static void ld_hide_cursor(void) {
    ld_puts("\x1b[?25l");
}

static void ld_show_cursor(void) {
    ld_puts("\x1b[?25h");
}

static void ld_clear_line(void) {
    ld_puts("\x1b[2K");
}

static void ld_draw_padded(const char *text, int width) {
    int len;
    int i;

    len = (int)strlen(text);
    if (len > width) len = width;
    ld_write_str(text, len);
    for (i = len; i < width; i++)
        ld_write_str(" ", 1);
}

static void ld_draw_header(void) {
    char line[256];
    char sizebuf[32];
    int i;

    ld_move_cursor(1, 1);
    ld_set_color(37, 44);

    snprintf(line, sizeof(line), " ldiskutil - Lebirun Disk Utility");
    ld_draw_padded(line, S.screen_cols - 1);
    ld_reset_color();

    ld_move_cursor(3, 1);
    if (S.disk.disk_sectors > 0) {
        ld_format_size(S.disk.disk_sectors, sizebuf, sizeof(sizebuf));
        snprintf(line, sizeof(line), "  Disk: /dev/%s  Size: %s (%llu sectors)  Label: %s",
                 S.disk.devname, sizebuf,
                 (unsigned long long)S.disk.disk_sectors,
                 S.disk.is_gpt ? "GPT" : "DOS");
    } else {
        snprintf(line, sizeof(line), "  Disk: /dev/%s  Size: unknown  Label: %s",
                 S.disk.devname,
                 S.disk.is_gpt ? "GPT" : (S.disk.part_count > 0 ? "DOS" : "none"));
    }
    ld_clear_line();
    ld_puts(line);

    if (S.disk.modified) {
        ld_puts("  [modified]");
    }

    ld_move_cursor(5, 1);
    ld_set_color(1, 0);
    if (S.disk.is_gpt) {
        snprintf(line, sizeof(line), "  %-4s  %12s  %12s  %12s  %10s  %-20s",
                 "#", "Start", "End", "Sectors", "Size", "Type");
    } else {
        snprintf(line, sizeof(line), "  %-4s  %12s  %12s  %12s  %10s  %-4s  %-18s",
                 "#", "Start", "End", "Sectors", "Size", "Id", "Type");
    }
    ld_draw_padded(line, S.screen_cols - 1);
    ld_reset_color();

    ld_move_cursor(6, 1);
    for (i = 0; i < S.screen_cols - 1; i++)
        ld_write_str("-", 1);
}

static void ld_draw_partition_list(void) {
    int start_row;
    int max_visible;
    int i;
    char line[256];
    char sizebuf[32];
    const char *type_name;

    start_row = 7;
    max_visible = S.screen_rows - 10;
    if (max_visible < 1) max_visible = 1;

    for (i = 0; i < max_visible; i++) {
        ld_move_cursor(start_row + i, 1);
        ld_clear_line();

        if (i < S.disk.part_count && S.disk.parts[i].valid) {
            ld_format_size(S.disk.parts[i].sector_count, sizebuf, sizeof(sizebuf));

            if (S.disk.is_gpt) {
                type_name = ld_gpt_type_name(S.disk.parts[i].gpt_type_guid);
                snprintf(line, sizeof(line), "  %-4d  %12llu  %12llu  %12llu  %10s  %-20s",
                         S.disk.parts[i].number,
                         (unsigned long long)S.disk.parts[i].start_lba,
                         (unsigned long long)(S.disk.parts[i].start_lba + S.disk.parts[i].sector_count - 1),
                         (unsigned long long)S.disk.parts[i].sector_count,
                         sizebuf,
                         type_name);
            } else {
                type_name = ld_mbr_type_name(S.disk.parts[i].mbr_type);
                snprintf(line, sizeof(line), "  %-4d  %12llu  %12llu  %12llu  %10s  %02x    %-18s",
                         S.disk.parts[i].number,
                         (unsigned long long)S.disk.parts[i].start_lba,
                         (unsigned long long)(S.disk.parts[i].start_lba + S.disk.parts[i].sector_count - 1),
                         (unsigned long long)S.disk.parts[i].sector_count,
                         sizebuf,
                         S.disk.parts[i].mbr_type,
                         type_name);
            }

            if (i == S.sel_part) {
                ld_set_color(30, 47);
                ld_draw_padded(line, S.screen_cols - 1);
                ld_reset_color();
            } else {
                ld_puts(line);
            }
        } else if (i == 0 && S.disk.part_count == 0) {
            if (i == S.sel_part) {
                ld_set_color(30, 47);
                ld_draw_padded("  (no partitions - use New to create or DOS/GPT to create table)", S.screen_cols - 1);
                ld_reset_color();
            } else {
                ld_puts("  (no partitions)");
            }
        }
    }
}

static void ld_draw_actions(void) {
    int row;
    int i;
    int col;

    row = S.screen_rows - 3;
    ld_move_cursor(row, 1);
    for (i = 0; i < S.screen_cols - 1; i++)
        ld_write_str("-", 1);

    ld_move_cursor(row + 1, 1);
    ld_clear_line();

    col = 2;
    for (i = 0; i < LD_ACT_COUNT; i++) {
        ld_move_cursor(row + 1, col);
        if (i == S.sel_action) {
            ld_set_color(30, 47);
            ld_puts(ld_action_labels[i]);
            ld_reset_color();
        } else {
            ld_set_color(37, 40);
            ld_puts(ld_action_labels[i]);
            ld_reset_color();
        }
        col += (int)strlen(ld_action_labels[i]) + 1;
    }
}

static void ld_draw_status(void) {
    int row;

    row = S.screen_rows - 1;
    ld_move_cursor(row, 1);
    ld_set_color(37, 44);

    if (S.msg[0]) {
        ld_draw_padded(S.msg, S.screen_cols - 1);
    } else {
        ld_draw_padded(" Arrow keys to navigate, Enter to select action, q to quit", S.screen_cols - 1);
    }
    ld_reset_color();
}

static void ld_draw_all(void) {
    ld_move_cursor(1, 1);
    ld_hide_cursor();
    ld_draw_header();
    ld_draw_partition_list();
    ld_draw_actions();
    ld_draw_status();
}

static int ld_prompt_input(const char *title, char *buf, int bufsz) {
    int row;
    int key;
    int len;

    row = S.screen_rows - 1;
    len = 0;
    buf[0] = '\0';

    ld_move_cursor(row, 1);
    ld_set_color(37, 44);
    {
        char prompt[128];
        snprintf(prompt, sizeof(prompt), " %s: ", title);
        ld_draw_padded(prompt, S.screen_cols - 1);
    }
    ld_reset_color();
    ld_show_cursor();
    ld_move_cursor(row, (int)strlen(title) + 4);

    for (;;) {
        key = ld_read_key();
        if (key == LD_KEY_ENTER) {
            buf[len] = '\0';
            ld_hide_cursor();
            return 0;
        }
        if (key == '\x1b') {
            buf[0] = '\0';
            ld_hide_cursor();
            return -1;
        }
        if (key == 127 || key == '\b') {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                ld_move_cursor(row, 1);
                ld_set_color(37, 44);
                {
                    char prompt[128];
                    snprintf(prompt, sizeof(prompt), " %s: %s", title, buf);
                    ld_draw_padded(prompt, S.screen_cols - 1);
                }
                ld_reset_color();
                ld_move_cursor(row, (int)strlen(title) + 4 + len);
            }
            continue;
        }
        if (key >= 32 && key < 127 && len < bufsz - 1) {
            buf[len++] = (char)key;
            buf[len] = '\0';
            ld_move_cursor(row, 1);
            ld_set_color(37, 44);
            {
                char prompt[128];
                snprintf(prompt, sizeof(prompt), " %s: %s", title, buf);
                ld_draw_padded(prompt, S.screen_cols - 1);
            }
            ld_reset_color();
            ld_move_cursor(row, (int)strlen(title) + 4 + len);
        }
    }
}

static int ld_confirm(const char *msg) {
    int row;
    int key;

    row = S.screen_rows - 1;
    ld_move_cursor(row, 1);
    ld_set_color(30, 43);
    {
        char prompt[128];
        snprintf(prompt, sizeof(prompt), " %s (y/N) ", msg);
        ld_draw_padded(prompt, S.screen_cols - 1);
    }
    ld_reset_color();

    key = ld_read_key();
    return (key == 'y' || key == 'Y') ? 1 : 0;
}

static void ld_action_new(void) {
    uint64_t start;
    uint64_t max_sectors;
    uint64_t sectors;
    char buf[64];
    char sizebuf[32];
    int slot;
    int next_num;
    int i;
    int used[4];
    int k;

    if (!S.disk.is_gpt && S.disk.part_count >= 4) {
        strcpy(S.msg, " Max 4 primary partitions for MBR.");
        return;
    }
    if (S.disk.part_count >= MAX_PARTS) {
        strcpy(S.msg, " Maximum partitions reached.");
        return;
    }
    if (S.disk.disk_sectors == 0) {
        if (ld_prompt_input("Total disk sectors", buf, sizeof(buf)) < 0) return;
        S.disk.disk_sectors = strtoull(buf, NULL, 10);
        if (S.disk.disk_sectors == 0) {
            strcpy(S.msg, " Invalid disk size.");
            return;
        }
    }

    start = ld_find_free_start(&S.disk);
    max_sectors = ld_find_max_sectors(&S.disk, start);

    if (max_sectors == 0) {
        strcpy(S.msg, " No free space available.");
        return;
    }

    {
        char prompt[96];
        ld_format_size(max_sectors, sizebuf, sizeof(sizebuf));
        snprintf(prompt, sizeof(prompt), "First sector (default %llu)", (unsigned long long)start);
        if (ld_prompt_input(prompt, buf, sizeof(buf)) < 0) return;
        if (buf[0] != '\0') {
            start = strtoull(buf, NULL, 10);
            max_sectors = ld_find_max_sectors(&S.disk, start);
        }
    }

    {
        char prompt[96];
        ld_format_size(max_sectors, sizebuf, sizeof(sizebuf));
        snprintf(prompt, sizeof(prompt), "Size (+100M, +1G, or sectors, default %s)", sizebuf);
        if (ld_prompt_input(prompt, buf, sizeof(buf)) < 0) return;
        if (buf[0] == '+')
            sectors = ld_parse_size_input(buf + 1, max_sectors);
        else if (buf[0] == '\0')
            sectors = max_sectors;
        else
            sectors = ld_parse_size_input(buf, max_sectors);
    }

    if (sectors == 0) {
        strcpy(S.msg, " Invalid size.");
        return;
    }
    if (sectors % 2048 != 0) {
        sectors = (sectors / 2048) * 2048;
        if (sectors == 0) sectors = 2048;
    }

    slot = S.disk.part_count;
    if (!S.disk.is_gpt) {
        memset(used, 0, sizeof(used));
        for (i = 0; i < S.disk.part_count; i++) {
            k = S.disk.parts[i].number - 1;
            if (S.disk.parts[i].valid && k >= 0 && k < 4)
                used[k] = 1;
        }
        next_num = 0;
        for (k = 0; k < 4; k++) {
            if (!used[k]) {
                next_num = k + 1;
                break;
            }
        }
        if (next_num == 0) {
            strcpy(S.msg, " No free MBR slot available.");
            return;
        }
    } else {
        next_num = 1;
        for (i = 0; i < S.disk.part_count; i++) {
            if (S.disk.parts[i].valid && S.disk.parts[i].number >= next_num)
                next_num = S.disk.parts[i].number + 1;
        }
    }

    S.disk.parts[slot].valid = 1;
    S.disk.parts[slot].number = next_num;
    S.disk.parts[slot].start_lba = start;
    S.disk.parts[slot].sector_count = sectors;

    if (S.disk.is_gpt) {
        memcpy(S.disk.parts[slot].gpt_type_guid, LD_GPT_LINUX_FS, 16);
        S.disk.parts[slot].gpt_name[0] = '\0';
    } else {
        S.disk.parts[slot].mbr_type = 0x83;
    }

    S.disk.part_count++;
    S.disk.modified = 1;
    S.sel_part = S.disk.part_count - 1;

    ld_format_size(sectors, sizebuf, sizeof(sizebuf));
    snprintf(S.msg, sizeof(S.msg), " Created partition %d (%s)", next_num, sizebuf);
}

static void ld_action_delete(void) {
    int idx;
    int j;

    if (S.disk.part_count == 0) {
        strcpy(S.msg, " No partitions to delete.");
        return;
    }

    idx = S.sel_part;
    if (idx < 0 || idx >= S.disk.part_count) return;

    if (!ld_confirm("Delete selected partition?")) {
        S.msg[0] = '\0';
        return;
    }

    for (j = idx; j < S.disk.part_count - 1; j++)
        S.disk.parts[j] = S.disk.parts[j + 1];
    memset(&S.disk.parts[S.disk.part_count - 1], 0, sizeof(ld_part_info_t));
    S.disk.part_count--;
    S.disk.modified = 1;

    if (S.sel_part >= S.disk.part_count && S.sel_part > 0)
        S.sel_part = S.disk.part_count - 1;

    strcpy(S.msg, " Partition deleted.");
}

static void ld_action_resize(void) {
    int idx;
    uint64_t cur_start;
    uint64_t cur_sectors;
    uint64_t max_end;
    uint64_t nearest_next;
    uint64_t max_sectors;
    uint64_t new_sectors;
    char buf[64];
    char cur_size[32];
    char max_size[32];
    char prompt[96];
    int i;

    if (S.disk.part_count == 0) {
        strcpy(S.msg, " No partitions to resize.");
        return;
    }

    idx = S.sel_part;
    if (idx < 0 || idx >= S.disk.part_count) return;
    if (!S.disk.parts[idx].valid) return;

    cur_start = S.disk.parts[idx].start_lba;
    cur_sectors = S.disk.parts[idx].sector_count;

    if (S.disk.is_gpt)
        max_end = S.disk.disk_sectors > 34 ? S.disk.disk_sectors - 34 : S.disk.disk_sectors;
    else
        max_end = S.disk.disk_sectors;

    nearest_next = max_end;
    for (i = 0; i < S.disk.part_count; i++) {
        if (i == idx) continue;
        if (!S.disk.parts[i].valid) continue;
        if (S.disk.parts[i].start_lba > cur_start &&
            S.disk.parts[i].start_lba < nearest_next)
            nearest_next = S.disk.parts[i].start_lba;
    }

    max_sectors = nearest_next > cur_start ? nearest_next - cur_start : 0;
    if (max_sectors == 0) {
        strcpy(S.msg, " Cannot resize: no room.");
        return;
    }

    ld_format_size(cur_sectors, cur_size, sizeof(cur_size));
    ld_format_size(max_sectors, max_size, sizeof(max_size));
    snprintf(prompt, sizeof(prompt), "New size (current %s, max %s)", cur_size, max_size);
    if (ld_prompt_input(prompt, buf, sizeof(buf)) < 0) return;

    if (buf[0] == '\0') {
        strcpy(S.msg, " Resize cancelled.");
        return;
    }
    if (buf[0] == '+')
        new_sectors = ld_parse_size_input(buf + 1, max_sectors);
    else
        new_sectors = ld_parse_size_input(buf, max_sectors);

    if (new_sectors == 0) {
        strcpy(S.msg, " Invalid size.");
        return;
    }
    if (new_sectors % 2048 != 0) {
        new_sectors = (new_sectors / 2048) * 2048;
        if (new_sectors == 0) new_sectors = 2048;
    }
    if (new_sectors > max_sectors)
        new_sectors = max_sectors;

    S.disk.parts[idx].sector_count = new_sectors;
    S.disk.modified = 1;

    ld_format_size(new_sectors, cur_size, sizeof(cur_size));
    snprintf(S.msg, sizeof(S.msg), " Partition %d resized to %s", S.disk.parts[idx].number, cur_size);
}

static int ld_type_selector(const char *title, const char **names, int count) {
    int sel;
    int top;
    int max_vis;
    int key;
    int i;
    int row;
    char line[128];

    sel = 0;
    top = 0;
    max_vis = S.screen_rows - 12;
    if (max_vis < 3) max_vis = 3;
    if (max_vis > count) max_vis = count;

    for (;;) {
        ld_move_cursor(7, 1);
        ld_set_color(1, 0);
        snprintf(line, sizeof(line), "  %s", title);
        ld_draw_padded(line, S.screen_cols - 1);
        ld_reset_color();

        for (i = 0; i < max_vis; i++) {
            row = 8 + i;
            ld_move_cursor(row, 1);
            if (top + i < count) {
                snprintf(line, sizeof(line), "    %s", names[top + i]);
                if (top + i == sel) {
                    ld_set_color(30, 47);
                    ld_draw_padded(line, S.screen_cols - 1);
                    ld_reset_color();
                } else {
                    ld_draw_padded(line, S.screen_cols - 1);
                }
            } else {
                ld_clear_line();
            }
        }

        ld_move_cursor(S.screen_rows - 1, 1);
        ld_set_color(37, 44);
        ld_draw_padded(" Up/Down to select, Enter to confirm, Esc to cancel", S.screen_cols - 1);
        ld_reset_color();

        key = ld_read_key();
        if (key == LD_KEY_UP && sel > 0) {
            sel--;
            if (sel < top) top = sel;
        } else if (key == LD_KEY_DOWN && sel < count - 1) {
            sel++;
            if (sel >= top + max_vis) top = sel - max_vis + 1;
        } else if (key == LD_KEY_ENTER) {
            return sel;
        } else if (key == '\x1b') {
            return -1;
        }
    }
}

static void ld_action_type(void) {
    int idx;
    int sel;
    int i;
    const char *names[16];

    if (S.disk.part_count == 0) {
        strcpy(S.msg, " No partitions.");
        return;
    }

    idx = S.sel_part;
    if (idx < 0 || idx >= S.disk.part_count) return;

    if (S.disk.is_gpt) {
        for (i = 0; i < LD_GPT_TYPE_COUNT; i++)
            names[i] = ld_gpt_types[i].name;
        sel = ld_type_selector("Select GPT partition type:", names, LD_GPT_TYPE_COUNT);
        if (sel < 0) return;
        memcpy(S.disk.parts[idx].gpt_type_guid, ld_gpt_types[sel].guid, 16);
    } else {
        static char mbr_labels[16][32];
        for (i = 0; i < LD_MBR_TYPE_COUNT; i++) {
            snprintf(mbr_labels[i], sizeof(mbr_labels[i]), "%02X  %s",
                     ld_mbr_types[i].code, ld_mbr_types[i].name);
            names[i] = mbr_labels[i];
        }
        sel = ld_type_selector("Select MBR partition type:", names, LD_MBR_TYPE_COUNT);
        if (sel < 0) return;
        S.disk.parts[idx].mbr_type = ld_mbr_types[sel].code;
    }
    S.disk.modified = 1;
    strcpy(S.msg, " Partition type changed.");
}

static int ld_write_mbr(ld_disk_t *disk) {
    ld_mbr_t mbr;
    int i;
    int slot;

    memset(&mbr, 0, sizeof(mbr));
    memcpy(mbr.bootstrap, disk->mbr_bootstrap, 446);
    mbr.signature = MBR_SIG;

    for (i = 0; i < disk->part_count; i++) {
        if (!disk->parts[i].valid) continue;
        slot = disk->parts[i].number - 1;
        if (slot < 0 || slot >= 4) continue;
        mbr.parts[slot].status = 0;
        mbr.parts[slot].type = disk->parts[i].mbr_type;
        mbr.parts[slot].lba_start = (uint32_t)disk->parts[i].start_lba;
        mbr.parts[slot].sector_count = (uint32_t)disk->parts[i].sector_count;
    }

    if (ld_disk_write(disk->devpath, 0, 1, &mbr) < SECTOR_SIZE)
        return -1;
    return 0;
}

static int ld_write_gpt(ld_disk_t *disk) {
    static uint8_t entries_buf[SECTOR_SIZE * 32];
    static uint8_t hdr_sector[SECTOR_SIZE];
    static uint8_t alt_sector[SECTOR_SIZE];
    ld_mbr_t pmbr;
    ld_gpt_header_t hdr;
    ld_gpt_header_t alt_hdr;
    ld_gpt_entry_t *entry;
    uint32_t entries_crc;
    uint32_t entries_sectors;
    uint32_t alt_entries_lba;
    uint64_t alt_lba;
    int i;
    int num_entries;

    if (disk->disk_sectors < 2048) return -1;

    num_entries = 128;
    entries_sectors = (uint32_t)((num_entries * sizeof(ld_gpt_entry_t) + 511) / 512);

    memset(&pmbr, 0, sizeof(pmbr));
    pmbr.signature = MBR_SIG;
    pmbr.parts[0].type = 0xEE;
    pmbr.parts[0].lba_start = 1;
    if (disk->disk_sectors - 1 > 0xFFFFFFFF)
        pmbr.parts[0].sector_count = 0xFFFFFFFF;
    else
        pmbr.parts[0].sector_count = (uint32_t)(disk->disk_sectors - 1);

    if (ld_disk_write(disk->devpath, 0, 1, &pmbr) < SECTOR_SIZE)
        return -1;

    alt_lba = disk->disk_sectors - 1;

    memset(entries_buf, 0, sizeof(entries_buf));
    for (i = 0; i < disk->part_count && i < num_entries; i++) {
        if (!disk->parts[i].valid) continue;
        entry = (ld_gpt_entry_t *)(entries_buf + i * sizeof(ld_gpt_entry_t));
        memcpy(entry->type_guid, disk->parts[i].gpt_type_guid, 16);
        ld_generate_guid(entry->unique_guid);
        entry->starting_lba = disk->parts[i].start_lba;
        entry->ending_lba = disk->parts[i].start_lba + disk->parts[i].sector_count - 1;
        entry->attributes = 0;
        {
            int k;
            int nlen;
            nlen = (int)strlen(disk->parts[i].gpt_name);
            if (nlen > 36) nlen = 36;
            for (k = 0; k < nlen; k++)
                entry->name[k] = (uint16_t)(unsigned char)disk->parts[i].gpt_name[k];
        }
    }

    entries_crc = ld_crc32(entries_buf, num_entries * sizeof(ld_gpt_entry_t));

    memset(&hdr, 0, sizeof(hdr));
    hdr.signature = GPT_SIG_VAL;
    hdr.revision = 0x00010000;
    hdr.header_size = 92;
    hdr.my_lba = 1;
    hdr.alternate_lba = alt_lba;
    hdr.first_usable_lba = 2 + entries_sectors;
    hdr.last_usable_lba = alt_lba - 1 - entries_sectors;
    memcpy(hdr.disk_guid, disk->gpt_disk_guid, 16);
    hdr.partition_entry_lba = 2;
    hdr.num_partition_entries = num_entries;
    hdr.partition_entry_size = sizeof(ld_gpt_entry_t);
    hdr.partition_array_crc32 = entries_crc;
    hdr.header_crc32 = 0;
    hdr.header_crc32 = ld_crc32((const uint8_t *)&hdr, 92);

    memset(hdr_sector, 0, SECTOR_SIZE);
    memcpy(hdr_sector, &hdr, sizeof(hdr));
    if (ld_disk_write(disk->devpath, 1, 1, hdr_sector) < SECTOR_SIZE)
        return -1;

    if (ld_disk_write(disk->devpath, 2, entries_sectors, entries_buf) < (int)(entries_sectors * SECTOR_SIZE))
        return -1;

    alt_entries_lba = (uint32_t)(alt_lba - entries_sectors);
    ld_disk_write(disk->devpath, alt_entries_lba, entries_sectors, entries_buf);

    memcpy(&alt_hdr, &hdr, sizeof(hdr));
    alt_hdr.my_lba = alt_lba;
    alt_hdr.alternate_lba = 1;
    alt_hdr.partition_entry_lba = alt_entries_lba;
    alt_hdr.header_crc32 = 0;
    alt_hdr.header_crc32 = ld_crc32((const uint8_t *)&alt_hdr, 92);

    memset(alt_sector, 0, SECTOR_SIZE);
    memcpy(alt_sector, &alt_hdr, sizeof(alt_hdr));
    ld_disk_write(disk->devpath, (uint32_t)alt_lba, 1, alt_sector);

    return 0;
}

static void ld_action_write(void) {
    int ret;

    if (!ld_confirm("Write partition table to disk?")) {
        S.msg[0] = '\0';
        return;
    }

    if (S.disk.is_gpt)
        ret = ld_write_gpt(&S.disk);
    else
        ret = ld_write_mbr(&S.disk);

    if (ret == 0) {
        S.disk.modified = 0;
        leb_syscall1(LEB_SYSCALL_BLOCKDEV_RESCAN, (long)S.disk.devname);
        strcpy(S.msg, " Partition table written successfully.");
    } else {
        strcpy(S.msg, " ERROR: Failed to write partition table!");
    }
}

static void ld_action_wipe(void) {
    static uint8_t zero_buf[SECTOR_SIZE * 8];
    int ret;

    if (!ld_confirm("Wipe filesystem signatures from disk?")) {
        S.msg[0] = '\0';
        return;
    }

    memset(zero_buf, 0, sizeof(zero_buf));

    ret = ld_disk_write(S.disk.devpath, 0, 8, zero_buf);
    if (ret < (int)sizeof(zero_buf)) {
        strcpy(S.msg, " ERROR: Failed to wipe signatures!");
        return;
    }

    strcpy(S.msg, " Filesystem signatures wiped (first 4 KiB zeroed).");
}

static void ld_action_create_dos(void) {
    if (S.disk.part_count > 0 || S.disk.is_gpt) {
        if (!ld_confirm("Create new DOS label? All partitions will be lost!")) {
            S.msg[0] = '\0';
            return;
        }
    }
    S.disk.is_gpt = 0;
    S.disk.part_count = 0;
    memset(S.disk.parts, 0, sizeof(S.disk.parts));
    memset(S.disk.mbr_bootstrap, 0, 446);
    S.disk.modified = 1;
    S.sel_part = 0;
    strcpy(S.msg, " Created new DOS (MBR) disklabel.");
}

static void ld_action_create_gpt(void) {
    if (S.disk.part_count > 0 || !S.disk.is_gpt) {
        if (!ld_confirm("Create new GPT label? All partitions will be lost!")) {
            S.msg[0] = '\0';
            return;
        }
    }
    if (S.disk.disk_sectors == 0) {
        char buf[64];
        if (ld_prompt_input("Total disk sectors (required for GPT)", buf, sizeof(buf)) < 0) return;
        S.disk.disk_sectors = strtoull(buf, NULL, 10);
        if (S.disk.disk_sectors == 0) {
            strcpy(S.msg, " Invalid disk size.");
            return;
        }
    }
    S.disk.is_gpt = 1;
    S.disk.part_count = 0;
    memset(S.disk.parts, 0, sizeof(S.disk.parts));
    ld_generate_guid(S.disk.gpt_disk_guid);
    S.disk.modified = 1;
    S.sel_part = 0;
    strcpy(S.msg, " Created new GPT disklabel.");
}

static void ld_action_help(void) {
    ld_clear_screen();
    ld_move_cursor(1, 1);
    ld_set_color(37, 44);
    ld_draw_padded(" ldiskutil Help", S.screen_cols - 1);
    ld_reset_color();

    ld_move_cursor(3, 1);
    ld_puts("  Navigation:");
    ld_move_cursor(4, 1);
    ld_puts("    Up/Down     Select partition");
    ld_move_cursor(5, 1);
    ld_puts("    Left/Right  Select action");
    ld_move_cursor(6, 1);
    ld_puts("    Enter       Execute selected action");
    ld_move_cursor(7, 1);
    ld_puts("    q           Quit");
    ld_move_cursor(9, 1);
    ld_puts("  Actions:");
    ld_move_cursor(10, 1);
    ld_puts("    New         Create a new partition in free space");
    ld_move_cursor(11, 1);
    ld_puts("    Delete      Delete the selected partition");
    ld_move_cursor(12, 1);
    ld_puts("    Type        Change partition type (Linux, swap, EFI, etc.)");
    ld_move_cursor(13, 1);
    ld_puts("    Resize      Resize the selected partition");
    ld_move_cursor(14, 1);
    ld_puts("    Write       Write partition table to disk");
    ld_move_cursor(15, 1);
    ld_puts("    Wipe        Wipe filesystem signatures from disk");
    ld_move_cursor(16, 1);
    ld_puts("    DOS         Create new empty MBR partition table");
    ld_move_cursor(17, 1);
    ld_puts("    GPT         Create new empty GPT partition table");
    ld_move_cursor(18, 1);
    ld_puts("    Help        Show this help screen");
    ld_move_cursor(19, 1);
    ld_puts("    Quit        Exit ldiskutil");
    ld_move_cursor(21, 1);
    ld_puts("  Size input accepts: 100 (sectors), 100M (megabytes), 1G (gigabytes)");
    ld_move_cursor(22, 1);
    ld_puts("  Prefix with + for relative size: +512M, +1G");

    ld_move_cursor(S.screen_rows - 1, 1);
    ld_set_color(37, 44);
    ld_draw_padded(" Press any key to return...", S.screen_cols - 1);
    ld_reset_color();

    ld_read_key();
}

static void ld_action_quit(void) {
    if (S.disk.modified) {
        if (!ld_confirm("Discard unsaved changes?")) {
            S.msg[0] = '\0';
            return;
        }
    }
    S.running = 0;
}

static void ld_handle_action(void) {
    switch (S.sel_action) {
    case LD_ACT_NEW:    ld_action_new(); break;
    case LD_ACT_DELETE: ld_action_delete(); break;
    case LD_ACT_TYPE:   ld_action_type(); break;
    case LD_ACT_RESIZE: ld_action_resize(); break;
    case LD_ACT_WRITE:  ld_action_write(); break;
    case LD_ACT_WIPE:   ld_action_wipe(); break;
    case LD_ACT_DUMBBR: ld_action_create_dos(); break;
    case LD_ACT_DGPT:   ld_action_create_gpt(); break;
    case LD_ACT_HELP:   ld_action_help(); break;
    case LD_ACT_QUIT:   ld_action_quit(); break;
    }
}

static void ld_process_key(int key) {
    switch (key) {
    case LD_KEY_UP:
        if (S.sel_part > 0)
            S.sel_part--;
        S.msg[0] = '\0';
        break;
    case LD_KEY_DOWN:
        if (S.sel_part < S.disk.part_count - 1)
            S.sel_part++;
        S.msg[0] = '\0';
        break;
    case LD_KEY_LEFT:
        if (S.sel_action > 0) S.sel_action--;
        S.msg[0] = '\0';
        break;
    case LD_KEY_RIGHT:
        if (S.sel_action < LD_ACT_COUNT - 1) S.sel_action++;
        S.msg[0] = '\0';
        break;
    case LD_KEY_ENTER:
        ld_handle_action();
        break;
    case 'n': case 'N':
        S.sel_action = LD_ACT_NEW;
        ld_handle_action();
        break;
    case 'd': case 'D':
        S.sel_action = LD_ACT_DELETE;
        ld_handle_action();
        break;
    case 't': case 'T':
        S.sel_action = LD_ACT_TYPE;
        ld_handle_action();
        break;
    case 'r': case 'R':
        S.sel_action = LD_ACT_RESIZE;
        ld_handle_action();
        break;
    case 'w': case 'W':
        S.sel_action = LD_ACT_WRITE;
        ld_handle_action();
        break;
    case 'q': case 'Q':
        S.sel_action = LD_ACT_QUIT;
        ld_handle_action();
        break;
    case 'h': case 'H': case '?':
        S.sel_action = LD_ACT_HELP;
        ld_handle_action();
        break;
    }
}

static int ld_is_whole_disk(const char *name) {
    int i;

    if (name[0] != 's' || name[1] != 'd')
        return 0;
    if (name[2] < 'a' || name[2] > 'z')
        return 0;
    for (i = 3; name[i]; i++) {
        if (name[i] >= '0' && name[i] <= '9')
            return 0;
    }
    return 1;
}

static int ld_tui(const char *devpath) {
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
    unsigned int font_h;
    unsigned int rows;
    unsigned int cursor_row;
    unsigned int font_w;
    unsigned int cols;
    const char *p;

    memset(&S, 0, sizeof(S));

    p = devpath;
    if (p[0] == '/' && p[1] == 'd' && p[2] == 'e' && p[3] == 'v' && p[4] == '/')
        p += 5;

    if (!ld_is_whole_disk(p)) {
        printf("Error: %s is not a whole disk device.\n", devpath);
        printf("ldiskutil only works with whole disks (e.g. /dev/sda), not partitions.\n");
        return 1;
    }

    strncpy(S.disk.devname, p, sizeof(S.disk.devname) - 1);
    strncpy(S.disk.devpath, devpath, sizeof(S.disk.devpath) - 1);

    fb_getinfo(&width, &height, &bpp, &font_h, &rows, &cursor_row, &font_w, &cols);
    S.screen_rows = (int)rows;
    S.screen_cols = (int)cols;
    if (S.screen_rows < 10) S.screen_rows = 24;
    if (S.screen_cols < 40) S.screen_cols = 80;

    ld_scan_disk(&S.disk);

    S.sel_part = 0;
    S.sel_action = 0;
    S.running = 1;

    ld_enable_raw();
    ld_puts("\x1b[?1049h");
    ld_clear_screen();
    ld_draw_all();

    while (S.running) {
        int key;
        key = ld_read_key();
        if (key < 0) continue;
        ld_process_key(key);
        if (S.running) ld_draw_all();
    }

    ld_puts("\x1b[?1049l");
    ld_clear_screen();
    ld_show_cursor();
    ld_reset_color();
    ld_disable_raw();

    return 0;
}

static void ld_print_partitions(ld_disk_t *disk) {
    int i;
    char sizebuf[32];

    printf("\nDisk /dev/%s: ", disk->devname);
    if (disk->disk_sectors > 0) {
        ld_format_size(disk->disk_sectors, sizebuf, sizeof(sizebuf));
        printf("%s, %llu sectors\n", sizebuf, (unsigned long long)disk->disk_sectors);
    } else {
        printf("unknown size\n");
    }
    printf("Disklabel type: %s\n", disk->is_gpt ? "gpt" : "dos");

    if (disk->part_count == 0) {
        printf("No partitions.\n");
        return;
    }

    if (disk->is_gpt) {
        printf("%-6s %12s %12s %12s  %-8s  %s\n",
               "#", "Start", "End", "Sectors", "Size", "Type");
        for (i = 0; i < disk->part_count; i++) {
            if (!disk->parts[i].valid) continue;
            ld_format_size(disk->parts[i].sector_count, sizebuf, sizeof(sizebuf));
            printf("%-6d %12llu %12llu %12llu  %-8s  %s\n",
                   disk->parts[i].number,
                   (unsigned long long)disk->parts[i].start_lba,
                   (unsigned long long)(disk->parts[i].start_lba + disk->parts[i].sector_count - 1),
                   (unsigned long long)disk->parts[i].sector_count,
                   sizebuf,
                   ld_gpt_type_name(disk->parts[i].gpt_type_guid));
        }
    } else {
        printf("%-6s %12s %12s %12s  %-8s  %-4s  %s\n",
               "#", "Start", "End", "Sectors", "Size", "Id", "Type");
        for (i = 0; i < disk->part_count; i++) {
            if (!disk->parts[i].valid) continue;
            ld_format_size(disk->parts[i].sector_count, sizebuf, sizeof(sizebuf));
            printf("%-6d %12llu %12llu %12llu  %-8s  %02x    %s\n",
                   disk->parts[i].number,
                   (unsigned long long)disk->parts[i].start_lba,
                   (unsigned long long)(disk->parts[i].start_lba + disk->parts[i].sector_count - 1),
                   (unsigned long long)disk->parts[i].sector_count,
                   sizebuf,
                   disk->parts[i].mbr_type,
                   ld_mbr_type_name(disk->parts[i].mbr_type));
        }
    }
}

static void ld_usage(void) {
    printf("Usage: ldiskutil [OPTIONS] [DEVICE]\n");
    printf("\nPartition table manipulator for Lebirun.\n");
    printf("\nOptions:\n");
    printf("  -l           List partition tables on all disks\n");
    printf("  -p DEVICE    Print partition table of a device\n");
    printf("  --help       Show this help\n");
    printf("\nExamples:\n");
    printf("  ldiskutil /dev/sda      Interactive TUI on /dev/sda\n");
    printf("  ldiskutil -l            List all disks and partitions\n");
    printf("  ldiskutil -p /dev/sda   Print partition table\n");
}

int cmd_ldiskutil(int argc, char **argv) {
    ld_disk_t disk;
    const char *devpath;

    if (argc < 2) {
        ld_usage();
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        ld_usage();
        return 0;
    }

    if (strcmp(argv[1], "-l") == 0) {
        int fd;
        char name[64];
        unsigned int type;
        unsigned int idx;

        fd = vfs_open("/dev", 0);
        if (fd < 0) {
            printf("Error: Cannot open /dev\n");
            return 1;
        }

        for (idx = 0; ; idx++) {
            if (vfs_readdir(fd, name, &type, idx) != 0) break;
            if (name[0] == 's' && name[1] == 'd' && name[2] >= 'a' && name[2] <= 'z') {
                int has_digit;
                int k;

                has_digit = 0;
                for (k = 3; name[k]; k++) {
                    if (name[k] >= '0' && name[k] <= '9') has_digit = 1;
                }
                if (!has_digit) {
                    memset(&disk, 0, sizeof(disk));
                    strcpy(disk.devname, name);
                    snprintf(disk.devpath, sizeof(disk.devpath), "/dev/%s", name);
                    ld_scan_disk(&disk);
                    ld_print_partitions(&disk);
                    printf("\n");
                }
            }
        }
        vfs_close_fd(fd);
        return 0;
    }

    if (strcmp(argv[1], "-p") == 0) {
        const char *p;

        if (argc < 3) {
            printf("Error: -p requires a device path.\n");
            return 1;
        }
        devpath = argv[2];
        memset(&disk, 0, sizeof(disk));
        p = devpath;
        if (p[0] == '/' && p[1] == 'd' && p[2] == 'e' && p[3] == 'v' && p[4] == '/')
            p += 5;
        if (!ld_is_whole_disk(p)) {
            printf("Error: %s is not a whole disk device.\n", devpath);
            printf("ldiskutil only works with whole disks (e.g. /dev/sda), not partitions.\n");
            return 1;
        }
        strncpy(disk.devname, p, sizeof(disk.devname) - 1);
        strncpy(disk.devpath, devpath, sizeof(disk.devpath) - 1);
        if (ld_scan_disk(&disk) < 0) {
            printf("Error: Cannot read disk %s\n", devpath);
            return 1;
        }
        ld_print_partitions(&disk);
        return 0;
    }

    return ld_tui(argv[1]);
}
