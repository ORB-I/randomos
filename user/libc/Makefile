CC := x86_64-elf-gcc
AR := x86_64-elf-ar
AS := nasm

ASFLAGS := -felf64
CCFLAGS := -m64 -nostdlib -fno-builtin \
    -fno-stack-protector -Iinclude \
    -nostartfiles -nodefaultlibs \
    -ffreestanding -Wall -Wextra -g \
	-MMD -MP

LIBC := libc.a
CRT := crt0.o

LIBSRC := $(shell find . -name "*.c")
ASMSRC := $(shell find . -name "*.asm")

LIBA_SRC := $(filter-out ./src/crt.asm src/crt.asm, $(ASMSRC))

LIBOBJ := $(LIBSRC:.c=.o)
ASMOBJ := $(LIBA_SRC:.asm=.o)

DEPS := $(LIBSRC:.c=.d)

ALL_LIB_OBJS := $(LIBOBJ) $(ASMOBJ)

CRTSRC := src/crt.asm

all: $(LIBC) $(CRT)

$(LIBC): $(ALL_LIB_OBJS)
	$(AR) rcs $@ $^
$(CRT): $(CRTSRC)
	$(AS) $(ASFLAGS) $< -o $@
%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@
clean:
	rm -f $(LIBC) $(CRT) $(LIBOBJ) $(ASMOBJ) $(DEPS)

.PHONY: all clean
-include $(DEPS)