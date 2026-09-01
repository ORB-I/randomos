# RandomOS

RandomOS is a 64-bit x86_64 operating system written from scratch in C and assembly. It features a custom monolithic kernel, SMP multicore scheduling, VirtIO and legacy driver stacks, an EXT2 filesystem driver with variable block size support, custom libc with dynamic ELF linking, a graphical window manager, and built-in user utilities.

## Features

### Kernel and Core Architecture
- Target: x86_64 (64-bit long mode), booted via Limine bootloader.
- Multiprocessing: Multicore SMP support with APIC/IOAPIC interrupt controller initialization.
- Memory: Physical frame allocator, virtual memory manager with high-half mapping, and userland page allocation via mmap.
- Scheduling: Preemptive round-robin scheduler with sleep deadlines, process wait/kill syscalls, and user credential tracking (UID, GID, EUID, EGID).
- ACPI: Power management and hardware discovery using the LAI interpreter library.

### Storage and Filesystem
- Storage Drivers: VirtIO Block (virtio-blk), AHCI SATA, IDE/ATA, and USB Mass Storage (USB-MSD).
- Filesystem: Native EXT2 filesystem driver supporting read, write, truncation, directory navigation, and variable block sizes (1024B, 2048B, 4096B).

### Hardware and Networking
- Network: Intel E1000 Gigabit Ethernet and VirtIO Net (virtio-net) integrated with the LwIP TCP/IP network stack.
- Input: USB UHCI controller with USB-HID keyboard/mouse and PS/2 keyboard/mouse drivers.
- Entropy: Hardware random number generation via VirtIO RNG (virtio-rng) and RDRAND with TSC fallback.
- Time: Real-time clock (RTC), HPET, PIT, and TSC calibration.

### Userland and Graphics
- Libc: Freestanding C runtime library with memory management, formatted I/O, string operations, math helpers, and system call wrappers.
- Executables: Dynamic ELF loader supporting shared object files (.so) and runtime symbol resolution.
- Security: User authentication with Argon2id password hashing stored in /etc/passwd.
- Documentation: Builtin manual page viewer (man) and documentation files in /share/man/.
- Desktop Environment: Graphical window manager (wm) with double-buffered compositing, mouse dragging, application launcher, taskbar, digital clock, and built-in tools (Terminal, File Manager, System Monitor, Notes, Calculator).

## Building from Source

### Prerequisites
Make sure the following tools are installed on your host system:
- clang and ld.lld (LLVM toolchain)
- nasm
- xorriso
- e2fsprogs (for mkfs.ext2 and debugfs)
- python3
- qemu-system-x86_64 (to run the system)

### Build Commands
Compile the complete operating system, libc, user binaries, ext2 disk image, and bootable ISO:

```bash
make -j$(nproc)
```

Clean build artifacts:

```bash
make clean
```

## Running in QEMU

Run the bootable ISO with full device emulation (VirtIO storage, networking, RNG, and USB):

```bash
make run
```

Run in headless mode for CI or automated testing:

```bash
make run-headless
```

## Project Links and Documentation

- GitHub Repository: https://github.com/Wdboyes13/randomos
- Project Website: https://wdboyes13.github.io/randomos/
- Online Documentation: https://wdboyes13.github.io/randomos/docs.html
- Features List: https://wdboyes13.github.io/randomos/features.html
- Project Licensing: https://wdboyes13.github.io/randomos/license.html

## License

RandomOS is released under the MIT License. See [LICENSE.md](LICENSE.md) for full license terms and third-party component acknowledgments.
