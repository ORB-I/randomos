CC := i686-elf-gcc
AR := i686-elf-ar
AS := nasm

ASFLAGS := -felf32
CCFLAGS := -m32 -nostdlib -fno-builtin \
	-fno-stack-protector -Iinclude \
	-nostartfiles -nodefaultlibs \
	-ffreestanding -Wall -Wextra

LIBC := libc.a
CRT := crt0.o

LIBSRC := $(shell find . -name "*.c")
LIBOBJ := $(LIBSRC:.c=.o)

CRTSRC := src/crt.asm

all: $(LIBC) $(CRT)

$(LIBC): $(LIBOBJ)
	$(AR) rcs $@ $^

$(CRT): $(CRTSRC)
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -f $(LIBC) $(CRT) $(LIBOBJ)