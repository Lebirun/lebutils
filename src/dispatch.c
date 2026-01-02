#include <string.h>
#include <stdio.h>
#include "cu.h"

struct cu_cmd {
    const char *name;
    int (*fn)(int, char **);
};

static const struct cu_cmd cu_cmds[] = {
    {"echo", cmd_echo},
    {"pwd", cmd_pwd},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"touch", cmd_touch},
    {"mkdir", cmd_mkdir},
    {"rm", cmd_rm},
    {"write", cmd_write},
    {"ticks", cmd_ticks},
};

#define CU_CMD_COUNT (sizeof(cu_cmds) / sizeof(cu_cmds[0]))

static unsigned int cu_hash(const char *s) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h;
}

static const struct cu_cmd *cu_find(const char *name) {
    unsigned int h = cu_hash(name);
    unsigned int idx = h % CU_CMD_COUNT;
    
    for (unsigned int i = 0; i < CU_CMD_COUNT; i++) {
        unsigned int probe = (idx + i) % CU_CMD_COUNT;
        if (strcmp(cu_cmds[probe].name, name) == 0) return &cu_cmds[probe];
    }
    return 0;
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
