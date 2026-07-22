#ifndef CU_H
#define CU_H

#include <lebirun.h>

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
#ifdef CONFIG_CMD_LNETURL
int cmd_lneturl(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LEBNET
int cmd_lebnet(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LEBPKG
int cmd_lebpkg(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_SYSCALL
int cmd_syscall(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_MOUNT
int cmd_mount(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_UMOUNT
int cmd_umount(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_PANIC
int cmd_panic(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LTXTEDIT
int cmd_ltxtedit(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LDISKUTIL
int cmd_ldiskutil(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LFORMAT_EXT4
int cmd_lformat_ext4(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_PASSWD
int cmd_passwd(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_USERADD
int cmd_useradd(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_USERDEL
int cmd_userdel(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_CHMOD
int cmd_chmod(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_CHOWN
int cmd_chown(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_INSMOD
int cmd_insmod(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_RMMOD
int cmd_rmmod(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LSMOD
int cmd_lsmod(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_MODINFO
int cmd_modinfo(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_MODPROBE
int cmd_modprobe(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_CP
int cmd_cp(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_MV
int cmd_mv(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_HEAD
int cmd_head(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_TAIL
int cmd_tail(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_GREP
int cmd_grep(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_CLEAR
int cmd_clear(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_HOSTNAME
int cmd_hostname(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_WHOAMI
int cmd_whoami(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_HEXDUMP
int cmd_hexdump(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_PS
int cmd_ps(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_KILL
int cmd_kill(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_DMESG
int cmd_dmesg(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_FILE
int cmd_file(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_LESS
int cmd_less(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_DIFF
int cmd_diff(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_TRUNCATE
int cmd_truncate(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_SUDO
int cmd_sudo(int argc, char **argv);
#endif
#ifdef CONFIG_CMD_PING
int cmd_ping(int argc, char **argv);
#endif

const char *cu_basename(const char *path);
int cu_path_abs(const char *in, char *out, unsigned int outsz);

#endif
