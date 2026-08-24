include ../../mk/tools.mk

ASFLAGS := -felf64
CCFLAGS := -m64 -nostdlib -fno-builtin \
    -fno-stack-protector -Iinclude \
    -nostartfiles -nodefaultlibs \
    -ffreestanding -Wall -Wextra -g \
	-MMD -MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
	-Wl,--allow-shlib-undefined
CCFLAGS_SO := $(CCFLAGS) -fPIC
LDFLAGS_SO := -shared -Wl,-soname,/lib/libc.so -Wl,-shared -m64 -nostdlib -ffreestanding

LIBC_SO := libc.so
LIBC := libc.a
CRT := crt0.o

LIBSRC := $(shell find . -name "*.c")
ASMSRC := $(shell find . -name "*.asm")

LIBA_SRC := $(filter-out ./src/crt.asm src/crt.asm, $(ASMSRC))

LIBOBJ := $(LIBSRC:.c=.o)
ASMOBJ := $(LIBA_SRC:.asm=.o)

LIB_SOOBJ := $(LIBSRC:.c=.so.o)
ASM_SOOBJ := $(LIBA_SRC:.asm=.so.o)

DEPS := $(LIBSRC:.c=.d)

ALL_LIB_OBJS := $(LIBOBJ) $(ASMOBJ)
ALL_SOFILE_OBJS := $(LIB_SOOBJ) $(ASM_SOOBJ)

CRTSRC := src/crt.asm

DRIVE ?= ../../drive.img

all: $(DRIVE) $(LIBC) $(CRT) $(LIBC_SO) copy

$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16
	mkfs.msdos -F 16 $@
	mmd -i $(DRIVE) ::/bin
	mmd -i $(DRIVE) ::/etc
	mmd -i $(DRIVE) ::/lib

$(LIBC): $(ALL_LIB_OBJS)
	$(AR) rcs $@ $^
$(LIBC_SO): $(ALL_SOFILE_OBJS)
	$(CC) $(LDFLAGS_SO) $^ -o $@
# same as user/progs: copy is a separate pass so a recreated drive.img
# still gets libc even when libc.so itself is already built
copy: $(LIBC_SO)
	mcopy -voi $(DRIVE) -s $(LIBC_SO) ::/lib/$(LIBC_SO)
$(CRT): $(CRTSRC)
	$(AS) $(ASFLAGS) $< -o $@
%.so.o: %.c
	$(CC) $(CCFLAGS_SO) -c $< -o $@
%.so.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@
%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@
clean:
	rm -f $(LIBC) $(CRT) $(LIBOBJ) $(ASMOBJ) $(DEPS) $(LIBC_SO) $(ALL_SOFILE_OBJS)

.PHONY: all copy clean
.SECONDARY: $(ALL_SOFILE_OBJS) $(ALL_LIB_OBJS)

-include $(DEPS)
