FILES := $(filter-out Makefile, $(wildcard *))

all: copy_man

copy_man: $(FILES)
	debugfs -w -R "mkdir /share" $(DRIVE) 2>/dev/null || true
	debugfs -w -R "mkdir /share/man" $(DRIVE) 2>/dev/null || true
	for file in $(FILES); do \
		debugfs -w -R "rm /share/man/$$file" $(DRIVE) 2>/dev/null || true; \
		debugfs -w -R "write $$file /share/man/$$file" $(DRIVE); \
	done

clean:

.PHONY: all copy_man clean
