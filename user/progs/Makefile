include ../../mk/tools.mk

# compile and link are separate steps and the link goes straight to
# ld.lld, same reason as the kernel makefile: clang would delegate to
# host gcc and every machine interprets that differently
CCFLAGS := -nostdlib -fno-builtin -fno-stack-protector \
    -I../libc/include -ffreestanding \
	-fno-pie \
	-g -MMD -MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
	-Wall -Wextra
LDFLAGS := -m elf_x86_64 -T ../../share/user.ld --no-pie

# libc.a is linked directly because the loader has no way to satisfy
# DT_NEEDED entries yet, and libc.so isnt shipped on the user disk

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
	dd if=/dev/zero of=$@ bs=1048576 count=16
	mformat -i $@ -F ::

$(PROGFILES): %.elf: %.c $(DEPLIBS)
	$(CC) $(CCFLAGS) -c $< -o $(basename $@).o
	$(LD) $(LDFLAGS) $(basename $@).o $(DEPLIBS) -o $@
	mcopy -voi $(DRIVE) -s $@ ::/$(basename $@)

clean:
	rm -f $(PROGFILES) $(PROGS:=.o) $(DEPS) $(DRIVE)
.PHONY: all clean
-include $(DEPS)