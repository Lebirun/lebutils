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

CFLAGS = -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -Os -fomit-frame-pointer -nostdinc -ffunction-sections -fdata-sections -fdiagnostics-color=always
CPPFLAGS = -isystem $(SYSROOT)/usr/include -isystem $(LIBC)/leblibc/arch/x86_64 -isystem $(LIBC)/leblibc/arch/generic -I$(LIBC)/include -I$(LIBC)/src -I./src -I./src/cmd -I./src/wrap

CRT1 = $(SYSROOT)/usr/lib/crt1.o
CRTI = $(SYSROOT)/usr/lib/crti.o
CRTN = $(SYSROOT)/usr/lib/crtn.o
LIBC_A = $(SYSROOT)/usr/lib/libc.a

LD_SCRIPT = $(LIBC)/user.ld

SYSROOT_BIN = ../../root/bin
SYSROOT_SBIN = ../../root/sbin

SBIN_APPS = mount umount lebnet lebpkg ldiskutil lformat.ext4 useradd userdel insmod rmmod modinfo modprobe ping dmesg

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
ifeq ($(COMMAND_INSMOD),y)
CONFIG_DEFINES += -DCONFIG_CMD_INSMOD
endif
ifeq ($(COMMAND_RMMOD),y)
CONFIG_DEFINES += -DCONFIG_CMD_RMMOD
endif
ifeq ($(COMMAND_LSMOD),y)
CONFIG_DEFINES += -DCONFIG_CMD_LSMOD
endif
ifeq ($(COMMAND_MODINFO),y)
CONFIG_DEFINES += -DCONFIG_CMD_MODINFO
endif
ifeq ($(COMMAND_MODPROBE),y)
CONFIG_DEFINES += -DCONFIG_CMD_MODPROBE
endif
ifeq ($(COMMAND_CP),y)
CONFIG_DEFINES += -DCONFIG_CMD_CP
endif
ifeq ($(COMMAND_MV),y)
CONFIG_DEFINES += -DCONFIG_CMD_MV
endif
ifeq ($(COMMAND_HEAD),y)
CONFIG_DEFINES += -DCONFIG_CMD_HEAD
endif
ifeq ($(COMMAND_TAIL),y)
CONFIG_DEFINES += -DCONFIG_CMD_TAIL
endif
ifeq ($(COMMAND_GREP),y)
CONFIG_DEFINES += -DCONFIG_CMD_GREP
endif
ifeq ($(COMMAND_CLEAR),y)
CONFIG_DEFINES += -DCONFIG_CMD_CLEAR
endif
ifeq ($(COMMAND_HOSTNAME),y)
CONFIG_DEFINES += -DCONFIG_CMD_HOSTNAME
endif
ifeq ($(COMMAND_WHOAMI),y)
CONFIG_DEFINES += -DCONFIG_CMD_WHOAMI
endif
ifeq ($(COMMAND_HEXDUMP),y)
CONFIG_DEFINES += -DCONFIG_CMD_HEXDUMP
endif
ifeq ($(COMMAND_PS),y)
CONFIG_DEFINES += -DCONFIG_CMD_PS
endif
ifeq ($(COMMAND_KILL),y)
CONFIG_DEFINES += -DCONFIG_CMD_KILL
endif
ifeq ($(COMMAND_DMESG),y)
CONFIG_DEFINES += -DCONFIG_CMD_DMESG
endif
ifeq ($(COMMAND_FILE),y)
CONFIG_DEFINES += -DCONFIG_CMD_FILE
endif
ifeq ($(COMMAND_PING),y)
CONFIG_DEFINES += -DCONFIG_CMD_PING
endif
ifeq ($(COMMAND_IPV67CLI),y)
CONFIG_DEFINES += -DCONFIG_CMD_IPV67CLI
endif
ifeq ($(COMMAND_IPV67D),y)
CONFIG_DEFINES += -DCONFIG_CMD_IPV67D
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
ifeq ($(COMMAND_INSMOD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_insmod.c
endif
ifeq ($(COMMAND_RMMOD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_rmmod.c
endif
ifeq ($(COMMAND_LSMOD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_lsmod.c
endif
ifeq ($(COMMAND_MODINFO),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_modinfo.c
endif
ifeq ($(COMMAND_MODPROBE),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_modprobe.c
endif
ifeq ($(COMMAND_CP),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_cp.c
endif
ifeq ($(COMMAND_MV),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_mv.c
endif
ifeq ($(COMMAND_HEAD),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_head.c
endif
ifeq ($(COMMAND_TAIL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_tail.c
endif
ifeq ($(COMMAND_GREP),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_grep.c
endif
ifeq ($(COMMAND_CLEAR),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_clear.c
endif
ifeq ($(COMMAND_HOSTNAME),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_hostname.c
endif
ifeq ($(COMMAND_WHOAMI),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_whoami.c
endif
ifeq ($(COMMAND_HEXDUMP),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_hexdump.c
endif
ifeq ($(COMMAND_PS),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_ps.c
endif
ifeq ($(COMMAND_KILL),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_kill.c
endif
ifeq ($(COMMAND_DMESG),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_dmesg.c
endif
ifeq ($(COMMAND_FILE),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_file.c
endif
ifeq ($(COMMAND_PING),y)
LEBUTILS_SRCS += $(SRCDIR)/cmd/cmd_ping.c
endif

IPV67_CRYPTO_SRC = $(SRCDIR)/cmd/ipv67/ipv67_crypto.c
IPV67_CRYPTO_OBJ = build/cmd/ipv67/ipv67_crypto.o

IPV67CLI_SRCS = $(SRCDIR)/cmd/ipv67/cli/ipv67cli.c
IPV67CLI_OBJS = $(patsubst $(SRCDIR)/%.c,build/%.o,$(IPV67CLI_SRCS)) $(IPV67_CRYPTO_OBJ)

IPV67D_SRCS = $(SRCDIR)/cmd/ipv67/daemon/ipv67d.c
IPV67D_OBJS = $(patsubst $(SRCDIR)/%.c,build/%.o,$(IPV67D_SRCS)) $(IPV67_CRYPTO_OBJ)

COREUTILS_OBJS = $(patsubst $(SRCDIR)/%.c,build/%.o,$(LEBUTILS_SRCS))

LEB_SYSCALLS_SRC = $(LIBC)/src/syscalls.c
LEB_SYSCALLS_OBJ = build/syscalls_vfs.o
LEB_LSYSCALLS_SRC = $(LIBC)/src/leb_syscalls.c
LEB_LSYSCALLS_OBJ = build/leb_syscalls.o

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
ifeq ($(COMMAND_INSMOD),y)
BIN_TARGETS += insmod
endif
ifeq ($(COMMAND_RMMOD),y)
BIN_TARGETS += rmmod
endif
ifeq ($(COMMAND_LSMOD),y)
BIN_TARGETS += lsmod
endif
ifeq ($(COMMAND_MODINFO),y)
BIN_TARGETS += modinfo
endif
ifeq ($(COMMAND_MODPROBE),y)
BIN_TARGETS += modprobe
endif
ifeq ($(COMMAND_CP),y)
BIN_TARGETS += cp
endif
ifeq ($(COMMAND_MV),y)
BIN_TARGETS += mv
endif
ifeq ($(COMMAND_HEAD),y)
BIN_TARGETS += head
endif
ifeq ($(COMMAND_TAIL),y)
BIN_TARGETS += tail
endif
ifeq ($(COMMAND_GREP),y)
BIN_TARGETS += grep
endif
ifeq ($(COMMAND_CLEAR),y)
BIN_TARGETS += clear
endif
ifeq ($(COMMAND_HOSTNAME),y)
BIN_TARGETS += hostname
endif
ifeq ($(COMMAND_WHOAMI),y)
BIN_TARGETS += whoami
endif
ifeq ($(COMMAND_HEXDUMP),y)
BIN_TARGETS += hexdump
endif
ifeq ($(COMMAND_PS),y)
BIN_TARGETS += ps
endif
ifeq ($(COMMAND_KILL),y)
BIN_TARGETS += kill
endif
ifeq ($(COMMAND_DMESG),y)
BIN_TARGETS += dmesg
endif
ifeq ($(COMMAND_FILE),y)
BIN_TARGETS += file
endif
ifeq ($(COMMAND_PING),y)
BIN_TARGETS += ping
endif

BINDIR = bin
PROGRAMS := $(addprefix $(BINDIR)/,$(addsuffix .bin,$(BIN_TARGETS)))

IPV67_TARGETS :=
ifeq ($(COMMAND_IPV67CLI),y)
IPV67_TARGETS += $(BINDIR)/ipv67cli.bin
endif
ifeq ($(COMMAND_IPV67D),y)
IPV67_TARGETS += $(BINDIR)/ipv67d.bin
endif

.PHONY: all clean stage lebconfig clean-lebconfig showconfig ipv67cli ipv67d stage-ipv67cli stage-ipv67d

all: $(PROGRAMS) $(IPV67_TARGETS)

ipv67cli: $(BINDIR)/ipv67cli.bin

ipv67d: $(BINDIR)/ipv67d.bin

stage-ipv67cli: $(BINDIR)/ipv67cli.bin
	$(Q)mkdir -p $(SYSROOT_BIN)
	$(Q)cp $(BINDIR)/ipv67cli.bin $(SYSROOT_BIN)/ipv67cli
	$(MSG_STRIP)$(STRIP) -s $(SYSROOT_BIN)/ipv67cli

stage-ipv67d: $(BINDIR)/ipv67d.bin
	$(Q)mkdir -p $(SYSROOT_SBIN)
	$(Q)cp $(BINDIR)/ipv67d.bin $(SYSROOT_SBIN)/ipv67d
	$(MSG_STRIP)$(STRIP) -s $(SYSROOT_SBIN)/ipv67d

showconfig:
	@echo "Using config: $(CONFIG_FILE)"
	@echo "Enabled commands: $(filter-out lebu,$(BIN_TARGETS))"
	@echo "Config defines: $(CONFIG_DEFINES)"

lebconfig:
	$(MAKE) -C lebconfig
	$(MAKE) -C lebconfig run

clean-lebconfig:
	$(MAKE) -C lebconfig clean


build/%.o: $(SRCDIR)/%.c
	$(Q)mkdir -p $(dir $@)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_VFS_OBJ): $(LEB_VFS_SRC)
	$(Q)mkdir -p $(dir $@)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_SYSCALLS_OBJ): $(LEB_SYSCALLS_SRC)
	$(Q)mkdir -p $(dir $@)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_LSYSCALLS_OBJ): $(LEB_LSYSCALLS_SRC)
	$(Q)mkdir -p $(dir $@)
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINDIR)/lebu.bin: $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(Q)mkdir -p $(BINDIR)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

$(IPV67_CRYPTO_OBJ): $(IPV67_CRYPTO_SRC)
	$(Q)mkdir -p $(dir $(IPV67_CRYPTO_OBJ))
	$(MSG_CC)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINDIR)/ipv67cli.bin: $(IPV67CLI_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(Q)mkdir -p $(BINDIR)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $(IPV67CLI_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

$(BINDIR)/ipv67d.bin: $(IPV67D_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(Q)mkdir -p $(BINDIR)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $(IPV67D_OBJS) $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

$(BINDIR)/%.bin: build/wrap/wrap_%.o $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(Q)mkdir -p $(BINDIR)
	$(MSG_LD)$(CC) -nostdlib -static -Wl,-z,noexecstack -Wl,--gc-sections -T $(LD_SCRIPT) -L$(SYSROOT)/usr/lib -o $@ $(CRT1) $(CRTI) $< $(LEB_SYSCALLS_OBJ) $(LEB_LSYSCALLS_OBJ) -lc $(CRTN) -lgcc

stage: all
	$(Q)mkdir -p $(SYSROOT_BIN)
	$(Q)mkdir -p $(SYSROOT_SBIN)
	$(Q)cp $(BINDIR)/lebu.bin $(SYSROOT_BIN)/lebu
	$(MSG_STRIP)$(STRIP) -s $(SYSROOT_BIN)/lebu
	@for app in $(filter-out lebu $(SBIN_APPS),$(BIN_TARGETS)); do \
		ln -sf lebu $(SYSROOT_BIN)/$$app; \
	done
	@[ -f $(BINDIR)/echo.bin ] && { rm -f $(SYSROOT_BIN)/echo; cp $(BINDIR)/echo.bin $(SYSROOT_BIN)/echo; $(STRIP) -s $(SYSROOT_BIN)/echo; } || true
	@[ -f $(BINDIR)/free.bin ] && { rm -f $(SYSROOT_BIN)/free; cp $(BINDIR)/free.bin $(SYSROOT_BIN)/free; $(STRIP) -s $(SYSROOT_BIN)/free; } || true
	@for app in $(SBIN_APPS); do \
		ln -sf ../bin/lebu $(SYSROOT_SBIN)/$$app; \
	done
	@[ -f $(BINDIR)/ipv67cli.bin ] && { cp $(BINDIR)/ipv67cli.bin $(SYSROOT_BIN)/ipv67cli; $(STRIP) -s $(SYSROOT_BIN)/ipv67cli; } || true
	@[ -f $(BINDIR)/ipv67d.bin ] && { cp $(BINDIR)/ipv67d.bin $(SYSROOT_SBIN)/ipv67d; $(STRIP) -s $(SYSROOT_SBIN)/ipv67d; } || true

clean:
	rm -rf build $(BINDIR)
