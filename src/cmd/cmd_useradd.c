#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "cu.h"

#define PASSWD_FILE  "/etc/passwd"
#define SHADOW_FILE  "/etc/shadow"
#define GROUP_FILE   "/etc/group"
#define MAX_LINE     512
#define MAX_FIELD    256
#define SALT_LEN     16

static int ua_parse_field(const char *line, int field_num, char *out, int out_max)
{
    int i;
    int f;
    int start;
    int len;

    f = 0;
    start = 0;
    for (i = 0; line[i]; i++) {
        if (line[i] == ':') {
            if (f == field_num) {
                len = i - start;
                if (len >= out_max) len = out_max - 1;
                memcpy(out, line + start, len);
                out[len] = '\0';
                return 0;
            }
            f++;
            start = i + 1;
        }
    }
    if (f == field_num) {
        len = i - start;
        if (len >= out_max) len = out_max - 1;
        memcpy(out, line + start, len);
        out[len] = '\0';
        return 0;
    }
    return -1;
}

static int ua_user_exists(const char *username)
{
    int fd;
    char buf[MAX_LINE];
    int pos;
    int r;
    char c;
    char field[MAX_FIELD];

    fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return 0;

    pos = 0;
    for (;;) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            if (pos > 0) {
                buf[pos] = '\0';
                if (ua_parse_field(buf, 0, field, sizeof(field)) == 0) {
                    if (strcmp(field, username) == 0) {
                        close(fd);
                        return 1;
                    }
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (ua_parse_field(buf, 0, field, sizeof(field)) == 0) {
                if (strcmp(field, username) == 0) {
                    close(fd);
                    return 1;
                }
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return 0;
}

static int ua_find_next_uid(void)
{
    int fd;
    char buf[MAX_LINE];
    int pos;
    int r;
    char c;
    char field[MAX_FIELD];
    int uid;
    int max_uid;

    max_uid = 999;

    fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return 1000;

    pos = 0;
    for (;;) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            if (pos > 0) {
                buf[pos] = '\0';
                if (ua_parse_field(buf, 2, field, sizeof(field)) == 0) {
                    uid = atoi(field);
                    if (uid >= 1000 && uid < 60000 && uid > max_uid)
                        max_uid = uid;
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (ua_parse_field(buf, 2, field, sizeof(field)) == 0) {
                uid = atoi(field);
                if (uid >= 1000 && uid < 60000 && uid > max_uid)
                    max_uid = uid;
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return max_uid + 1;
}

static int ua_append_file(const char *path, const char *line)
{
    int fd;
    int len;

    fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        vfs_create(path, 0644);
        fd = open(path, O_WRONLY | O_APPEND);
        if (fd < 0) return -1;
    }

    len = strlen(line);
    if (write(fd, line, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void ua_generate_salt(char *salt, int len)
{
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789./";
    unsigned int ticks;
    unsigned int seed;
    int i;

    ticks = getticks();
    seed = ticks ^ (ticks >> 16) ^ (unsigned int)getpid();

    for (i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        salt[i] = charset[(seed >> 16) % (sizeof(charset) - 1)];
    }
    salt[len] = '\0';
}

int cmd_useradd(int argc, char **argv)
{
    char *username;
    char *home_dir;
    char *shell;
    char *password;
    int uid;
    int gid;
    int i;
    int custom_uid;
    int custom_gid;
    char passwd_line[MAX_LINE];
    char shadow_line[MAX_LINE];
    char group_line[MAX_LINE];
    char salt[4 + SALT_LEN + 1];
    char *hashed;
    int create_home;

    username = NULL;
    home_dir = NULL;
    shell = "/bin/sh";
    password = NULL;
    custom_uid = -1;
    custom_gid = -1;
    create_home = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: useradd [OPTIONS] USERNAME");
            puts("Create a new user account.");
            puts("");
            puts("  -d DIR     home directory (default: /home/USERNAME)");
            puts("  -s SHELL   login shell (default: /bin/sh)");
            puts("  -u UID     user ID");
            puts("  -g GID     group ID");
            puts("  -p PASS    password (will be hashed)");
            puts("  -M         do not create home directory");
            puts("  -h, --help display this help and exit");
            return 0;
        }
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            home_dir = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            shell = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            custom_uid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            custom_gid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            password = argv[++i];
        } else if (strcmp(argv[i], "-M") == 0) {
            create_home = 0;
        } else if (argv[i][0] != '-') {
            username = argv[i];
        } else {
            fprintf(stderr, "useradd: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!username) {
        fprintf(stderr, "useradd: missing username\n");
        return 1;
    }

    if (getuid() != 0) {
        fprintf(stderr, "useradd: permission denied (must be root)\n");
        return 1;
    }

    if (ua_user_exists(username)) {
        fprintf(stderr, "useradd: user '%s' already exists\n", username);
        return 1;
    }

    if (strlen(username) > 32) {
        fprintf(stderr, "useradd: username too long (max 32)\n");
        return 1;
    }

    uid = (custom_uid >= 0) ? custom_uid : ua_find_next_uid();
    gid = (custom_gid >= 0) ? custom_gid : uid;

    if (!home_dir) {
        static char home_buf[MAX_FIELD];
        snprintf(home_buf, sizeof(home_buf), "/home/%s", username);
        home_dir = home_buf;
    }

    snprintf(passwd_line, sizeof(passwd_line),
        "%s:x:%d:%d:%s:%s:%s\n",
        username, uid, gid, username, home_dir, shell);

    if (password) {
        memcpy(salt, "$5$", 3);
        ua_generate_salt(salt + 3, SALT_LEN);
        salt[3 + SALT_LEN] = '$';
        salt[3 + SALT_LEN + 1] = '\0';
        hashed = crypt(password, salt);
        if (!hashed) {
            fprintf(stderr, "useradd: failed to hash password\n");
            return 1;
        }
        snprintf(shadow_line, sizeof(shadow_line),
            "%s:%s:0:0:99999:7:::\n", username, hashed);
    } else {
        snprintf(shadow_line, sizeof(shadow_line),
            "%s:!:0:0:99999:7:::\n", username);
    }

    snprintf(group_line, sizeof(group_line),
        "%s:x:%d:\n", username, gid);

    if (ua_append_file(PASSWD_FILE, passwd_line) < 0) {
        fprintf(stderr, "useradd: failed to update %s\n", PASSWD_FILE);
        return 1;
    }

    if (ua_append_file(SHADOW_FILE, shadow_line) < 0) {
        fprintf(stderr, "useradd: failed to update %s\n", SHADOW_FILE);
        return 1;
    }

    if (ua_append_file(GROUP_FILE, group_line) < 0) {
        fprintf(stderr, "useradd: warning: failed to update %s\n", GROUP_FILE);
    }

    if (create_home) {
        if (vfs_mkdir(home_dir, 0755) < 0) {
            fprintf(stderr, "useradd: warning: could not create home directory '%s'\n", home_dir);
        }
    }

    printf("useradd: user '%s' created (uid=%d, gid=%d)\n", username, uid, gid);
    return 0;
}
