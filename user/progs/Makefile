include ../../mk/tools.mk

# compile and link are separate steps and the link goes straight to
# ld.lld, same reason as the kernel makefile: clang would delegate to
# host gcc and every machine interprets that differently
CCFLAGS := -m64 -nostdlib -fno-builtin \
    -I../libc/include -ffreestanding \
	-I../libs/libmcrypto -fno-pie -g -MMD \
	-MP -mno-mmx -mno-sse -mno-sse2 \
	-mno-red-zone -msoft-float \
	-mno-fp-ret-in-387 -Wall -Wextra -Werror
LDFLAGS := -m elf_x86_64 --no-pie

PROGS := init sh echo cat \
		 ls mkfile poweroff \
		 reboot sleep clear \
		 rm wm fbtest dspinfo \
		 wintest keytest mousetest \
		 id whoami serialtest environ \
			 random man rtop

PROGS_C := login

DRIVE ?= ../../drive.img

DEPLIBS := ../libc/libc.so ../libc/crt0.o
DEPLIBS_C := ../libs/libmcrypto/libmcrypto.so

DEPS := $(PROGS:=.d)

PROGFILES := $(PROGS:=.elf)
PROGFILES_C := $(PROGS_C:=.elf)

all: $(DRIVE) $(PROGFILES) $(PROGFILES_C) copy

$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=100
	mkfs.ext2 $@
	debugfs -w -R "mkdir /bin" $@
	debugfs -w -R "mkdir /etc" $@
	debugfs -w -R "mkdir /lib" $@

	#mkfs.msdos -F 16 $@
	#mmd -i $(DRIVE) ::/bin
	#mmd -i $(DRIVE) ::/etc
	#mmd -i $(DRIVE) ::/lib


$(PROGFILES): %.elf: %.c $(DEPLIBS)
	$(CC) $(CCFLAGS) -c $< -o $(basename $@).o
	$(LD) $(LDFLAGS) $(basename $@).o $(DEPLIBS) -o $@

$(PROGFILES_C): %.elf: %.c $(DEPLIBS) $(DEPLIBS_C)
	$(CC) $(CCFLAGS) -c $< -o $(basename $@).o
	$(LD) $(LDFLAGS) $(basename $@).o $(DEPLIBS) $(DEPLIBS_C) -o $@

# copy is its own pass so a freshly created drive.img gets populated
# even when every .elf is already built - mcopy used to live inside the
# link rules and up-to-date targets skipped it, leaving /bin half empty
copy: $(PROGFILES) $(PROGFILES_C)
	for f in $(PROGFILES) $(PROGFILES_C); do \
		debugfs -w -R "rm /bin/$$(basename $$f .elf)" $(DRIVE); \
		debugfs -w -R "write $$f /bin/$$(basename $$f .elf)" $(DRIVE); \
	done

clean:
	rm -f $(PROGFILES) $(PROGS:=.o) $(DEPS) $(DRIVE)
.PHONY: all copy clean
-include $(DEPS)