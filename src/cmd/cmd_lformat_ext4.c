#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <lebirun.h>
#include "cu.h"

#define EXT4_SUPER_MAGIC     0xEF53
#define EXT4_SB_OFFSET       1024
#define EXT4_ROOT_INO        2
#define EXT4_GOOD_OLD_FIRST  11
#define EXT4_FT_DIR          2
#define EXT4_S_IFDIR         0x4000

#define BLOCK_SIZE    4096
#define INODE_SIZE    256
#define SECT_SIZE     512

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t  s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint32_t s_last_error_line;
    uint64_t s_last_error_block;
    uint8_t  s_last_error_func[32];
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_blocks;
    uint32_t s_backup_bgs[2];
    uint8_t  s_encrypt_algos[4];
    uint8_t  s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint32_t s_reserved[98];
    uint32_t s_checksum;
} __attribute__((packed)) lf_sb_t;

typedef struct {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __attribute__((packed)) lf_gd_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint16_t i_checksum_lo;
    uint16_t i_reserved;
    uint16_t i_extra_isize;
    uint16_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    uint32_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    uint32_t i_projid;
} __attribute__((packed)) lf_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[255];
} __attribute__((packed)) lf_dirent_t;

static int lf_write_block(int fd, uint32_t block, const uint8_t *data) {
    int ret;
    off_t pos;

    pos = lseek(fd, (off_t)block * BLOCK_SIZE, SEEK_SET);
    if (pos < 0) {
        fprintf(stderr, "lformat.ext4: lseek to block %u failed (%d)\n", block, (int)pos);
        return -1;
    }
    ret = vfs_write_fd(fd, data, BLOCK_SIZE);
    if (ret != BLOCK_SIZE) {
        fprintf(stderr, "lformat.ext4: write block %u failed (got %d)\n", block, ret);
        return -1;
    }
    return ret;
}

static void lf_gen_uuid(uint8_t *uuid) {
    int rfd;
    int i;
    uint32_t v;

    rfd = vfs_open("/dev/urandom", 0);
    if (rfd >= 0) {
        vfs_read_fd(rfd, uuid, 16);
        vfs_close_fd(rfd);
    } else {
        v = (uint32_t)getticks();
        for (i = 0; i < 16; i++) {
            v = v * 1103515245 + 12345;
            uuid[i] = (uint8_t)(v >> 16);
        }
    }
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

int cmd_lformat_ext4(int argc, char **argv) {
    int fd;
    int rfd;
    const char *devpath;
    const char *label;
    uint64_t dev_size;
    uint64_t dev_type;
    uint32_t total_sectors;
    uint32_t total_blocks;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t groups;
    uint32_t inode_table_blocks;
    uint32_t overhead_per_group;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint32_t gdt_blocks;
    uint32_t g;
    uint32_t blk;
    uint32_t bb_block;
    uint32_t ib_block;
    uint32_t it_block;
    uint32_t first_data;
    uint32_t grp_blocks;
    uint32_t used_blocks;
    uint32_t bit;
    uint32_t byte_idx;
    int i;
    int ret;
    uint8_t *block_buf;
    uint8_t *verify_buf;
    lf_sb_t *sb;
    lf_gd_t *gd;
    lf_inode_t *inode;
    lf_dirent_t *dot;
    lf_dirent_t *dotdot;
    lf_dirent_t *end;
    lf_inode_t *ri;
    uint8_t uuid[16];

    fd = -1;
    block_buf = NULL;
    verify_buf = NULL;
    sb = NULL;
    gd = NULL;
    inode = NULL;
    ret = 1;

    label = "";
    if (argc < 2) {
        printf("Usage: lformat.ext4 <device> [-L label]\n");
        printf("  e.g. lformat.ext4 /dev/sda1\n");
        printf("       lformat.ext4 /dev/sda1 -L myvolume\n");
        return 1;
    }

    devpath = argv[1];
    for (i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-L") == 0) {
            label = argv[i + 1];
            i++;
        }
    }

    rfd = vfs_open(devpath, 0);
    if (rfd < 0) {
        printf("lformat.ext4: cannot open %s\n", devpath);
        return 1;
    }
    if (vfs_stat(rfd, &dev_size, &dev_type) != 0 || dev_size == 0) {
        vfs_close_fd(rfd);
        printf("lformat.ext4: cannot determine size of %s\n", devpath);
        return 1;
    }
    vfs_close_fd(rfd);

    total_sectors = dev_size / SECT_SIZE;
    total_blocks = dev_size / BLOCK_SIZE;

    if (total_blocks < 64) {
        printf("lformat.ext4: device too small (%u blocks)\n", total_blocks);
        return 1;
    }

    blocks_per_group = BLOCK_SIZE * 8;
    inodes_per_group = total_blocks / 32;
    if (inodes_per_group < 16) inodes_per_group = 16;
    if (inodes_per_group > 1024) inodes_per_group = 1024;
    if (inodes_per_group > (uint32_t)(BLOCK_SIZE * 8)) inodes_per_group = BLOCK_SIZE * 8;

    groups = (total_blocks + blocks_per_group - 1) / blocks_per_group;
    if (groups == 0) groups = 1;

    gdt_blocks = (groups * 32 + BLOCK_SIZE - 1) / BLOCK_SIZE;

    inode_table_blocks = (inodes_per_group * INODE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;

    overhead_per_group = 1 + gdt_blocks + 1 + 1 + inode_table_blocks;

    if (groups == 1 && total_blocks <= overhead_per_group + 2) {
        inodes_per_group = 16;
        inode_table_blocks = (inodes_per_group * INODE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        overhead_per_group = 1 + gdt_blocks + 1 + 1 + inode_table_blocks;
    }

    total_inodes = inodes_per_group * groups;
    free_inodes = total_inodes - EXT4_GOOD_OLD_FIRST;
    free_blocks = 0;

    printf("Creating ext4 filesystem on %s\n", devpath);
    printf("  %u blocks (%u sectors), %u block group(s)\n", total_blocks, total_sectors, groups);
    printf("  %u inodes, inode size %u, block size %u\n", total_inodes, INODE_SIZE, BLOCK_SIZE);

    fd = vfs_open(devpath, 2);
    if (fd < 0) {
        printf("lformat.ext4: cannot open %s for writing\n", devpath);
        return 1;
    }

    block_buf = (uint8_t *)malloc(BLOCK_SIZE);
    verify_buf = (uint8_t *)malloc(BLOCK_SIZE);
    sb = (lf_sb_t *)malloc(sizeof(lf_sb_t));
    gd = (lf_gd_t *)malloc(sizeof(lf_gd_t));
    inode = (lf_inode_t *)malloc(sizeof(lf_inode_t));
    if (!block_buf || !verify_buf || !sb || !gd || !inode) {
        printf("lformat.ext4: out of memory\n");
        goto out;
    }

    lf_gen_uuid(uuid);

    memset(sb, 0, sizeof(*sb));
    sb->s_inodes_count = total_inodes;
    sb->s_blocks_count_lo = total_blocks;
    sb->s_r_blocks_count_lo = 0;
    sb->s_free_blocks_count_lo = 0;
    sb->s_free_inodes_count = free_inodes;
    sb->s_first_data_block = 0;
    sb->s_log_block_size = 2;
    sb->s_log_cluster_size = 2;
    sb->s_blocks_per_group = blocks_per_group;
    sb->s_clusters_per_group = blocks_per_group;
    sb->s_inodes_per_group = inodes_per_group;
    sb->s_mtime = 0;
    sb->s_wtime = 0;
    sb->s_mnt_count = 0;
    sb->s_max_mnt_count = 20;
    sb->s_magic = EXT4_SUPER_MAGIC;
    sb->s_state = 1;
    sb->s_errors = 1;
    sb->s_minor_rev_level = 0;
    sb->s_lastcheck = 0;
    sb->s_checkinterval = 0;
    sb->s_creator_os = 5;
    sb->s_rev_level = 1;
    sb->s_def_resuid = 0;
    sb->s_def_resgid = 0;
    sb->s_first_ino = EXT4_GOOD_OLD_FIRST;
    sb->s_inode_size = INODE_SIZE;
    sb->s_block_group_nr = 0;
    sb->s_feature_compat = 0;
    sb->s_feature_incompat = 0x0002;
    sb->s_feature_ro_compat = 0x0003;
    memcpy(sb->s_uuid, uuid, 16);
    if (label[0]) {
        strncpy(sb->s_volume_name, label, 16);
    }
    sb->s_desc_size = 32;
    sb->s_min_extra_isize = 28;
    sb->s_want_extra_isize = 28;

    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf + EXT4_SB_OFFSET, sb, sizeof(*sb));
    lf_write_block(fd, 0, block_buf);

    for (g = 0; g < groups; g++) {
        blk = g * blocks_per_group;
        bb_block = blk + 1 + gdt_blocks;
        ib_block = bb_block + 1;
        it_block = ib_block + 1;

        grp_blocks = blocks_per_group;
        if (blk + grp_blocks > total_blocks)
            grp_blocks = total_blocks - blk;

        used_blocks = 1 + gdt_blocks + 1 + 1 + inode_table_blocks;
        if (used_blocks > grp_blocks)
            used_blocks = grp_blocks;

        memset(gd, 0, sizeof(*gd));
        gd->bg_block_bitmap_lo = bb_block;
        gd->bg_inode_bitmap_lo = ib_block;
        gd->bg_inode_table_lo = it_block;
        gd->bg_free_blocks_count_lo = (uint16_t)(grp_blocks - used_blocks);
        if (g == 0) {
            gd->bg_free_inodes_count_lo = (uint16_t)(inodes_per_group - EXT4_GOOD_OLD_FIRST);
            gd->bg_used_dirs_count_lo = 1;
        } else {
            gd->bg_free_inodes_count_lo = (uint16_t)inodes_per_group;
            gd->bg_used_dirs_count_lo = 0;
        }

        free_blocks += gd->bg_free_blocks_count_lo;

        memset(block_buf, 0, BLOCK_SIZE);
        lseek(fd, (off_t)BLOCK_SIZE + (off_t)g * 32, SEEK_SET);
        vfs_write_fd(fd, gd, 32);

        memset(block_buf, 0, BLOCK_SIZE);
        for (bit = 0; bit < used_blocks; bit++) {
            byte_idx = bit / 8;
            block_buf[byte_idx] |= (1 << (bit & 7));
        }
        if (g == 0) {
            byte_idx = (it_block + inode_table_blocks - blk) / 8;
            bit = (it_block + inode_table_blocks - blk) & 7;
            block_buf[byte_idx] |= (1 << bit);
        }
        for (bit = grp_blocks; bit < blocks_per_group; bit++) {
            byte_idx = bit / 8;
            block_buf[byte_idx] |= (1 << (bit & 7));
        }
        lf_write_block(fd, bb_block, block_buf);

        memset(block_buf, 0, BLOCK_SIZE);
        if (g == 0) {
            for (bit = 0; bit < EXT4_GOOD_OLD_FIRST; bit++) {
                byte_idx = bit / 8;
                block_buf[byte_idx] |= (1 << (bit & 7));
            }
        }
        for (bit = inodes_per_group; bit < (uint32_t)(BLOCK_SIZE * 8); bit++) {
            byte_idx = bit / 8;
            block_buf[byte_idx] |= (1 << (bit & 7));
        }
        lf_write_block(fd, ib_block, block_buf);

        memset(block_buf, 0, BLOCK_SIZE);
        {
            uint32_t batch;
            uint32_t chunk;
            uint32_t remain;
            uint8_t *zbuf;
            off_t zpos;

            batch = 32;
            zbuf = (uint8_t *)malloc(batch * BLOCK_SIZE);
            if (zbuf) {
                memset(zbuf, 0, batch * BLOCK_SIZE);
                remain = inode_table_blocks;
                chunk = 0;
                while (remain > 0) {
                    uint32_t n;
                    n = remain > batch ? batch : remain;
                    zpos = lseek(fd, (off_t)(it_block + chunk) * BLOCK_SIZE, SEEK_SET);
                    if (zpos >= 0) {
                        vfs_write_fd(fd, zbuf, n * BLOCK_SIZE);
                    }
                    chunk += n;
                    remain -= n;
                }
                free(zbuf);
            } else {
                for (bit = 0; bit < inode_table_blocks; bit++) {
                    lf_write_block(fd, it_block + bit, block_buf);
                }
            }
        }
    }

    first_data = 1 + gdt_blocks + 1 + 1 + inode_table_blocks;

    it_block = 1 + gdt_blocks + 1 + 1;

    memset(block_buf, 0, BLOCK_SIZE);
    memset(inode, 0, sizeof(*inode));
    inode->i_mode = EXT4_S_IFDIR | 0755;
    inode->i_uid = 0;
    inode->i_size_lo = BLOCK_SIZE;
    inode->i_links_count = 2;
    inode->i_blocks_lo = BLOCK_SIZE / 512;
    inode->i_block[0] = first_data;
    inode->i_extra_isize = 28;

    memcpy(block_buf + (EXT4_ROOT_INO - 1) * INODE_SIZE, inode, sizeof(*inode));
    if (lf_write_block(fd, it_block, block_buf) < 0) {
        printf("lformat.ext4: FATAL: failed to write root inode to block %u\n", it_block);
        goto out;
    }

    lseek(fd, (off_t)it_block * BLOCK_SIZE, SEEK_SET);
    vfs_read_fd(fd, verify_buf, BLOCK_SIZE);
    ri = (lf_inode_t *)(verify_buf + (EXT4_ROOT_INO - 1) * INODE_SIZE);
    if (ri->i_mode == 0) {
        printf("lformat.ext4: WARNING: readback of root inode shows mode=0! Write may have failed.\n");
    } else {
        printf("lformat.ext4: root inode written ok (mode=0x%04x)\n", ri->i_mode);
    }

    memset(block_buf, 0, BLOCK_SIZE);
    dot = (lf_dirent_t *)block_buf;
    dot->inode = EXT4_ROOT_INO;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT4_FT_DIR;
    dot->name[0] = '.';

    dotdot = (lf_dirent_t *)(block_buf + 12);
    dotdot->inode = EXT4_ROOT_INO;
    dotdot->rec_len = 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT4_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    end = (lf_dirent_t *)(block_buf + 24);
    end->inode = 0;
    end->rec_len = (uint16_t)(BLOCK_SIZE - 24);
    end->name_len = 0;
    end->file_type = 0;

    lf_write_block(fd, first_data, block_buf);

    bb_block = 1 + gdt_blocks;
    memset(block_buf, 0, BLOCK_SIZE);
    lseek(fd, (off_t)bb_block * BLOCK_SIZE, SEEK_SET);
    vfs_read_fd(fd, block_buf, BLOCK_SIZE);
    byte_idx = first_data / 8;
    block_buf[byte_idx] |= (1 << (first_data & 7));
    lf_write_block(fd, bb_block, block_buf);

    sb->s_free_blocks_count_lo = free_blocks - 1;
    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf + EXT4_SB_OFFSET, sb, sizeof(*sb));
    lf_write_block(fd, 0, block_buf);

    {
        lf_gd_t gd0;
        memset(&gd0, 0, sizeof(gd0));
        lseek(fd, (off_t)1 * BLOCK_SIZE, SEEK_SET);
        vfs_read_fd(fd, &gd0, 32);
        gd0.bg_free_blocks_count_lo = (uint16_t)(gd0.bg_free_blocks_count_lo - 1);
        lseek(fd, (off_t)1 * BLOCK_SIZE, SEEK_SET);
        vfs_write_fd(fd, &gd0, 32);
    }

    fsync(fd);
    vfs_close_fd(fd);
    fd = -1;

    printf("Filesystem created successfully.\n");
    printf("  UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           uuid[0], uuid[1], uuid[2], uuid[3],
           uuid[4], uuid[5], uuid[6], uuid[7],
           uuid[8], uuid[9], uuid[10], uuid[11],
           uuid[12], uuid[13], uuid[14], uuid[15]);
    if (label[0])
        printf("  Volume label: %s\n", label);
    printf("  Total blocks: %u, Free blocks: %u\n", total_blocks, sb->s_free_blocks_count_lo);
    printf("  Total inodes: %u, Free inodes: %u\n", total_inodes, free_inodes);

    ret = 0;

out:
    if (fd >= 0) vfs_close_fd(fd);
    free(block_buf);
    free(verify_buf);
    free(sb);
    free(gd);
    free(inode);
    return ret;
}
