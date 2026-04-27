#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cu.h"

#define LKE_DIR      "/lib/lke"
#define MODINFO_BUFSZ 65536

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ET_REL  1
#define SHT_PROGBITS 1
#define SHT_STRTAB   3

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

typedef struct {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} Elf64_Ehdr_mi;

typedef struct {
    u32 sh_name;
    u32 sh_type;
    u64 sh_flags;
    u64 sh_addr;
    u64 sh_offset;
    u64 sh_size;
    u32 sh_link;
    u32 sh_info;
    u64 sh_addralign;
    u64 sh_entsize;
} Elf64_Shdr_mi;

static int modinfo_read_file(const char *path, u8 **out_data, u64 *out_size) {
    int fd;
    int rd;
    u8 *data;
    u64 cap;
    u64 len;

    fd = vfs_open(path, 0);
    if (fd < 0) return -1;

    cap = MODINFO_BUFSZ;
    data = (u8 *)malloc(cap);
    if (!data) { vfs_close_fd(fd); return -2; }

    len = 0;
    while (1) {
        if (len + 4096 > cap) {
            u8 *tmp;
            cap *= 2;
            tmp = (u8 *)realloc(data, cap);
            if (!tmp) { free(data); vfs_close_fd(fd); return -2; }
            data = tmp;
        }
        rd = vfs_read_fd(fd, (char *)(data + len), 4096);
        if (rd <= 0) break;
        len += (u64)rd;
    }
    vfs_close_fd(fd);

    *out_data = data;
    *out_size = len;
    return 0;
}

static void modinfo_print_field(const char *tag, const char *key, const u8 *sec, u64 sec_size) {
    u64 pos;
    u64 slen;
    const char *entry;
    u64 klen;

    klen = strlen(key);
    pos = 0;
    while (pos < sec_size) {
        entry = (const char *)(sec + pos);
        slen = strlen(entry);
        if (slen == 0) { pos++; continue; }
        if (strncmp(entry, key, klen) == 0 && entry[klen] == '=') {
            printf("%-12s%s\n", tag, entry + klen + 1);
        }
        pos += slen + 1;
    }
}

static int modinfo_run(const char *path) {
    u8 *data;
    u64 data_size;
    const Elf64_Ehdr_mi *ehdr;
    const Elf64_Shdr_mi *shdr;
    const Elf64_Shdr_mi *shstrtab_sh;
    const char *shstrtab;
    u64 i;
    const Elf64_Shdr_mi *sh;
    const char *sec_name;
    const u8 *info_sec;
    u64 info_sec_size;

    if (modinfo_read_file(path, &data, &data_size) != 0) {
        fprintf(stderr, "modinfo: cannot read '%s'\n", path);
        return 1;
    }

    if (data_size < sizeof(Elf64_Ehdr_mi)) {
        fprintf(stderr, "modinfo: '%s' is not a valid module\n", path);
        free(data);
        return 1;
    }

    ehdr = (const Elf64_Ehdr_mi *)data;
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3 ||
        ehdr->e_type != ET_REL) {
        fprintf(stderr, "modinfo: '%s' is not a relocatable ELF module\n", path);
        free(data);
        return 1;
    }

    if (ehdr->e_shoff + (u64)ehdr->e_shnum * sizeof(Elf64_Shdr_mi) > data_size) {
        fprintf(stderr, "modinfo: '%s' is truncated\n", path);
        free(data);
        return 1;
    }

    shdr = (const Elf64_Shdr_mi *)(data + ehdr->e_shoff);
    shstrtab_sh = &shdr[ehdr->e_shstrndx];
    shstrtab = (const char *)(data + shstrtab_sh->sh_offset);

    info_sec = NULL;
    info_sec_size = 0;
    for (i = 0; i < ehdr->e_shnum; i++) {
        sh = &shdr[i];
        sec_name = shstrtab + sh->sh_name;
        if (sh->sh_type == SHT_PROGBITS && strcmp(sec_name, ".lke_info") == 0) {
            info_sec = data + sh->sh_offset;
            info_sec_size = sh->sh_size;
            break;
        }
    }

    printf("filename:   %s\n", path);

    if (!info_sec || info_sec_size == 0) {
        printf("(no module metadata found)\n");
        free(data);
        return 0;
    }

    modinfo_print_field("name:",        "name",        info_sec, info_sec_size);
    modinfo_print_field("description:", "description", info_sec, info_sec_size);
    modinfo_print_field("license:",     "license",     info_sec, info_sec_size);
    modinfo_print_field("author:",      "author",      info_sec, info_sec_size);
    modinfo_print_field("version:",     "version",     info_sec, info_sec_size);

    free(data);
    return 0;
}

static void modinfo_try_dir(const char *name) {
    char path[256];
    int n;

    n = (int)strlen(LKE_DIR) + 1 + (int)strlen(name) + 4 + 1;
    if (n > (int)sizeof(path)) {
        fprintf(stderr, "modinfo: name too long\n");
        return;
    }

    snprintf(path, sizeof(path), "%s/%s.lke", LKE_DIR, name);
    modinfo_run(path);
}

int cmd_modinfo(int argc, char **argv) {
    char path[256];
    int i;
    int is_path;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        puts("Usage: modinfo <module_name|path> [...]");
        puts("Show metadata embedded in a Lebirun Kernel Extension module.");
        puts("If given a plain name (no '/'), looks in " LKE_DIR ".");
        return argc < 2 ? 1 : 0;
    }

    for (i = 1; i < argc; i++) {
        is_path = 0;
        if (argv[i][0] == '/' || argv[i][0] == '.') is_path = 1;

        if (is_path) {
            if (cu_path_abs(argv[i], path, sizeof(path)) != 0) {
                fprintf(stderr, "modinfo: path too long: %s\n", argv[i]);
                continue;
            }
            modinfo_run(path);
        } else {
            modinfo_try_dir(argv[i]);
        }
    }

    return 0;
}
