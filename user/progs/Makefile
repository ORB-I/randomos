CC := x86_64-elf-gcc
# link libc.a directly: the loader has no way to satisfy DT_NEEDED
# entries yet, and libc.so isn't shipped on the user disk anyway
CCFLAGS := ../libc/libc.a ../libc/crt0.o \
    -nostdlib -fno-builtin -fno-stack-protector \
    -I../libc/include -nostartfiles -nodefaultlibs \
    -ffreestanding -m64 -T../../share/user.ld \
	-g -MMD -MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
	-Wall -Wextra

PROGS := init \
		 echo \
		 ls \
		 mkfile \
		 poweroff \
		 reboot \
		 sleep \
		 clear \
		 rm \
		 wm \
		 fbtest \
		 dspinfo \
		 wintest \
		 keytest \
		 mousetest

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