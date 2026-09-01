DRIVE ?= ../../drive.img
FILES := $(filter-out Makefile, $(wildcard *))

all: $(DRIVE) copy_man

$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=100
	mkfs.ext2 $@
	debugfs -w -R "mkdir /bin" $@
	debugfs -w -R "mkdir /etc" $@
	debugfs -w -R "mkdir /lib" $@
	debugfs -w -R "mkdir /share" $@
	debugfs -w -R "mkdir /share/man" $@

copy_man: $(FILES)
	debugfs -w -R "mkdir /share" $(DRIVE) 2>/dev/null || true
	debugfs -w -R "mkdir /share/man" $(DRIVE) 2>/dev/null || true
	for file in $(FILES); do \
		debugfs -w -R "rm /share/man/$$file" $(DRIVE) 2>/dev/null || true; \
		debugfs -w -R "write $$file /share/man/$$file" $(DRIVE); \
	done

clean:

.PHONY: all copy_man clean
