CC := i686-elf-gcc
LD := i686-elf-ld
AS := nasm
XORRISO := xorriso
QEMU := qemu-system-i386

GRUB_BSTAGE2 := share/stage2_eltorito
GRUB_MENULST := share/menu.lst
GRUB_BOOTDIR := iso/boot
GRUB_GRUBDIR := $(GRUB_BOOTDIR)/grub

ASFLAGS      := -felf32
LDFLAGS      := -Tshare/link.ld -m32 -ffreestanding -O2 -nostdlib
LIBS         := -Llib -llai -lff -lgcc
CCFLAGS      := -m32 -nostdlib -fno-builtin -fno-stack-protector -Iinclude \
		        -nostartfiles -nodefaultlibs -ffreestanding -Wall -Wextra -g
XORRISOFLAGS := -as mkisofs -R -no-emul-boot -boot-load-size 4 -A os \
		        -input-charset utf8 -quiet -boot-info-table
QEMUFLAGS    :=

AS_SRC := $(shell find src -name '*.asm')
CC_SRC := $(shell find src -name '*.c')

OBJ := $(AS_SRC:.asm=.o) $(CC_SRC:.c=.o)
EXE := kern.elf
ISO := os.iso

SUBDIRS := user/libc user/progs

all: $(ISO)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

$(ISO): $(EXE)
	@echo "[ISO] $<"
	@mkdir -p $(GRUB_GRUBDIR)
	@cp $(GRUB_BSTAGE2) $(GRUB_GRUBDIR)/
	@cp $< $(GRUB_BOOTDIR)/
	@cp $(GRUB_MENULST) $(GRUB_GRUBDIR)/
	@$(XORRISO) $(XORRISOFLAGS) -b boot/grub/stage2_eltorito -o $@ iso
	@rm -rf $(GRUB_GRUBDIR)

$(EXE): $(OBJ)
	@echo "[LD] $@"
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $<

run: $(ISO)
	$(QEMU) $(QEMUFLAGS) \
		-M pc -boot d -m 1G \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-drive id=disk,file=drive.img,format=raw,if=none \
  		-device ide-hd,drive=disk,bus=ide.0,unit=0 \
		-cdrom $<

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
	rm -f $(OBJ) $(ISO) $(EXE)
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done

.PHONY: run clean all