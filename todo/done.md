# randomOS - DONE

Completed work, grouped per developer. Format: `- [x] what - @who`.

Own your section with ## @name

Newest entries go at the bottom of each section.

## @eskridd

- [x] Switch from 8259 PIC to IOAPIC
- [x] Write UHCI or xHCI driver
- [x] Implement USB HID keyboard driver
- [x] Create userspace wm
- [x] Test userspace framebuffer stuff
- [x] Add SOFILE support and dynamic ELFs
- [x] Fix USB HID driver is_usb_devicetype failing to get configuration descriptor
- [x] Rewrite syscall stuff so that it doesn't get interrupted by the scheduler mid-syscall
- [x] Rewrite sys_exit to return to the process in the ppid instead of to a now non-existent kernel function
- [x] Probably fix a ton of bugs in the scheduler
- [x] Implement AHCI driver
- [x] Implement USBMSD driver
- [x] Add WAIT syscall so parent processes can wait till a child dies
- [x] Add KILL syscall so that processes can be killed
- [x] Improve syscall safety, pointer validation, reparenting, and kill protections
- [x] Fix USBMSD Bulk-Only reset flow and wait_ready initialization order
- [x] Make SYS_SLEEP non-busy-spinning via timer wake deadlines and scheduler idle halting
- [x] Fix user mmap memory allocation range, sys_newproc address space isolation, and process execution
- [x] Fix libc create_fb return type check, fprintf fd cast, and UHCI link operator precedence
- [x] Fix storage stack drive parameter routing and disk_ioctl geometry reporting in ff16host.c
- [x] Squash remaining -Wextra warnings across kernel and libc (liballoc, strftime, laihost, wm)
- [x] Implement Intel E1000 Gigabit Ethernet network driver
- [x] Implement multi-user support (uid, gid, euid, egid syscalls, credentials inheritance, and userland id/whoami)

## @Wdboyes13

- [x] Add USBHID and PS/2 mouse support
- [x] Make it so ET_EXECs get DT_NEEDED tags processed
- [x] Fix USBHID Mouse hanging
- [x] dead processes are never reaped, pid slots are gone forever and
      NEWPROC starts failing after roughly 254 spawns
- [x] SYS_EXIT throws its exit code away, WAIT can only ever report pids
- [x] add getpid syscall
- [x] environment variables
- [x] switch to PATH
- [x] refactor disk to use directories for stuff (/bin, /lib, /etc)