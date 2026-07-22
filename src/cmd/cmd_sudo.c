#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <crypt.h>
#include "cu.h"

static int sudo_user_allowed(const struct passwd *user)
{
    struct group *group;
    char **member;

    if (user->pw_uid == 0) return 1;
    group = getgrnam("sudo");
    if (!group) return 0;
    if (user->pw_gid == group->gr_gid) return 1;
    member = group->gr_mem;
    while (member && *member) {
        if (strcmp(*member, user->pw_name) == 0) return 1;
        member++;
    }
    return 0;
}

static int sudo_authenticate(const struct passwd *user)
{
    struct spwd *shadow;
    char *password;
    char *hashed;
    int attempt;

    shadow = getspnam(user->pw_name);
    if (!shadow || !shadow->sp_pwdp || shadow->sp_pwdp[0] == '\0' ||
        shadow->sp_pwdp[0] == '!' || shadow->sp_pwdp[0] == '*') {
        fprintf(stderr, "sudo: account has no usable password\n");
        return -1;
    }

    for (attempt = 0; attempt < 3; attempt++) {
        password = getpass("[sudo] password: ");
        if (!password) return -1;
        hashed = crypt(password, shadow->sp_pwdp);
        if (hashed && strcmp(hashed, shadow->sp_pwdp) == 0) {
            memset(password, 0, strlen(password));
            return 0;
        }
        memset(password, 0, strlen(password));
        fprintf(stderr, "Sorry, try again.\n");
    }
    return -1;
}

static void sudo_prepare_environment(void)
{
    unsetenv("IFS");
    unsetenv("ENV");
    unsetenv("BASH_ENV");
    unsetenv("LD_PRELOAD");
    unsetenv("LD_LIBRARY_PATH");
    setenv("PATH", "/sbin:/bin:/usr/sbin:/usr/bin", 1);
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
}

static int sudo_exec_command(char **argv)
{
    static const char *directories[] = {
        "/sbin", "/bin", "/usr/sbin", "/usr/bin", NULL
    };
    char path[256];
    int i;
    int length;

    if (strchr(argv[0], '/')) {
        execv(argv[0], argv);
        return -1;
    }
    for (i = 0; directories[i]; i++) {
        length = snprintf(path, sizeof(path), "%s/%s", directories[i], argv[0]);
        if (length > 0 && length < (int)sizeof(path)) execv(path, argv);
    }
    return -1;
}

int cmd_sudo(int argc, char **argv)
{
    struct passwd *user;
    uid_t invoking_uid;
    int command_index;

    command_index = 1;
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        puts("Usage: sudo COMMAND [ARG]...");
        puts("Execute a command as root.");
        puts("Members of the sudo group may authenticate with their password.");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        invoking_uid = getuid();
        user = getpwuid(invoking_uid);
        if (user && sudo_user_allowed(user)) {
            puts("User may run commands as root.");
            return 0;
        }
        puts("User may not run commands as root.");
        return 1;
    }
    if (argc <= command_index) {
        fprintf(stderr, "sudo: a command is required\n");
        return 1;
    }
    if (geteuid() != 0) {
        fprintf(stderr, "sudo: executable is not installed setuid root\n");
        return 1;
    }

    invoking_uid = getuid();
    user = getpwuid(invoking_uid);
    if (!user) {
        fprintf(stderr, "sudo: unknown user ID %u\n", (unsigned int)invoking_uid);
        return 1;
    }
    if (!sudo_user_allowed(user)) {
        fprintf(stderr, "sudo: %s is not in the sudo group\n", user->pw_name);
        return 1;
    }
    if (invoking_uid != 0 && sudo_authenticate(user) < 0) {
        fprintf(stderr, "sudo: authentication failed\n");
        return 1;
    }

    if (setgid(0) < 0) {
        fprintf(stderr, "sudo: cannot set root group\n");
        return 1;
    }
    if (setuid(0) < 0) {
        fprintf(stderr, "sudo: cannot set root user\n");
        return 1;
    }
    sudo_prepare_environment();
    sudo_exec_command(argv + command_index);
    fprintf(stderr, "sudo: command not found: %s\n", argv[command_index]);
    return 127;
}
