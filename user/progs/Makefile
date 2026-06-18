CC := i686-elf-gcc
CCFLAGS := -L../libc -lc ../libc/crt0.o \
    -nostdlib -fno-builtin -fno-stack-protector \
    -I../libc/include -nostartfiles -nodefaultlibs \
    -ffreestanding -m32 -T../../share/user.ld \
	-g -MMD -MP

PROGS := echo ls mkfile poweroff reboot sleep clear
DRIVE ?= ../../drive.img

DEPLIBS := ../libc/libc.a ../libc/crt0.o
DEPS := $(PROGS:=.d)

PROGFILES := $(PROGS:=.elf)

all: $(DRIVE) $(PROGFILES)
$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16
	mkfs.msdos -F 16 $@

$(PROGFILES): %.elf: %.c $(DEPLIBS)
	$(CC) $< -o $@ $(CCFLAGS)
	mcopy -voi $(DRIVE) -s $@ ::/$(basename $@)

clean:
	rm -f $(PROGFILES) $(DEPS) $(DRIVE)
.PHONY: all clean
-include $(DEPS)