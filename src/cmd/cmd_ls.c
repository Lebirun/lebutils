#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include "cu.h"

#define O_RDONLY 0
#define O_DIRECTORY 0200000

typedef struct {
    char name[256];
    struct stat st;
    int has_stat;
} ls_entry_t;

static void ls_human(char *buf, unsigned long long size) {
    const char *units = "KMGTPE";
    double v = (double)size;
    int u = -1;
    if (size < 1024) {
        sprintf(buf, "%llu", size);
        return;
    }
    while (v >= 1024 && u < 5) {
        v /= 1024;
        u++;
    }
    if (v >= 100)
        sprintf(buf, "%.0f%c", v, units[u]);
    else if (v >= 10)
        sprintf(buf, "%.1f%c", v, units[u]);
    else
        sprintf(buf, "%.1f%c", v, units[u]);
}

static void ls_perms(char *buf, mode_t mode) {
    buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : S_ISCHR(mode) ? 'c' :
             S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');
    buf[10] = '\0';
}

static void ls_time(char *buf, size_t bufsz, time_t then, time_t now) {
    struct tm *tm;
    double diff;
    tm = localtime(&then);
    if (!tm) {
        snprintf(buf, bufsz, "-- --- ----");
        return;
    }
    diff = difftime(now, then);
    if (diff < 0) diff = -diff;
    if (diff < 15552000)
        strftime(buf, bufsz, "%b %e %H:%M", tm);
    else
        strftime(buf, bufsz, "%b %e  %Y", tm);
}

static const char *ls_owner(uid_t uid, char *buf, size_t bufsz) {
    struct passwd *pw;
    pw = getpwuid(uid);
    if (pw && pw->pw_name) return pw->pw_name;
    snprintf(buf, bufsz, "%u", (unsigned)uid);
    return buf;
}

static const char *ls_group(gid_t gid, char *buf, size_t bufsz) {
    struct group *gr;
    gr = getgrgid(gid);
    if (gr && gr->gr_name) return gr->gr_name;
    snprintf(buf, bufsz, "%u", (unsigned)gid);
    return buf;
}

static int ls_add(ls_entry_t **list, size_t *count, size_t *cap,
                  const char *dir, const char *name) {
    ls_entry_t *grown;
    ls_entry_t *e;
    char full[512];
    if (*count >= *cap) {
        size_t ncap = *cap ? *cap * 2 : 64;
        grown = (ls_entry_t *)realloc(*list, ncap * sizeof(ls_entry_t));
        if (!grown) return -1;
        *list = grown;
        *cap = ncap;
    }
    e = &(*list)[*count];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->has_stat = 0;
    if (dir && name[0] != '/' && snprintf(full, sizeof(full), "%s/%s", dir, name) > 0) {
        if (lstat(full, &e->st) == 0) e->has_stat = 1;
    } else if (stat(name, &e->st) == 0) {
        e->has_stat = 1;
    }
    (*count)++;
    return 0;
}

static void ls_sort(ls_entry_t *list, size_t count) {
    size_t i, j;
    ls_entry_t tmp;
    for (i = 1; i < count; i++) {
        tmp = list[i];
        j = i;
        while (j > 0 && strcmp(list[j - 1].name, tmp.name) > 0) {
            list[j] = list[j - 1];
            j--;
        }
        list[j] = tmp;
    }
}

int cmd_ls(int argc, char **argv) {
    int show_all, long_format, human, i;
    const char *arg, *p;
    char path[256];
    char full[512];
    int fd;
    uint64_t size, type;
    unsigned int entry_type, idx;
    char name[256];
    ls_entry_t *list;
    size_t count, cap, k;
    size_t maxlen, len, cols, col;
    char perms[11];
    char sizebuf[16];
    char timebuf[16];
    char ownbuf[32];
    char grpbuf[32];
    char linkbuf[256];
    ssize_t linklen;
    unsigned long long total;
    time_t now;

    show_all = 0;
    long_format = 0;
    human = 0;
    arg = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            puts("Usage: ls [-alh] [DIRECTORY]");
            puts("List directory contents.");
            puts("");
            puts("  -a   show all entries including hidden");
            puts("  -l   use long listing format");
            puts("  -h   human-readable sizes with -l");
            puts("  --help  display this help and exit");
            return 0;
        }
        if (argv[i][0] == '-') {
            for (p = argv[i] + 1; *p; p++) {
                if (*p == 'a') show_all = 1;
                else if (*p == 'l') long_format = 1;
                else if (*p == 'h') human = 1;
            }
        } else {
            arg = argv[i];
        }
    }

    if (arg && *arg) {
        if (cu_path_abs(arg, path, sizeof(path)) < 0) return 1;
    } else {
        if (!getcwd(path, sizeof(path))) return 1;
    }

    fd = vfs_open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        fd = vfs_open(path, O_RDONLY);
    }
    if (fd < 0) {
        printf("ls: cannot access '%s'\n", path);
        return 1;
    }

    size = 0;
    type = 0;
    if (vfs_stat(fd, &size, &type) == 0) {
        if ((type & 0x07) != 0x02 && (type & 0x08) == 0) {
            vfs_close_fd(fd);
            puts(path);
            return 0;
        }
    }

    list = NULL;
    count = 0;
    cap = 0;
    if (show_all) {
        ls_add(&list, &count, &cap, path, ".");
        ls_add(&list, &count, &cap, path, "..");
    }
    entry_type = 0;
    for (idx = 0; idx < 100000; idx++) {
        if (vfs_readdir(fd, name, &entry_type, idx) < 0) break;
        if (!show_all && name[0] == '.') continue;
        if (ls_add(&list, &count, &cap, path, name) < 0) break;
    }
    vfs_close_fd(fd);
    ls_sort(list, count);

    if (long_format) {
        total = 0;
        for (k = 0; k < count; k++) {
            if (list[k].has_stat) total += (unsigned long long)list[k].st.st_blocks;
        }
        if (human) {
            ls_human(sizebuf, total * 512);
            printf("total %s\n", sizebuf);
        } else {
            printf("total %llu\n", total / 2);
        }
        now = time(NULL);
        for (k = 0; k < count; k++) {
            if (!list[k].has_stat) {
                printf("?????????? ? ? ? ? %s\n", list[k].name);
                continue;
            }
            ls_perms(perms, list[k].st.st_mode);
            ls_time(timebuf, sizeof(timebuf), (time_t)list[k].st.st_mtime, now);
            if (human)
                ls_human(sizebuf, (unsigned long long)list[k].st.st_size);
            else
                snprintf(sizebuf, sizeof(sizebuf), "%lld", (long long)list[k].st.st_size);
            printf("%s %3lu %s %s %8s %s %s", perms,
                   (unsigned long)list[k].st.st_nlink,
                   ls_owner(list[k].st.st_uid, ownbuf, sizeof(ownbuf)),
                   ls_group(list[k].st.st_gid, grpbuf, sizeof(grpbuf)),
                   sizebuf, timebuf, list[k].name);
            if (S_ISLNK(list[k].st.st_mode)) {
                if (snprintf(full, sizeof(full), "%s/%s", path, list[k].name) > 0) {
                    linklen = readlink(full, linkbuf, sizeof(linkbuf) - 1);
                    if (linklen > 0) {
                        linkbuf[linklen] = '\0';
                        printf(" -> %s", linkbuf);
                    }
                }
            }
            putchar('\n');
        }
    } else {
        maxlen = 0;
        for (k = 0; k < count; k++) {
            len = strlen(list[k].name);
            if (len > maxlen) maxlen = len;
        }
        maxlen += 2;
        cols = maxlen > 0 ? 80 / maxlen : 1;
        if (cols < 1) cols = 1;
        col = 0;
        for (k = 0; k < count; k++) {
            fputs(list[k].name, stdout);
            len = strlen(list[k].name);
            while (len < maxlen) {
                putchar(' ');
                len++;
            }
            col++;
            if (col >= cols) {
                putchar('\n');
                col = 0;
            }
        }
        if (col > 0) putchar('\n');
    }

    free(list);
    return 0;
}
