#ifndef CU_H
#define CU_H

int cu_main(int argc, char **argv);
int cu_dispatch(int argc, char **argv);
int cu_dispatch_as(const char *applet, int argc, char **argv);

int cmd_echo(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_ls(int argc, char **argv);
int cmd_cat(int argc, char **argv);
int cmd_touch(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_write(int argc, char **argv);
int cmd_ticks(int argc, char **argv);

const char *cu_basename(const char *path);
int cu_path_abs(const char *in, char *out, unsigned int outsz);

#endif
