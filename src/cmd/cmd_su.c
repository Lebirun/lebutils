#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <crypt.h>
#include "cu.h"

static int su_authenticate(const struct passwd *pw) {
    struct spwd *sp;
    char *password, *hashed;
    int ok;

    sp = getspnam(pw->pw_name);
    if (!sp || !sp->sp_pwdp || sp->sp_pwdp[0] == '!' || sp->sp_pwdp[0] == '*') return -1;
    if (sp->sp_pwdp[0] == '\0') return 0;
    password = getpass("Password: ");
    if (!password) return -1;
    hashed = crypt(password, sp->sp_pwdp);
    ok = hashed && strcmp(hashed, sp->sp_pwdp) == 0;
    memset(password, 0, strlen(password));
    return ok ? 0 : -1;
}

int cmd_su(int argc, char **argv) {
    struct passwd *pw;
    const char *name, *cmd, *shell, *base;
    char *args[4];
    char *term;
    int login, i;

    name = "root";
    cmd = NULL;
    login = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: su [-] [-c COMMAND] [USER]");
            puts("Run a shell as USER (default: root).");
            puts("");
            puts("  -, -l, --login  start a login shell with a fresh environment");
            puts("  -c COMMAND      run COMMAND with the user's shell");
            return 0;
        }
        if (strcmp(argv[i], "-") == 0 || strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--login") == 0) {
            login = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "su: option requires an argument -- 'c'\n");
                return 1;
            }
            cmd = argv[i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "su: invalid option '%s'\n", argv[i]);
            return 1;
        } else {
            name = argv[i];
        }
    }

    pw = getpwnam(name);
    if (!pw) {
        fprintf(stderr, "su: user %s does not exist\n", name);
        return 1;
    }
    if (geteuid() != 0) {
        fprintf(stderr, "su: executable is not installed setuid root\n");
        return 1;
    }
    if (getuid() != 0 && su_authenticate(pw) < 0) {
        fprintf(stderr, "su: authentication failure\n");
        return 1;
    }
    if (initgroups(pw->pw_name, pw->pw_gid) < 0 || setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
        fprintf(stderr, "su: cannot set user or group: %s\n", strerror(errno));
        return 1;
    }

    shell = pw->pw_shell && *pw->pw_shell ? pw->pw_shell : "/bin/sh";
    if (login) {
        term = getenv("TERM");
        if (term) term = strdup(term);
        clearenv();
        if (term) {
            setenv("TERM", term, 1);
            free(term);
        }
        setenv("PATH", "/bin:/sbin", 1);
        if (chdir(pw->pw_dir) < 0) chdir("/");
    }
    setenv("HOME", pw->pw_dir, 1);
    setenv("SHELL", shell, 1);
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);

    base = strrchr(shell, '/');
    base = base ? base + 1 : shell;
    args[0] = NULL;
    if (login && asprintf(&args[0], "-%s", base) < 0) args[0] = NULL;
    if (!args[0]) args[0] = (char *)base;
    i = 1;
    if (cmd) {
        args[i++] = "-c";
        args[i++] = (char *)cmd;
    }
    args[i] = NULL;
    execv(shell, args);
    fprintf(stderr, "su: cannot run %s: %s\n", shell, strerror(errno));
    return errno == ENOENT ? 127 : 126;
}
