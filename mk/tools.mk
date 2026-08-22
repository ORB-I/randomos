# shared toolchain resolution, included by every makefile here so the
# kernel, libc and progs builds can never drift apart
#
# plain tool names were fine until they werent: on some machines the
# llvm/nasm binaries sit outside PATH and builds die with "No such
# file". so every tool falls back to a shallow sweep of common install
# spots instead. nothing machine specific lives in this file; if your
# setup still isnt found, run `TOOLCHAIN=/your/toolchains/bin make`
# or override single tools like `make CC=/somewhere/clang`

TOOLCHAIN ?=
locate = $(shell command -v $(1) 2>/dev/null || { test -n "$(TOOLCHAIN)" && echo "$(TOOLCHAIN)/$(1)"; } || find ~/opt ~/.local /usr/local /opt/homebrew -maxdepth 4 -type f -name '$(1)' 2>/dev/null | head -n1)

CC := $(call locate,clang) --target=x86_64-elf
LD := $(call locate,ld.lld)
AS := $(call locate,nasm)
AR := $(call locate,llvm-ar) --format=default
NM := $(call locate,llvm-nm)
XORRISO := $(call locate,xorriso)
QEMU := qemu-system-x86_64
