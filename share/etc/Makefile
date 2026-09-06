FILES := $(filter-out Makefile, $(wildcard *))

all: copy_etc

copy_etc: $(FILES)
	for file in $(FILES); do \
		debugfs -w -R "rm /etc/$$file" $(DRIVE); \
		debugfs -w -R "write $$file /etc/$$file" $(DRIVE); \
	done

clean:

.PHONY: all copy_etc clean