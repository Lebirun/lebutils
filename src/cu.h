#ifndef CU_H
#define CU_H

unsigned int getticks(void);

int vfs_open(const char *path, int flags);
int vfs_close_fd(int fd);
int vfs_read_fd(int fd, void *buf, unsigned int count);
int vfs_write_fd(int fd, const void *buf, unsigned int count);
int vfs_readdir(int fd, char *name, unsigned int *type, unsigned int index);
int vfs_stat(int fd, unsigned int *size, unsigned int *type);
int vfs_create(const char *path, unsigned int perms);
int vfs_mkdir(const char *path, unsigned int perms);
int vfs_unlink(const char *path);
int vfs_mounts(void);

int fb_set_mode(unsigned int width, unsigned int height, unsigned int refresh_rate);
int fb_getinfo(unsigned int *width, unsigned int *height, unsigned int *bpp,
    unsigned int *font_height, unsigned int *rows, unsigned int *cursor_row,
    unsigned int *font_width, unsigned int *cols);
int fb_getcaps(unsigned int *words, unsigned int count);

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
#ifdef CONFIG_CMD_CRES
int cmd_cres(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_FREE
int cmd_free(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_DF
int cmd_df(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_UNAME
int cmd_uname(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_DATE
int cmd_date(int argc, char **argv);
#endif

const char *cu_basename(const char *path);
int cu_path_abs(const char *in, char *out, unsigned int outsz);

#endif
