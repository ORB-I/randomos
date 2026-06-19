CC := x86_64-elf-gcc
LD := x86_64-elf-ld
AS := nasm
XORRISO := xorriso
QEMU := qemu-system-x86_64

ASFLAGS      := -felf64
LDFLAGS      := -Tshare/link.ld -m64 -ffreestanding -O2 -nostdlib
LIBS         := #-Llib -llai -lff -lgcc
CCFLAGS      := -mcmodel=kernel -mno-red-zone -m64 -nostdlib -fno-builtin \
				-fno-stack-protector -Iinclude \
		        -nostartfiles -nodefaultlibs -ffreestanding -Wall -Wextra -g \
				-MMD -MP
XORRISOFLAGS := -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        		-efi-boot-part --efi-boot-image --protective-msdos-label
QFLAGS       := -M pc -boot d -m 1G \
				-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
				-drive id=disk,file=drive.img,format=raw,if=none \
  				-device ide-hd,drive=disk,bus=ide.0,unit=0

AS_SRC := $(shell find src -name '*.asm')
CC_SRC := $(shell find src -name '*.c')

OBJ := $(AS_SRC:.asm=.o) $(CC_SRC:.c=.o)
EXE := kern.elf
ISO := os.iso
DEPS := $(CC_SRC:.c=.d)

SUBDIRS := user/libc user/progs

all: $(ISO)
#	@for dir in $(SUBDIRS); do \
#		$(MAKE) -C $$dir; \
#	done

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

$(EXE): $(OBJ)
	@echo "[LD] $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $<

run: all
	@echo "[QEMU]"
	$(QEMU) $(QFLAGS) $(QEMUFLAGS) -cdrom $(ISO)

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

clean:
	@echo "[CLEAN]"
	@rm -f $(OBJ) $(ISO) $(EXE) $(DEPS)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done

.PHONY: run clean all
-include $(DEPS)