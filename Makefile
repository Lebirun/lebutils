CC = i686-elf-gcc

LIBC = ../../libc
LIBC_ABS = $(abspath $(LIBC))

CFLAGS = -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -m32 -O2 -nostdinc
CPPFLAGS = -isystem $(LIBC)/include -I./src -I./src/cmd -I./src/wrap

CRT1 = $(LIBC)/lib/crt1.o
CRTI = $(LIBC)/lib/crti.o
CRTN = $(LIBC)/lib/crtn.o
LIBC_A = $(LIBC)/lib/libc.a
LIBLEBIRUN = $(LIBC)/lib/liblebirun.a

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

COREUTILS_OBJS = $(COREUTILS_SRCS:.c=.o)

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

lebcu.bin: $(COREUTILS_OBJS) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A) $(LIBLEBIRUN)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/lib -o $@ $(CRT1) $(CRTI) $(COREUTILS_OBJS) -llebirun -lc $(CRTN) -lgcc

%.bin: $(SRCDIR)/wrap/wrap_%.o $(CRT1) $(CRTI) $(CRTN) $(LIBC_A) $(LIBLEBIRUN)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/lib -o $@ $(CRT1) $(CRTI) $< -llebirun -lc $(CRTN) -lgcc

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
