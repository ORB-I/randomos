# shared toolchain resolution, included by every makefile here so the
# kernel, libc and progs builds can never drift apart
#
# plain tool names were fine until they werent: on some machines the
# llvm/nasm binaries sit outside PATH. so: normal PATH lookup first,
# then a shallow sweep of common install spots as a safety net.
# override anything per-invocation with e.g. `make CC=/somewhere/clang`

locate = $(shell command -v $(1) 2>/dev/null || find ~/opt ~/.local /usr/local /opt/homebrew -maxdepth 4 -type f -name '$(1)' 2>/dev/null | head -n 1)

CC := $(call locate,clang) --target=x86_64-elf
AS := $(call locate,nasm)
AR := $(call locate,llvm-ar) --format=default
NM := $(call locate,llvm-nm)
XORRISO := $(call locate,xorriso)
QEMU := qemu-system-x86_64

# apple-built llvm bottles dont ship the ld.lld symlink, only the
# universal lld driver, which speaks gnu when told -flavor gnu
LD := $(call locate,ld.lld)
ifeq ($(strip $(LD)),)
LLD_UNIVERSAL := $(call locate,lld)
ifeq ($(strip $(LLD_UNIVERSAL)),)
$(error no linker found: need ld.lld or lld from llvm)
endif
LD := $(LLD_UNIVERSAL) -flavor gnu
endif

# fail loudly here instead of three steps later with a confusing
# "No such file", an empty CC/LD once produced recipes like `m
# elf_x86_64 ...` because make ate the leading dash as a flag
TOOLS_MISSING := $(strip $(foreach t,CC LD AS AR NM XORRISO,$(if $($t),,$t)))
ifneq ($(strip $(TOOLS_MISSING)),)
$(error could not locate: $(TOOLS_MISSING) - install llvm/nasm/xorriso or point TOOLCHAIN/make vars at them)
endif
