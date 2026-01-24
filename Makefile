CC = i686-elf-gcc
STRIP = i686-elf-strip

LIBC = ../../libc
LIBC_ABS = $(abspath $(LIBC))

CFLAGS = -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -m32 -Os -fomit-frame-pointer -nostdinc
CPPFLAGS = -isystem $(LIBC)/leblibc/include -isystem $(LIBC)/leblibc/build-i386/obj/include -isystem $(LIBC)/leblibc/arch/i386 -isystem $(LIBC)/leblibc/arch/generic -I$(LIBC)/include -I$(LIBC)/src -I./src -I./src/cmd -I./src/wrap

CRT1 = $(LIBC)/leblibc/build-i386/lib/crt1.o
CRTI = $(LIBC)/leblibc/build-i386/lib/crti.o
CRTN = $(LIBC)/leblibc/build-i386/lib/crtn.o
LIBC_A = $(LIBC)/leblibc/build-i386/lib/libc.a

LD_SCRIPT = $(LIBC)/user.ld

SYSROOT_BIN = ../../root/bin

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
ifeq ($(COMMAND_TICKS),y)
CONFIG_DEFINES += -DCONFIG_CMD_TICKS
endif
ifeq ($(COMMAND_TOUCH),y)
CONFIG_DEFINES += -DCONFIG_CMD_TOUCH
endif
ifeq ($(COMMAND_WRITE),y)
CONFIG_DEFINES += -DCONFIG_CMD_WRITE
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

CPPFLAGS += $(CONFIG_DEFINES)

COREUTILS_SRCS = \
	$(SRCDIR)/coreutils.c \
	$(SRCDIR)/main.c \
	$(SRCDIR)/dispatch.c \
	$(SRCDIR)/argv0.c \
	$(SRCDIR)/path.c

ifeq ($(COMMAND_CAT),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_cat.c
endif
ifeq ($(COMMAND_ECHO),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_echo.c
endif
ifeq ($(COMMAND_LS),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_ls.c
endif
ifeq ($(COMMAND_MKDIR),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_mkdir.c
endif
ifeq ($(COMMAND_PWD),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_pwd.c
endif
ifeq ($(COMMAND_RM),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_rm.c
endif
ifeq ($(COMMAND_TICKS),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_ticks.c
endif
ifeq ($(COMMAND_TOUCH),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_touch.c
endif
ifeq ($(COMMAND_WRITE),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_write.c
endif
ifeq ($(COMMAND_CRES),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_cres.c
endif
ifeq ($(COMMAND_FREE),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_free.c
endif
ifeq ($(COMMAND_DF),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_df.c
endif
ifeq ($(COMMAND_UNAME),y)
COREUTILS_SRCS += $(SRCDIR)/cmd/cmd_uname.c
endif

COREUTILS_OBJS = $(COREUTILS_SRCS:.c=.o)

LEB_SYSCALLS_SRC = $(LIBC)/src/syscalls.c
LEB_SYSCALLS_OBJ = $(SRCDIR)/syscalls_vfs.o

BIN_TARGETS := lebcu
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
ifeq ($(COMMAND_TICKS),y)
BIN_TARGETS += ticks
endif
ifeq ($(COMMAND_TOUCH),y)
BIN_TARGETS += touch
endif
ifeq ($(COMMAND_WRITE),y)
BIN_TARGETS += write
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

PROGRAMS := $(addsuffix .bin,$(BIN_TARGETS))

.PHONY: all clean stage lebconfig clean-lebconfig showconfig

all: $(PROGRAMS)

showconfig:
	@echo "Using config: $(CONFIG_FILE)"
	@echo "Enabled commands: $(filter-out lebcu,$(BIN_TARGETS))"
	@echo "Config defines: $(CONFIG_DEFINES)"

lebconfig:
	$(MAKE) -C lebconfig
	$(MAKE) -C lebconfig run

clean-lebconfig:
	$(MAKE) -C lebconfig clean


%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_VFS_OBJ): $(LEB_VFS_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LEB_SYSCALLS_OBJ): $(LEB_SYSCALLS_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lebcu.bin: $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/leblibc/build-i386/lib -o $@ $(CRT1) $(CRTI) $(COREUTILS_OBJS) $(LEB_SYSCALLS_OBJ) -lc $(CRTN) -lgcc

%.bin: $(SRCDIR)/wrap/wrap_%.o $(LEB_SYSCALLS_OBJ) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/leblibc/build-i386/lib -o $@ $(CRT1) $(CRTI) $< $(LEB_SYSCALLS_OBJ) -lc $(CRTN) -lgcc

stage: all
	mkdir -p $(SYSROOT_BIN)
	@for app in $(BIN_TARGETS); do \
		if [ -f $$app.bin ]; then \
			cp $$app.bin $(SYSROOT_BIN)/$$app; \
		else \
			echo "$$app.bin not built; skipping"; \
		fi; \
	done

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(SRCDIR)/cmd/*.o
	rm -f $(SRCDIR)/wrap/*.o
	rm -f *.bin
