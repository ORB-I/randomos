CC := i686-elf-gcc
CCFLAGS := -L../libc -lc ../libc/crt0.o \
    -nostdlib -fno-builtin -fno-stack-protector \
    -I../libc/include -nostartfiles -nodefaultlibs \
    -ffreestanding -m32 -T../../share/user.ld -g

PROGS := echo ls mkfile poweroff reboot sleep
DRIVE ?= ../../drive.img

all: $(DRIVE) $(PROGS)
$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16

$(PROGS): %: %.c
	$(CC) $< -o $@ $(CCFLAGS)
	mcopy -voi $(DRIVE) -s $@ ::/;

clean:
	rm -f $(PROGS)
.PHONY: all clean