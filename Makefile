# toolchain discovery lives in mk/tools.mk so the kernel, libc and
# progs builds can never drift apart. dont inline it back here.
include mk/tools.mk

ASFLAGS      := -Iinclude -felf64
# linking runs ld.lld directly: clang hands *-elf targets to whatever
# host gcc it can find, and every gcc interprets linker flags its own
# way. going straight to lld removes that whole lottery.
LDFLAGS      := -m elf_x86_64 -T share/link.ld --no-pie -O0 -nostdlib -no-pie
# no -lgcc on purpose: the kernel doesnt need its builtins and pure
# llvm machines (mac) dont ship it anyway
LIBS         := -Llib -llai -lff -lflanterm -llwip
CCFLAGS      := -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
				-m64 -nostdlib -fno-builtin -fno-stack-protector -fno-pie -Iinclude \
		        -nodefaultlibs -ffreestanding -Wall -Wextra -g \
		        -MMD -MP -O0
XORRISOFLAGS := -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        		-efi-boot-part --efi-boot-image --protective-msdos-label
# kvm when available with tcg fallback for speed. run-headless appends
# -display none and moves serial into qemu.log for quiet CI-style runs.
QFLAGS       := -M pc -cpu qemu64,+rdrand -boot d -smp 2 -m 1G -serial stdio \
				-accel kvm -accel tcg \
				-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
				-drive id=disk,file=drive.img,format=raw,if=none \
  				-device ide-hd,drive=disk,bus=ide.0,unit=0 \
				-device piix3-usb-uhci,id=uhci \
				-device usb-kbd,bus=uhci.0,port=1 \
				-device usb-mouse,bus=uhci.0,port=2 \
				-netdev user,id=net0 -device e1000,netdev=net0 \
				-monitor unix:/tmp/qemu-monitor.sock,server=on,wait=off \
				-qmp unix:/tmp/qemu-qmp.sock,server=on,wait=off
QFLAGS_HEADLESS := -display none -serial file:qemu.log

AS_SRC := $(shell find src -name '*.asm')
CC_SRC := $(shell find src -name '*.c')

# lwip is built as a plain static lib: core + ipv4/ipv6 + the ethernet
# netif. NO_SYS=1 in lwipopts.h so api/ and apps/ are not compiled.
LWIP_DIR  := vendor/lwip-2.2.1
LWIP_SRC  := $(shell find $(LWIP_DIR)/src/core -name '*.c') \
             $(shell find $(LWIP_DIR)/src/api -name '*.c') \
			 $(shell find $(LWIP_DIR)/src/netif -name '*.c')

LWIP_OBJ  := $(LWIP_SRC:.c=.o)
LWIP_DEPS := $(LWIP_SRC:.c=.d)

OBJ  := $(AS_SRC:.asm=.o) $(CC_SRC:.c=.o)
EXE  := kern.elf
ISO  := os.iso
DEPS := $(CC_SRC:.c=.d)

SUBDIRS := user/libs/libmcrypto user/libc user/progs share/etc

all: subdirs $(ISO)

subdirs:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir 'CC=$(CC)' 'LD=$(LD)' 'AS=$(AS)' 'AR=$(AR)' 'NM=$(NM)' || exit 1; \
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
$(EXE): $(OBJ) lib/liblwip.a
	@echo "[LD] $@"
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)
	python3 mkksyms.py $(NM) $@
	$(CC) $(CCFLAGS) -c ksyms.c -o ksyms.o
	$(LD) $(LDFLAGS) ksyms.o $(OBJ) lib/liblwip.a -o $@ $(LIBS)
	@rm -f ksyms.o ksyms.d

lib/liblwip.a: $(LWIP_OBJ)
	@echo "[AR] $@"
	$(AR) rcs $@ $^

# lwip headers live in vendor/lwip-2.2.1/src/include; the mirror under
# include/lwip is only for kernel-side <lwip/...> lookups. -w because
# upstream code does not care about our -Wall -Wextra.
$(LWIP_OBJ): $(LWIP_DIR)/%.o: $(LWIP_DIR)/%.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -I$(LWIP_DIR)/src/include -w -c $< -o $@


%.o: %.c
	@echo "[CC] $<"
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $<

run: all
	@echo "[QEMU]"
	$(QEMU) $(QFLAGS) $(QEMUFLAGS) -cdrom $(ISO)

# same vm, no window, serial captured into qemu.log
run-headless: all
	@echo "[QEMU headless]"
	$(QEMU) $(QFLAGS) $(QFLAGS_HEADLESS) $(QEMUFLAGS) -cdrom $(ISO)

clean:
	@echo "[CLEAN]"
	@rm -f $(OBJ) $(ISO) $(EXE) $(DEPS) ksyms.c ksyms.o ksyms.d lib/liblwip.a $(LWIP_OBJ) $(LWIP_DEPS)
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
