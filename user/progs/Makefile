include ../../mk/tools.mk

# compile and link are separate steps and the link goes straight to
# ld.lld, same reason as the kernel makefile: clang would delegate to
# host gcc and every machine interprets that differently
CCFLAGS := -nostdlib -fno-builtin -fno-stack-protector \
    -I../libc/include -ffreestanding \
	-fno-pie \
	-g -MMD -MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
	-Wall -Wextra
LDFLAGS := -m elf_x86_64 --no-pie

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

DEPLIBS := ../libc/libc.so ../libc/crt0.o
DEPS := $(PROGS:=.d)

PROGFILES := $(PROGS:=.elf)

all: $(DRIVE) $(PROGFILES)
$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16
	mkfs.msdos -F 16 $@

$(PROGFILES): %.elf: %.c $(DEPLIBS)
	$(CC) $(CCFLAGS) -c $< -o $(basename $@).o
	$(LD) $(LDFLAGS) $(basename $@).o $(DEPLIBS) -o $@
	mcopy -voi $(DRIVE) -s $@ ::/$(basename $@)

clean:
	rm -f $(PROGFILES) $(PROGS:=.o) $(DEPS) $(DRIVE)
.PHONY: all clean
-include $(DEPS)