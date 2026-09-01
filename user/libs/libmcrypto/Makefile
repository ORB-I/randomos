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

DRIVE ?= ../../../drive.img

all: $(DRIVE) $(LIB) copy

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

$(LIB): $(LIBOBJ)
	$(CC) $(LDFLAGS) $^ -o $@

copy: $(LIB)
	debugfs -w -R "rm /lib/$(LIB)" $(DRIVE)
	debugfs -w -R "write $(LIB) /lib/$(LIB)" $(DRIVE)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@
clean:
	rm -f $(LIB) $(LIBOBJ) $(DEPS)

.PHONY: all copy clean
-include $(DEPS)