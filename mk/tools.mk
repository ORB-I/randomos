# shared toolchain, included by every makefile here
#
# tools are looked up through PATH like every other unix program
# (IEEE Std 1003.1, PATH). if one isnt there you will get told exactly
# which, and the fix belongs to the environment, not this file:
#
#   linux:   sudo apt install clang lld llvm nasm mtools xorriso
#   mac:     brew install llvm lld nasm mtools xorriso
#            then put the bin dirs on PATH (see .github/workflows/build.yml)
#   custom:  make CC=/somewhere/clang LD=/somewhere/ld.lld ...

CC := clang --target=x86_64-elf
LD := ld.lld
AS := nasm
AR := llvm-ar --format=default
NM := llvm-nm
XORRISO := xorriso
QEMU := qemu-system-x86_64

# same PATH search command -v always did, just up front and loud,
# instead of "No such file" halfway through a build
NEEDED := clang ld.lld nasm llvm-ar llvm-nm xorriso
MISSING := $(foreach t,$(NEEDED),$(if $(shell command -v $(t) 2>/dev/null),,$(t)))
ifneq ($(strip $(MISSING)),)
$(error not on PATH: $(MISSING) - see the header above for install hints)
endif
