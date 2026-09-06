CC := clang --target=x86_64-elf
LD := ld.lld

CFLAGS := -m64 -nostdlib -fno-builtin -ffreestanding -fno-pie -g -MMD \
	-MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -msoft-float \
	-mno-fp-ret-in-387 -Wall -Wextra -O2 -std=c17 \
	-fwrapv -ftrivial-auto-var-init=zero -fno-common \
	-ffunction-sections -fdata-sections -Wall -I. -I./include \
	-I./x86 -I./asm -I./disasm -I../libc/include -I./output -I./output \
	-I../libs/zlib -MMD -MP

LDFLAGS := -m elf_x86_64 --no-pie --dynamic-linker=/lib/ld.so --allow-shlib-undefined --hash-style=both
LDLIBS  := -L../libc -L../libs/zlib -lc -lz ../libc/crt0.o

SRCS := $(wildcard asm/*.c) \
		$(wildcard common/*.c) \
		$(wildcard nasmlib/*.c) \
		$(wildcard output/*.c) \
		$(wilcard stdlib/*.c) \
		macros/macros.c x86/insnsa.c \
		x86/insnsb.c x86/insnsn.c \
		x86/regs.c x86/regvals.c \
		x86/regflags.c x86/iflag.c

OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
OUT := nasm

$(OUT): $(OBJS)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
clean:
	rm -f $(OBJS) $(DEPS) $(OUT)

-include $(DEPS)