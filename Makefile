# toolchain discovery lives in mk/tools.mk so the kernel, libc and
# progs builds can never drift apart. dont inline it back here.
include mk/tools.mk

ASFLAGS      := -Iinclude -felf64
# --no-pie goes through -Wl because some clang versions drop the bare
# -no-pie instead of handing it to lld, which then rejects the 32S
# relocations our -mcmodel=kernel code produces
LDFLAGS      := -Tshare/link.ld -m64 -ffreestanding -O0 -nostdlib -fuse-ld=lld -Wl,--no-pie
# no -lgcc on purpose: the kernel doesnt need its builtins and pure
# llvm machines (mac) dont ship it anyway
LIBS         := -Llib -llai -lff -lflanterm
CCFLAGS      := -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
				-m64 -nostdlib -fno-builtin -fno-stack-protector -fno-pie -Iinclude \
		        -nostartfiles -nodefaultlibs -ffreestanding -Wall -Wextra -g \
		        -MMD -MP -O0
XORRISOFLAGS := -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        		--efi-boot-part --efi-boot-image --protective-msdos-label
QFLAGS       := -M pc -boot d -m 1G -monitor stdio \
				-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
				-drive id=disk,file=drive.img,format=raw,if=none \
  				-device ide-hd,drive=disk,bus=ide.0,unit=0 \
				-device piix3-usb-uhci,id=uhci \
				-device usb-kbd,bus=uhci.0,port=1 \
				-device usb-mouse,bus=uhci.0,port=2

AS_SRC := $(shell find src -name '*.asm')
CC_SRC := $(shell find src -name '*.c')

OBJ  := $(AS_SRC:.asm=.o) $(CC_SRC:.c=.o)
EXE  := kern.elf
ISO  := os.iso
DEPS := $(CC_SRC:.c=.d)

SUBDIRS := user/libc user/progs

all: $(ISO)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir 'CC=$(CC)' 'LD=$(LD)' 'AS=$(AS)' 'AR=$(AR)' 'NM=$(NM)'; \
	done

$(ISO): $(EXE)
	@$(MAKE) -C limine-binary
	@echo "[ISO] $<"
	@mkdir -p iso/boot/limine
	@cp $< iso/boot/
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

# link twice: first pass gets nm a final image to read symbol
# addresses from, second pass links those back in as .ksyms so panics
# can name functions. awk does the wrapping, no python involved.
$(EXE): $(OBJ)
	@echo "[LD] $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)
	$(NM) $@ | awk 'BEGIN { \
		print "#include <core/debug.h>"; \
		print "__attribute__((section(\".ksyms\"))) struct kern_symbol ksymtbl[] = {"; \
	} \
	NF == 3 && $$1 ~ /^[0-9A-Fa-f]+$$/ { n++; printf "(struct kern_symbol){ 0x%s, \"%s\" },\n", $$1, $$3 } \
	END { print "};"; print "usize nksyms = " n+0 ";" }' > ksyms.c
	$(CC) $(CCFLAGS) -c ksyms.c -o ksyms.o
	$(CC) $(LDFLAGS) ksyms.o $^ -o $@ $(LIBS)
	rm -f ksyms.o ksyms.d

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $<

run: all
	@echo "[QEMU]"
	$(QEMU) $(QFLAGS) $(QEMUFLAGS) -cdrom $(ISO)

clean:
	@echo "[CLEAN]"
	@rm -f $(OBJ) $(ISO) $(EXE) $(DEPS) ksyms.c ksyms.o ksyms.d
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

.PHONY: run clean all
-include $(DEPS)
