#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include "cu.h"

#define PASSWD_FILE  "/etc/passwd"
#define SHADOW_FILE  "/etc/shadow"
#define SHADOW_TMP   "/etc/shadow.tmp"
#define MAX_LINE     512
#define MAX_FIELD    256
#define MAX_PASSWORD 256
#define SALT_LEN     16

static int pw_read_password(const char *prompt, char *buf, int max)
{
    struct termios old_t;
    struct termios new_t;
    int i;
    char c;
    int r;

    write(1, prompt, strlen(prompt));

    tcgetattr(0, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tcsetattr(0, TCSANOW, &new_t);

    i = 0;
    while (i < max - 1) {
        r = read(0, &c, 1);
        if (r <= 0) break;
        if (c == '\n' || c == '\r') break;
        if (c == 127 || c == '\b') {
            if (i > 0) i--;
            continue;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';

    tcsetattr(0, TCSANOW, &old_t);
    write(1, "\n", 1);
    return i;
}

static int pw_parse_field(const char *line, int field_num, char *out, int out_max)
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

static int pw_user_exists(const char *username)
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
                if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
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
            if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
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

static int pw_lookup_name_by_uid(int uid, char *out, int out_max)
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
                if (pw_parse_field(buf, 2, field, sizeof(field)) == 0) {
                    if (atoi(field) == uid) {
                        if (pw_parse_field(buf, 0, out, out_max) == 0) {
                            close(fd);
                            return 0;
                        }
                    }
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (pw_parse_field(buf, 2, field, sizeof(field)) == 0) {
                if (atoi(field) == uid) {
                    if (pw_parse_field(buf, 0, out, out_max) == 0) {
                        close(fd);
                        return 0;
                    }
                }
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return -1;
}

static int pw_lookup_uid_by_name(const char *username)
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
                if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
                    if (strcmp(field, username) == 0) {
                        if (pw_parse_field(buf, 2, field, sizeof(field)) == 0) {
                            close(fd);
                            return atoi(field);
                        }
                    }
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
                if (strcmp(field, username) == 0) {
                    if (pw_parse_field(buf, 2, field, sizeof(field)) == 0) {
                        close(fd);
                        return atoi(field);
                    }
                }
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return -1;
}

static void pw_generate_salt(char *salt, int len)
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

static int pw_lookup_shadow_hash(const char *username, char *hash_out, int hash_max)
{
    int fd;
    char buf[MAX_LINE];
    int pos;
    int r;
    char c;
    char field[MAX_FIELD];

    fd = open(SHADOW_FILE, O_RDONLY);
    if (fd < 0) return -1;

    pos = 0;
    for (;;) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            if (pos > 0) {
                buf[pos] = '\0';
                if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
                    if (strcmp(field, username) == 0) {
                        pw_parse_field(buf, 1, hash_out, hash_max);
                        close(fd);
                        return 0;
                    }
                }
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;
            if (pw_parse_field(buf, 0, field, sizeof(field)) == 0) {
                if (strcmp(field, username) == 0) {
                    pw_parse_field(buf, 1, hash_out, hash_max);
                    close(fd);
                    return 0;
                }
            }
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd);
    return -1;
}

static int pw_update_shadow(const char *username, const char *new_hash)
{
    int fd_in;
    int fd_out;
    char buf[MAX_LINE];
    char field[MAX_FIELD];
    char lines[16][MAX_LINE];
    int line_count;
    int pos;
    int r;
    char c;
    char newline[MAX_LINE];
    int nlen;
    int found;
    int i;

    fd_in = open(SHADOW_FILE, O_RDONLY);
    if (fd_in < 0) return -1;

    line_count = 0;
    found = 0;
    pos = 0;
    for (;;) {
        r = read(fd_in, &c, 1);
        if (r <= 0) {
            if (pos > 0 && line_count < 16) {
                buf[pos] = '\0';
                if (pw_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                    strcmp(field, username) == 0) {
                    nlen = snprintf(newline, sizeof(newline),
                        "%s:%s:0:0:99999:7:::", username, new_hash);
                    memcpy(lines[line_count], newline, nlen + 1);
                    found = 1;
                } else {
                    memcpy(lines[line_count], buf, pos + 1);
                }
                line_count++;
            }
            break;
        }
        if (c == '\n') {
            buf[pos] = '\0';
            if (line_count < 16) {
                if (pw_parse_field(buf, 0, field, sizeof(field)) == 0 &&
                    strcmp(field, username) == 0) {
                    nlen = snprintf(newline, sizeof(newline),
                        "%s:%s:0:0:99999:7:::", username, new_hash);
                    memcpy(lines[line_count], newline, nlen + 1);
                    found = 1;
                } else {
                    memcpy(lines[line_count], buf, pos + 1);
                }
                line_count++;
            }
            pos = 0;
        } else {
            if (pos < MAX_LINE - 1) buf[pos++] = c;
        }
    }
    close(fd_in);

    if (!found) return -1;

    fd_out = open(SHADOW_FILE, O_WRONLY | O_TRUNC);
    if (fd_out < 0) return -1;

    for (i = 0; i < line_count; i++) {
        nlen = strlen(lines[i]);
        write(fd_out, lines[i], nlen);
        write(fd_out, "\n", 1);
    }
    close(fd_out);
    return 0;
}

int cmd_passwd(int argc, char **argv)
{
    char *target_user;
    int my_uid;
    char current_pw[MAX_PASSWORD];
    char new_pw[MAX_PASSWORD];
    char confirm_pw[MAX_PASSWORD];
    char stored_hash[MAX_FIELD];
    char salt[4 + SALT_LEN + 1];
    char *hashed;
    char my_name[MAX_FIELD];
    int target_uid;

    target_user = NULL;
    my_uid = getuid();

    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            puts("Usage: passwd [USERNAME]");
            puts("Change user password.");
            puts("Only root can change other users' passwords.");
            return 0;
        }
        target_user = argv[1];
    }

    if (!target_user) {
        if (pw_lookup_name_by_uid(my_uid, my_name, sizeof(my_name)) == 0) {
            target_user = my_name;
        } else {
            target_user = "root";
        }
    }

    if (my_uid != 0 && target_user) {
        target_uid = pw_lookup_uid_by_name(target_user);
        if (target_uid < 0 || target_uid != my_uid) {
            fprintf(stderr, "passwd: only root can change other users' passwords\n");
            return 1;
        }
    }

    if (!pw_user_exists(target_user)) {
        fprintf(stderr, "passwd: user '%s' does not exist\n", target_user);
        return 1;
    }

    if (my_uid != 0) {
        pw_read_password("Current password: ", current_pw, sizeof(current_pw));
        if (pw_lookup_shadow_hash(target_user, stored_hash, sizeof(stored_hash)) < 0) {
            fprintf(stderr, "passwd: cannot read shadow entry\n");
            memset(current_pw, 0, sizeof(current_pw));
            return 1;
        }
        hashed = crypt(current_pw, stored_hash);
        if (!hashed || strcmp(hashed, stored_hash) != 0) {
            fprintf(stderr, "passwd: authentication failure\n");
            memset(current_pw, 0, sizeof(current_pw));
            memset(stored_hash, 0, sizeof(stored_hash));
            return 1;
        }
        memset(current_pw, 0, sizeof(current_pw));
        memset(stored_hash, 0, sizeof(stored_hash));
    }

    pw_read_password("New password: ", new_pw, sizeof(new_pw));
    pw_read_password("Retype new password: ", confirm_pw, sizeof(confirm_pw));

    if (strcmp(new_pw, confirm_pw) != 0) {
        fprintf(stderr, "passwd: passwords do not match\n");
        memset(new_pw, 0, sizeof(new_pw));
        memset(confirm_pw, 0, sizeof(confirm_pw));
        return 1;
    }
    memset(confirm_pw, 0, sizeof(confirm_pw));

    if (strlen(new_pw) == 0) {
        fprintf(stderr, "passwd: password cannot be empty\n");
        memset(new_pw, 0, sizeof(new_pw));
        return 1;
    }

    memcpy(salt, "$5$", 3);
    pw_generate_salt(salt + 3, SALT_LEN);
    salt[3 + SALT_LEN] = '$';
    salt[3 + SALT_LEN + 1] = '\0';

    hashed = crypt(new_pw, salt);
    memset(new_pw, 0, sizeof(new_pw));

    if (!hashed) {
        fprintf(stderr, "passwd: failed to hash password\n");
        return 1;
    }

    if (pw_update_shadow(target_user, hashed) < 0) {
        fprintf(stderr, "passwd: failed to update shadow file\n");
        return 1;
    }

    printf("passwd: password updated successfully\n");
    return 0;
}
