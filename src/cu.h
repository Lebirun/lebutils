#ifndef CU_H
#define CU_H

int cu_main(int argc, char **argv);
int cu_dispatch(int argc, char **argv);
int cu_dispatch_as(const char *applet, int argc, char **argv);

void cu_print_commands(void);

#ifdef CONFIG_CMD_ECHO
int cmd_echo(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_PWD
int cmd_pwd(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LS
int cmd_ls(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_CAT
int cmd_cat(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_TOUCH
int cmd_touch(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_MKDIR
int cmd_mkdir(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_RM
int cmd_rm(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_WRITE
int cmd_write(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_TICKS
int cmd_ticks(int argc, char **argv);
#endif

const char *cu_basename(const char *path);
int cu_path_abs(const char *in, char *out, unsigned int outsz);

#endif
