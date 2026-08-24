DRIVE ?= ../../drive.img
FILES := $(filter-out Makefile, $(wildcard *))

all: $(DRIVE) copy_etc

$(DRIVE):
	dd if=/dev/zero of=$@ bs=1M count=16
	mkfs.msdos -F 16 $@
	mmd -i $(DRIVE) ::/bin
	mmd -i $(DRIVE) ::/etc
	mmd -i $(DRIVE) ::/lib

copy_etc: $(FILES)
	for file in $(FILES); do \
		mcopy -voi $(DRIVE) "$$file" "::/etc/$$file"; \
	done

clean:

.PHONY: all copy_etc clean