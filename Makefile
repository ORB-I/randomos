include mk/tools.mk

ASFLAGS      := -Iinclude -felf64
LDFLAGS      := -m elf_x86_64 -T share/link.ld --no-pie -O0 -nostdlib -no-pie

LIBS         := -Llib -lflanterm -llwip -luacpi
CCFLAGS      := -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
				-msoft-float -mno-fp-ret-in-387 \
				-m64 -nostdlib -fno-builtin -fno-pie -Iinclude \
		        -nodefaultlibs -ffreestanding -Wall -Wextra -g \
		        -MMD -MP -O0 -fstack-protector-strong \
				
XORRISOFLAGS := -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        		-efi-boot-part --efi-boot-image --protective-msdos-label

QFLAGS := -M pc -cpu qemu64 -boot d -smp 2 -m 1G -serial stdio -accel tcg \
		  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		  -device piix3-usb-uhci,id=uhci \
		  -device usb-kbd,bus=uhci.0,port=1 \
		  -device usb-mouse,bus=uhci.0,port=2 \
		  -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
		  -device virtio-rng-pci \
		  -monitor unix:/tmp/qemu-monitor.sock,server=on,wait=off \
		  -device virtio-keyboard-pci \
		  -device virtio-tablet-pci \
		  -d int,cpu_reset -D qemu.log
QFLAGS_HEADLESS := -display none -serial file:qemu.log

AS_SRC := $(shell find src -name '*.asm')
CC_SRC := $(shell find src -name '*.c')

OBJ  := $(AS_SRC:.asm=.o) $(CC_SRC:.c=.o)
EXE  := kern.elf
ISO  := os.iso
DEPS := $(CC_SRC:.c=.d)
SUS  := $(CC_SRC:.c=.su)

INITRD := initrd.img
INITRD_STAGE := .initrd-stage
PYTHON ?= python3

DRIVE ?= drive.img

SUBDIRS := user/libs/zlib user/libs/libmcrypto \
		   user/libc user/progs user/nasm share/etc share/man \
		   vendor/lwip-2.2.1 vendor/flanterm \
		   vendor/uACPI

all: $(DRIVE) subdirs $(ISO)

subdirs:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir 'CC=$(CC)' 'LD=$(LD)' 'AS=$(AS)' 'AR=$(AR)' 'NM=$(NM)' 'DRIVE=$(shell realpath $(DRIVE))' 'DFLCFLAGS=$(CCFLAGS)' || exit 1; \
	done

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

$(ISO): $(EXE) $(INITRD)
	@$(MAKE) -C limine-binary
	@echo "[ISO] $<"
	@mkdir -p iso/boot/limine
	@cp $< iso/boot/
	@cp $(INITRD) iso/boot/initrd.img
	@cp share/limine.conf limine-binary/limine-bios.sys \
		limine-binary/limine-bios-cd.bin \
    	limine-binary/limine-uefi-cd.bin \
		iso/boot/limine/
	@mkdir -p iso/EFI/BOOT
	@cp limine-binary/BOOTX64.EFI limine-binary/BOOTIA32.EFI iso/EFI/BOOT/
	@$(XORRISO) $(XORRISOFLAGS) -o $@ iso
	./limine-binary/limine bios-install $@
	@$(MAKE) -C limine-binary clean
	@rm -rf iso

# Pack the userland into a cpio "newc" archive the kernel unpacks at boot
# (see src/kern/initramfs.c).  The archive must end up in the ISO as
# /boot/initrd.img because share/limine.conf loads it as a module.
$(INITRD): $(wildcard user/progs/*.elf) user/libc/libc.so \
		user/libs/libmcrypto/libmcrypto.so \
		$(wildcard share/etc/passwd share/etc/passwd.fmt) \
		$(wildcard share/man/*.txt) tools/mkinitrd.py
	@echo "[INITRD] $@"
	@rm -rf $(INITRD_STAGE)
	@mkdir -p $(INITRD_STAGE)/bin $(INITRD_STAGE)/etc \
		$(INITRD_STAGE)/lib $(INITRD_STAGE)/share/man
	@for f in user/progs/*.elf; do \
		cp "$$f" "$(INITRD_STAGE)/bin/$$(basename "$$f" .elf)"; \
	done
	@cp share/etc/passwd share/etc/passwd.fmt "$(INITRD_STAGE)/etc/"
	@cp user/libc/libc.so "$(INITRD_STAGE)/lib/"
	@cp user/libs/libmcrypto/libmcrypto.so "$(INITRD_STAGE)/lib/"
	@cp share/man/*.txt "$(INITRD_STAGE)/share/man/"
	$(PYTHON) tools/mkinitrd.py "$(INITRD_STAGE)" "$@"
	@rm -rf $(INITRD_STAGE)

$(EXE): $(OBJ)
	@echo "[LD] $@"
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)
	python3 tools/mkksyms.py $(NM) $@
	$(CC) $(CCFLAGS) -c ksyms.c -o ksyms.o
	$(LD) $(LDFLAGS) ksyms.o $(OBJ) lib/liblwip.a -o $@ $(LIBS)
	@rm -f ksyms.o ksyms.d

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $<

run: all
	@echo "[QEMU]"
	$(QEMU) $(QFLAGS) $(QEMUFLAGS) -cdrom $(ISO)

debug: all
	@echo "[QEMU DEBUG]"
	$(QEMU) $(QFLAGS) $(QEMUFLAGS) -S -s -cdrom $(ISO)
	
clean:
	@echo "[CLEAN]"
	@rm -f $(OBJ) $(ISO) $(EXE) $(DEPS) $(DRIVE) ksyms.c ksyms.o ksyms.d $(INITRD)
	@rm -rf $(INITRD_STAGE)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir 'CC=$(CC)' 'LD=$(LD)' 'AS=$(AS)' 'AR=$(AR)' 'NM=$(NM)' $@; \
	done

compile_commands.json: clean
	@echo "Generating $@"
	@if command -v bear >/dev/null 2>&1; then \
		bear -- make $(EXE); \
	elif command -v compiledb >/dev/null 2>&1; then \
		compiledb make $(EXE); \
	else \
		echo "ERROR: Please install 'bear' or 'compiledb' to generate compile_commands.json"; \
		exit 1; \
	fi

.PHONY: run run-headless clean all subdirs
-include $(DEPS)
-include $(LWIP_DEPS)
