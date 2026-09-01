DRIVE ?= ../../drive.img
FILES := $(filter-out Makefile, $(wildcard *))

all: $(DRIVE) copy_etc

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

copy_etc: $(FILES)
	for file in $(FILES); do \
		debugfs -w -R "rm /etc/$$file" $(DRIVE); \
		debugfs -w -R "write $$file /etc/$$file" $(DRIVE); \
	done

clean:

.PHONY: all copy_etc clean