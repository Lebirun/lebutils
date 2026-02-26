#include <string.h>
#include <stdio.h>
#include "cu.h"

struct cu_cmd {
    const char *name;
    int (*fn)(int, char **);
};

static const struct cu_cmd cu_cmds[] = {
#ifdef CONFIG_CMD_ECHO
    {"echo", cmd_echo},
#endif
#ifdef CONFIG_CMD_PWD
    {"pwd", cmd_pwd},
#endif
#ifdef CONFIG_CMD_LS
    {"ls", cmd_ls},
#endif
#ifdef CONFIG_CMD_CAT
    {"cat", cmd_cat},
#endif
#ifdef CONFIG_CMD_TOUCH
    {"touch", cmd_touch},
#endif
#ifdef CONFIG_CMD_MKDIR
    {"mkdir", cmd_mkdir},
#endif
#ifdef CONFIG_CMD_RM
    {"rm", cmd_rm},
#endif
#ifdef CONFIG_CMD_WRITE
    {"write", cmd_write},
#endif
#ifdef CONFIG_CMD_TICKS
    {"ticks", cmd_ticks},
#endif
#ifdef CONFIG_CMD_CRES
    {"cres", cmd_cres},
#endif
#ifdef CONFIG_CMD_FREE
    {"free", cmd_free},
#endif
#ifdef CONFIG_CMD_DF
    {"df", cmd_df},
#endif
#ifdef CONFIG_CMD_UNAME
    {"uname", cmd_uname},
#endif
#ifdef CONFIG_CMD_DATE
    {"date", cmd_date},
#endif
#ifdef CONFIG_CMD_LNETURL
    {"lneturl", cmd_lneturl},
#endif
#ifdef CONFIG_CMD_DHCP
    {"dhcp", cmd_dhcp},
#endif
    {NULL, NULL}
};

#define CU_CMD_COUNT (sizeof(cu_cmds) / sizeof(cu_cmds[0]) - 1)

static const struct cu_cmd *cu_find(const char *name) {
    for (unsigned int i = 0; i < CU_CMD_COUNT; i++) {
        if (cu_cmds[i].name && strcmp(cu_cmds[i].name, name) == 0) {
            return &cu_cmds[i];
        }
    }
    return 0;
}

void cu_print_commands(void) {
    if (CU_CMD_COUNT == 0) {
        puts("(no commands enabled)");
        return;
    }
    int first = 1;
    for (unsigned int i = 0; i < CU_CMD_COUNT; i++) {
        if (cu_cmds[i].name) {
            if (!first) putchar(' ');
            printf("%s", cu_cmds[i].name);
            first = 0;
        }
    }
    putchar('\n');
}

int cu_dispatch_as(const char *applet, int argc, char **argv) {
    if (!applet || !*applet) return 1;
    const struct cu_cmd *cmd = cu_find(applet);
    if (!cmd) {
        fprintf(stderr, "%s: unknown command\n", applet);
        return 127;
    }
    return cmd->fn(argc, argv);
}

int cu_dispatch(int argc, char **argv) {
    const char *applet = cu_basename((argc > 0 && argv) ? argv[0] : "");
    if (!applet || !*applet) return 1;
    return cu_dispatch_as(applet, argc, argv);
}
