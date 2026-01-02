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

COREUTILS_SRCS = \
	$(SRCDIR)/coreutils.c \
	$(SRCDIR)/main.c \
	$(SRCDIR)/dispatch.c \
	$(SRCDIR)/argv0.c \
	$(SRCDIR)/path.c \
	$(SRCDIR)/cmd/cmd_echo.c \
	$(SRCDIR)/cmd/cmd_pwd.c \
	$(SRCDIR)/cmd/cmd_ls.c \
	$(SRCDIR)/cmd/cmd_cat.c \
	$(SRCDIR)/cmd/cmd_touch.c \
	$(SRCDIR)/cmd/cmd_mkdir.c \
	$(SRCDIR)/cmd/cmd_rm.c \
	$(SRCDIR)/cmd/cmd_write.c \
	$(SRCDIR)/cmd/cmd_ticks.c

COREUTILS_OBJS = $(COREUTILS_SRCS:.c=.o)

PROGRAMS = lebcu.bin echo.bin pwd.bin ls.bin cat.bin touch.bin mkdir.bin rm.bin write.bin ticks.bin

.PHONY: all clean stage

all: $(PROGRAMS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lebcu.bin: $(COREUTILS_OBJS) $(CRT1) $(CRTI) $(CRTN) $(LIBC_A) $(LIBLEBIRUN)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/lib -o $@ $(CRT1) $(CRTI) $(COREUTILS_OBJS) -llebirun -lc $(CRTN) -lgcc

%.bin: $(SRCDIR)/wrap/wrap_%.o $(CRT1) $(CRTI) $(CRTN) $(LIBC_A) $(LIBLEBIRUN)
	$(CC) -nostdlib -static -Wl,-z,noexecstack -T $(LD_SCRIPT) -L$(LIBC)/lib -o $@ $(CRT1) $(CRTI) $< -llebirun -lc $(CRTN) -lgcc

stage: all
	mkdir -p $(SYSROOT_BIN)
	cp lebcu.bin $(SYSROOT_BIN)/lebcu
	cp echo.bin $(SYSROOT_BIN)/echo
	cp pwd.bin $(SYSROOT_BIN)/pwd
	cp ls.bin $(SYSROOT_BIN)/ls
	cp cat.bin $(SYSROOT_BIN)/cat
	cp touch.bin $(SYSROOT_BIN)/touch
	cp mkdir.bin $(SYSROOT_BIN)/mkdir
	cp rm.bin $(SYSROOT_BIN)/rm
	cp write.bin $(SYSROOT_BIN)/write
	cp ticks.bin $(SYSROOT_BIN)/ticks

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(SRCDIR)/cmd/*.o
	rm -f $(SRCDIR)/wrap/*.o
	rm -f *.bin
