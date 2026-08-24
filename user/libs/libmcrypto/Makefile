include ../../../mk/tools.mk

CCFLAGS := -m64 -nostdlib -fno-builtin \
    -fno-stack-protector -Iinclude \
    -nostartfiles -nodefaultlibs \
    -ffreestanding -Wall -Wextra -g \
	-MMD -MP -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
	-Wl,--allow-shlib-undefined -fPIC
LDFLAGS := -shared -Wl,-soname,/lib/libmcrypto.so -Wl,-shared -m64 -nostdlib -ffreestanding

LIB := libmcrypto.so
LIBSRC := mcrypto.c
LIBOBJ := $(LIBSRC:.c=.o)

DEPS := $(LIBSRC:.c=.d)

all: $(DRIVE) $(LIB)

$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16
	mkfs.msdos -F 16 $@
	mmd -i $(DRIVE) ::/bin
	mmd -i $(DRIVE) ::/etc
	mmd -i $(DRIVE) ::/lib

$(LIB): $(LIBOBJ)
	$(CC) $(LDFLAGS) $^ -o $@
	mcopy -voi $(DRIVE) -s $@ ::/lib/$@
%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
clean:
	rm -f $(LIB) $(LIBOBJ) $(DEPS)

.PHONY: all clean
-include $(DEPS)