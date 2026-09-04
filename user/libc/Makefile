include ../../mk/tools.mk

ASFLAGS := -felf64
CCFLAGS := -m64 -nostdlib \
	-fno-builtin -Iinclude -nodefaultlibs \
    -ffreestanding -Wall -Wextra -g \
	-MMD -MP -mno-mmx -mno-sse -mno-sse2 \
	-mno-red-zone -msoft-float -mno-fp-ret-in-387 \
	-Werror 

CCFLAGS_SO := $(CCFLAGS) -fPIC
LDFLAGS_SO := -shared -Wl,-soname,/lib/libc.so -Wl,-shared -m64 -nostdlib -ffreestanding -nostartfiles \
			  -Wl,--allow-shlib-undefined -Wl,--hash-style=both

CFLAGS_LDSO := -m64 -nostdlib -fno-builtin -ffreestanding \
			   -fPIC -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
			   -msoft-float -mno-fp-ret-in-387 -fno-unwind-tables \
			   -fno-asynchronous-unwind-tables -fno-stack-check \
			   -fno-stack-protector -Iinclude
LDFLAGS_LDSO := -nostdlib -m elf_x86_64 -shared --hash-style=both

LIBC_SO := libc.so
LIBC := libc.a
CRT := crt0.o
LDSO := ld.so

LIBSRC := $(shell find . -name "*.c")
ASMSRC := $(shell find . -name "*.asm")

LIBSRC   := $(filter-out ./ldso/ldso.c, $(LIBSRC))
LIBA_SRC := $(filter-out ./src/crt.asm src/crt.asm, $(ASMSRC))
LDSOSRC := ./ldso/ldso.c

LIBOBJ := $(LIBSRC:.c=.o)
ASMOBJ := $(LIBA_SRC:.asm=.o)

LIB_SOOBJ := $(LIBSRC:.c=.so.o)
ASM_SOOBJ := $(LIBA_SRC:.asm=.so.o)

LDSO_OBJ := ldso/ldso.o

DEPS := $(LIBSRC:.c=.d)

ALL_LIB_OBJS := $(LIBOBJ) $(ASMOBJ)
ALL_SOFILE_OBJS := $(LIB_SOOBJ) $(ASM_SOOBJ)

CRTSRC := src/crt.asm

DRIVE ?= ../../drive.img

all: $(DRIVE) $(LIBC) $(CRT) $(LIBC_SO) copy

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

$(LIBC): $(ALL_LIB_OBJS)
	$(AR) rcs $@ $^

$(LIBC_SO): $(ALL_SOFILE_OBJS)
	$(CC) $(LDFLAGS_SO) $^ -o $@

$(LDSO): $(LDSO_OBJ)
	$(LD) $(LDFLAGS_LDSO)  $^ -o $@

copy: $(LIBC_SO) $(LDSO)
	debugfs -w -R "rm /lib/$(LIBC_SO)" $(DRIVE) || true
	debugfs -w -R "rm /lib/$(LDSO)" $(DRIVE) || true
	debugfs -w -R "write $(LIBC_SO) /lib/$(LIBC_SO)" $(DRIVE)
	debugfs -w -R "write $(LDSO) /lib/$(LDSO)" $(DRIVE)

$(CRT): $(CRTSRC)
	$(AS) $(ASFLAGS) $< -o $@

$(LDSO_OBJ): $(LDSOSRC)
	$(CC) $(CFLAGS_LDSO) -c $< -o $@

%.so.o: %.c
	$(CC) $(CCFLAGS_SO) -c $< -o $@
%.so.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@
%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@
clean:
	rm -f $(LIBC) $(CRT) $(LIBOBJ) $(ASMOBJ) $(DEPS) $(LIBC_SO) $(ALL_SOFILE_OBJS) $(LDSO) $(LDSO_OBJ)

.PHONY: all copy clean
.SECONDARY: $(ALL_SOFILE_OBJS) $(ALL_LIB_OBJS)

-include $(DEPS)
