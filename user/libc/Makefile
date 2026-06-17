CC := i686-elf-gcc
AR := i686-elf-ar
AS := nasm

ASFLAGS := -felf32
CCFLAGS := -m32 -nostdlib -fno-builtin \
    -fno-stack-protector -Iinclude \
    -nostartfiles -nodefaultlibs \
    -ffreestanding -Wall -Wextra -g

LIBC := libc.a
CRT := crt0.o

LIBSRC := $(shell find . -name "*.c")
ASMSRC := $(shell find . -name "*.asm")

LIBA_SRC := $(filter-out ./src/crt.asm src/crt.asm, $(ASMSRC))

LIBOBJ := $(LIBSRC:.c=.o)
ASMOBJ := $(LIBA_SRC:.asm=.o)

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
	rm -f $(LIBC) $(CRT) $(LIBOBJ) $(ASMOBJ)