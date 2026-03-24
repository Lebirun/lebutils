CC = x86_64-elf-gcc
STRIP = x86_64-elf-strip

V ?= 0
ifeq ($(V),0)
  Q = @
  MSG_CC    = @printf '  CC      %s\n' $<;
  MSG_LD    = @printf '  LD      %s\n' $@;
  MSG_STRIP = @printf '  STRIP   %s\n' $@;
else
  Q =
  MSG_CC =
  MSG_LD =
  MSG_STRIP =
endif

LIBC = ../../libc
LIBC_ABS = $(abspath $(LIBC))
SYSROOT = ../../sysroot

CFLAGS = -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -Os -fomit-frame-pointer -nostdinc -ffunction-sections -fdata-sections
CPPFLAGS = -isystem $(SYSROOT)/usr/include -isystem $(LIBC)/leblibc/arch/x86_64 -isystem $(LIBC)/leblibc/arch/generic -I$(LIBC)/include -I$(LIBC)/src -I./src -I./src/cmd -I./src/wrap

CRT1 = $(SYSROOT)/usr/lib/crt1.o
CRTI = $(SYSROOT)/usr/lib/crti.o
CRTN = $(SYSROOT)/usr/lib/crtn.o
LIBC_A = $(SYSROOT)/usr/lib/libc.a

LD_SCRIPT = $(LIBC)/user.ld

SYSROOT_BIN = ../../root/bin
SYSROOT_SBIN = ../../root/sbin

SBIN_APPS = mount umount lebnet lebpkg ldiskutil lformat.ext4 useradd userdel lke

SRCDIR = src

CONFIG_FILE := $(if $(wildcard .config),.config,.config.default)

-include $(CONFIG_FILE)

CONFIG_DEFINES :=
ifeq ($(COMMAND_CAT),y)
CONFIG_DEFINES += -DCONFIG_CMD_CAT
endif
ifeq ($(COMMAND_ECHO),y)
CONFIG_DEFINES += -DCONFIG_CMD_ECHO
endif
ifeq ($(COMMAND_LS),y)
CONFIG_DEFINES += -DCONFIG_CMD_LS
endif
ifeq ($(COMMAND_MKDIR),y)
CONFIG_DEFINES += -DCONFIG_CMD_MKDIR
endif
ifeq ($(COMMAND_PWD),y)
CONFIG_DEFINES += -DCONFIG_CMD_PWD
endif
ifeq ($(COMMAND_RM),y)
CONFIG_DEFINES += -DCONFIG_CMD_RM
endif
ifeq ($(COMMAND_TOUCH),y)
CONFIG_DEFINES += -DCONFIG_CMD_TOUCH
endif
ifeq ($(COMMAND_CRES),y)
CONFIG_DEFINES += -DCONFIG_CMD_CRES
endif
ifeq ($(COMMAND_FREE),y)
CONFIG_DEFINES += -DCONFIG_CMD_FREE
endif
ifeq ($(COMMAND_DF),y)
CONFIG_DEFINES += -DCONFIG_CMD_DF
endif
ifeq ($(COMMAND_UNAME),y)
CONFIG_DEFINES += -DCONFIG_CMD_UNAME
endif
ifeq ($(COMMAND_DATE),y)
CONFIG_DEFINES += -DCONFIG_CMD_DATE
endif
ifeq ($(COMMAND_LNETURL),y)
CONFIG_DEFINES += -DCONFIG_CMD_LNETURL
endif
ifeq ($(COMMAND_LEBNET),y)
CONFIG_DEFINES += -DCONFIG_CMD_LEBNET
endif
ifeq ($(COMMAND_LEBPKG),y)
CONFIG_DEFINES += -DCONFIG_CMD_LEBPKG
endif
ifeq ($(COMMAND_SYSCALL),y)
CONFIG_DEFINES += -DCONFIG_CMD_SYSCALL
endif
ifeq ($(COMMAND_MOUNT),y)
CONFIG_DEFINES += -DCONFIG_CMD_MOUNT
endif
ifeq ($(COMMAND_UMOUNT),y)
CONFIG_DEFINES += -DCONFIG_CMD_UMOUNT
endif
ifeq ($(COMMAND_PANIC),y)
CONFIG_DEFINES += -DCONFIG_CMD_PANIC
endif
ifeq ($(COMMAND_LTXTEDIT),y)
CONFIG_DEFINES += -DCONFIG_CMD_LTXTEDIT
endif
ifeq ($(COMMAND_LDISKUTIL),y)
CONFIG_DEFINES += -DCONFIG_CMD_LDISKUTIL
endif
ifeq ($(COMMAND_LFORMAT_EXT4),y)
CONFIG_DEFINES += -DCONFIG_CMD_LFORMAT_EXT4
endif
ifeq ($(COMMAND_PASSWD),y)
CONFIG_DEFINES += -DCONFIG_CMD_PASSWD
endif
ifeq ($(COMMAND_USERADD),y)
CONFIG_DEFINES += -DCONFIG_CMD_USERADD
endif
ifeq ($(COMMAND_USERDEL),y)
CONFIG_DEFINES += -DCONFIG_CMD_USERDEL
endif
ifeq ($(COMMAND_CHMOD),y)
CONFIG_DEFINES += -DCONFIG_CMD_CHMOD
endif
ifeq ($(COMMAND_CHOWN),y)
CONFIG_DEFINES += -DCONFIG_CMD_CHOWN
endif
ifeq ($(COMMAND_LKE),y)
CONFIG_DEFINES += -DCONFIG_CMD_LKE
endif

CPPFLAGS += $(CONFIG_DEFINES)

LEBUTILS_SRCS = \
	$(SRCDIR)/lebutils.c \
	$(SRCDIR)/main.c \
	$(SRCDIR)/dispatch.c \
	$(SRCDIR)/argv0.c \
	$(SRCDIR)/path.c

ifeq ($(COMMAND_CAT),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_cat.c
endif
ifeq ($(COMMAND_ECHO),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_echo.c
endif
ifeq ($(COMMAND_LS),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_ls.c
endif
ifeq ($(COMMAND_MKDIR),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_mkdir.c
endif
ifeq ($(COMMAND_PWD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_pwd.c
endif
ifeq ($(COMMAND_RM),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_rm.c
endif
ifeq ($(COMMAND_TOUCH),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_touch.c
endif
ifeq ($(COMMAND_CRES),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_cres.c
endif
ifeq ($(COMMAND_FREE),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_free.c
endif
ifeq ($(COMMAND_DF),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_df.c
endif
ifeq ($(COMMAND_UNAME),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_uname.c
endif
ifeq ($(COMMAND_DATE),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_date.c
endif
ifeq ($(COMMAND_LNETURL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lneturl.c
endif
ifeq ($(COMMAND_LEBNET),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lebnet.c
endif
ifeq ($(COMMAND_LEBPKG),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lebpkg.c
endif
ifeq ($(COMMAND_SYSCALL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_syscall.c
endif
ifeq ($(COMMAND_MOUNT),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_mount.c
endif
ifeq ($(COMMAND_UMOUNT),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_umount.c
endif
ifeq ($(COMMAND_PANIC),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_panic.c
endif
ifeq ($(COMMAND_LTXTEDIT),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_ltxtedit.c
endif
ifeq ($(COMMAND_LDISKUTIL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_ldiskutil.c
endif
ifeq ($(COMMAND_LFORMAT_EXT4),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lformat_ext4.c
endif
ifeq ($(COMMAND_PASSWD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_passwd.c
endif
ifeq ($(COMMAND_USERADD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_useradd.c
endif
ifeq ($(COMMAND_USERDEL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_userdel.c
endif
ifeq ($(COMMAND_CHMOD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_chmod.c
endif
ifeq ($(COMMAND_CHOWN),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_chown.c
endif
ifeq ($(COMMAND_LKE),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lke.c
endif

COREUTILS_OBJS = $(LEBUTILS_SRCS:.c=.o)

LEB_SYSCALLS_SRC = $(LIBC)/src/syscalls.c
LEB_SYSCALLS_OBJ = $(SRCDIR)/syscalls_vfs.o
LEB_LSYSCALLS_SRC = $(LIBC)/src/leb_syscalls.c
LEB_LSYSCALLS_OBJ = $(SRCDIR)/leb_syscalls.o

BIN_TARGETS := lebu
ifeq ($(COMMAND_CAT),y)
BIN_TARGETS += cat
endif
ifeq ($(COMMAND_ECHO),y)
BIN_TARGETS += echo
endif
ifeq ($(COMMAND_LS),y)
BIN_TARGETS += ls
endif
ifeq ($(COMMAND_MKDIR),y)
BIN_TARGETS += mkdir
endif
ifeq ($(COMMAND_PWD),y)
BIN_TARGETS += pwd
endif
ifeq ($(COMMAND_RM),y)
BIN_TARGETS += rm
endif
ifeq ($(COMMAND_TOUCH),y)
BIN_TARGETS += touch
endif
ifeq ($(COMMAND_CRES),y)
BIN_TARGETS += cres
endif
ifeq ($(COMMAND_FREE),y)
BIN_TARGETS += free
endif
ifeq ($(COMMAND_DF),y)
BIN_TARGETS += df
endif
ifeq ($(COMMAND_UNAME),y)
BIN_TARGETS += uname
endif
ifeq ($(COMMAND_DATE),y)
BIN_TARGETS += date
endif
ifeq ($(COMMAND_LNETURL),y)
BIN_TARGETS += lneturl
endif
ifeq ($(COMMAND_LEBNET),y)
BIN_TARGETS += lebnet
endif
ifeq ($(COMMAND_LEBPKG),y)
BIN_TARGETS += lebpkg
endif
ifeq ($(COMMAND_SYSCALL),y)
BIN_TARGETS += syscall
endif
ifeq ($(COMMAND_MOUNT),y)
BIN_TARGETS += mount
endif
ifeq ($(COMMAND_UMOUNT),y)
BIN_TARGETS += umount
endif
ifeq ($(COMMAND_PANIC),y)
BIN_TARGETS += panic
endif
ifeq ($(COMMAND_LTXTEDIT),y)
BIN_TARGETS += ltxtedit
endif
ifeq ($(COMMAND_LDISKUTIL),y)
BIN_TARGETS += ldiskutil
endif
ifeq ($(COMMAND_LFORMAT_EXT4),y)
BIN_TARGETS += lformat.ext4
endif
ifeq ($(COMMAND_PASSWD),y)
BIN_TARGETS += passwd
endif
ifeq ($(COMMAND_USERADD),y)
BIN_TARGETS += useradd
endif
ifeq ($(COMMAND_USERDEL),y)
BIN_TARGETS += userdel
endif
ifeq ($(COMMAND_CHMOD),y)
BIN_TARGETS += chmod
endif
ifeq ($(COMMAND_CHOWN),y)
BIN_TARGETS += chown
endif
ifeq ($(COMMAND_LKE),y)
BIN_TARGETS += lke
endif

PROGRAMS := $(addsuffix .bin,$(BIN_TARGETS))

.PHONY: all clean stage lebconfig clean-lebconfig showconfig

all: $(PROGRAMS)

showconfig:
	@echo "Using config: $(CONFIG_FILE)"
	@echo "Enabled commands: $(filter-out lebu,$(BIN_TARGETS))"
	@echo "Config defines: $(CONFIG_DEFINES)"

lebconfig:
	$(MAKE) -C lebconfig
	$(MAKE) -C lebconfig run

clean-lebconfig:
	$(MAKE) -C lebconfig clean


%.o: %.c
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_VFS_OBJ): $(LEB_VFS_SRC)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_SYSCALLS_OBJ): $(LEB_SYSCALLS_SRC)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_LSYSCALLS_OBJ): $(LEB_LSYSCALLS_SRC)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lebu.bin: $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

%.bin: $(SRCDIR)/wrap/wrap_%.o $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $< $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

stage: all
	$(Q)mkdir -p $(SYSROOT_BIN)
	$(Q)mkdir -p $(SYSROOT_SBIN)
	$(Q)cp lebu.bin $(SYSROOT_BIN)/lebu
	$(MSG_STRIP)$(STRIP) -s $(SYSROOT_BIN)/lebu
	@for app in $(filter-out lebu $(SBIN_APPS),$(BIN_TARGETS)); do \
		ln -sf lebu $(SYSROOT_BIN)/$$app; \
	done
	@for app in $(SBIN_APPS); do \
		ln -sf ../bin/lebu $(SYSROOT_SBIN)/$$app; \
	done

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(SRCDIR)/cmd/*.o
	rm -f $(SRCDIR)/wrap/*.o
	rm -f *.bin
