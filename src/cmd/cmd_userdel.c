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

static int ud_parse_field(const char *line, int field_num, char *out, int out_max)
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

static int ud_remove_line_from_file(const char *filepath, const char *tmp_path,
                                     const char *username)
{
    int fd_in;
    int fd_out;
    char buf[MAX_LINE];
    char field[MAX_FIELD];
    int pos;
    int r;
    char c;
    int found;

    fd_in = open(filepath, O_RDONLY);
    if (fd_in < 0) return -1;

    vfs_create(tmp_path, 0600);
    fd_out = open(tmp_path, O_WRONLY | O_TRUNC);
    if (fd_out < 0) {
        close(fd_in);
        return -1;
    }

    found = 0;
    pos = 0;
    for (;;) {
        r = read(fd_in, &c, 1);
        if (r <= 0) {
            if (pos > 0) {
                buf[pos] = '\0';
                if (ud_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                    strcmp(field, username) == 0) {
                    found = 1;
                } else {
                    write(fd_out, buf, pos);
                    write(fd_out, "\n", 1);
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (ud_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                strcmp(field, username) == 0) {
                found = 1;
            } else {
                write(fd_out, buf, strlen(buf));
                write(fd_out, "\n", 1);
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }

    close(fd_in);
    close(fd_out);

    if (!found) {
        unlink(tmp_path);
        return 0;
    }

    unlink(filepath);
    rename(tmp_path, filepath);
    return 1;
}

static int ud_get_home_dir(const char *username, char *home, int home_max)
{
    int fd;
    char buf[MAX_LINE];
    int pos;
    int r;
    char c;
    char field[MAX_FIELD];

    fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return -1;

    pos = 0;
    for (;;) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            if (pos > 0) {
                buf[pos] = '\0';
                if (ud_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                    strcmp(field, username) == 0) {
                    ud_parse_field(buf, 5, home, home_max);
                    close(fd);
                    return 0;
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (ud_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                strcmp(field, username) == 0) {
                ud_parse_field(buf, 5, home, home_max);
                close(fd);
                return 0;
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return -1;
}

static int ud_user_exists(const char *username)
{
    char home[MAX_FIELD];
    return (ud_get_home_dir(username, home, sizeof(home)) == 0) ? 1 : 0;
}

static void ud_recursive_remove(const char *path)
{
    int fd;
    char name[256];
    unsigned int type;
    unsigned int idx;
    char child_path[512];

    fd = vfs_open(path, 0);
    if (fd < 0) {
        vfs_unlink(path);
        return;
    }

    idx = 0;
    while (vfs_readdir(fd, name, &type, idx) == 0) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            idx++;
            continue;
        }
        snprintf(child_path, sizeof(child_path), "%s/%s", path, name);
        if (type == 2) {
            ud_recursive_remove(child_path);
        } else {
            vfs_unlink(child_path);
        }
        idx++;
    }
    vfs_close_fd(fd);
    vfs_unlink(path);
}

int cmd_userdel(int argc, char **argv)
{
    char *username;
    int remove_home;
    int i;
    char home[MAX_FIELD];
    int ret;

    username = NULL;
    remove_home = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: userdel [OPTIONS] USERNAME");
            puts("Delete a user account.");
            puts("");
            puts("  -r         remove home directory");
            puts("  -h, --help display this help and exit");
            return 0;
        }
        if (strcmp(argv[i], "-r") == 0) {
            remove_home = 1;
        } else if (argv[i][0] != '-') {
            username = argv[i];
        } else {
            fprintf(stderr, "userdel: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!username) {
        fprintf(stderr, "userdel: missing username\n");
        return 1;
    }

    if (getuid() != 0) {
        fprintf(stderr, "userdel: permission denied (must be root)\n");
        return 1;
    }

    if (!ud_user_exists(username)) {
        fprintf(stderr, "userdel: user '%s' does not exist\n", username);
        return 1;
    }

    if (strcmp(username, "root") == 0) {
        fprintf(stderr, "userdel: cannot delete root user\n");
        return 1;
    }

    if (remove_home) {
        if (ud_get_home_dir(username, home, sizeof(home)) == 0 && home[0] != '\0') {
            if (strcmp(home, "/") != 0 && strcmp(home, "/root") != 0) {
                ud_recursive_remove(home);
            }
        }
    }

    ret = ud_remove_line_from_file(PASSWD_FILE, "/etc/passwd.tmp", username);
    if (ret < 0) {
        fprintf(stderr, "userdel: failed to update %s\n", PASSWD_FILE);
        return 1;
    }

    ud_remove_line_from_file(SHADOW_FILE, "/etc/shadow.tmp", username);
    ud_remove_line_from_file(GROUP_FILE, "/etc/group.tmp", username);

    printf("userdel: user '%s' deleted\n", username);
    return 0;
}
