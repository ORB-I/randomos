# randomOS — TODO

> Only open work goes here. Finished things move to [done.md](done.md).
> `// TODO:` comments in code count too, drop a reference to them here so
> they dont get forgotten.

## In progress

- [ ] Improve syscall.c more — @44tl

## Open

- [ ] Audit most of the code — @44tl
- [ ] Squash remaining -Wextra warnings in kernel + libc (list below in known issues) — @44tl

## Known issues

- [ ] usbmsd: init() builds a usb control request it never sends and
      wait_ready ignores both its args, looks like the mass storage
      reset flow was never finished (src/drivers/storage/usbmsd.c:160-180)
- [ ] libc create_fb checks `sz < 0` on a usize, so a failed syscall
      comes back as a huge size instead of being caught
      (user/libc/src/fb.c:8)
- [ ] fprintf smuggles the fd through a void* callback with int<->ptr
      casts, works by luck on x86_64 (user/libc/src/printf.c:946)
- [ ] uhci bulk transfer link pointer math mixes + and | without parens,
      intent unclear (src/drivers/usb/uhci.c:463)
- [ ] disk_ioctl hands every drive the same made up geometry: hardcoded
      512 byte sectors and a fixed sector count (src/drivers/storage/ff16host.c)
- [ ] storage stack pretends to be multi-drive but isnt: ff16host routes
      everything through one global, ahci/usbmsd ignore their drv param
- [ ] libc-only warnings: sign compares + dead null checks in liballoc,
      unused wday_names in strftime.c, unused wm_hit in progs/wm.c
- [ ] kernel-only warnings: unused params in laihost_unmap, vmm_remumap,
      liballoc sign compare (src/kern/mem/liballoc.c:58,516-521)

## Design debt

- [ ] SYS_SLEEP busy-spins inside the syscall, and since preemption is
      deferred in kernel mode the whole machine freezes until it returns
- [ ] dead processes are never reaped, pid slots are gone forever and
      NEWPROC starts failing after roughly 254 spawns
- [ ] SYS_EXIT throws its exit code away, WAIT can only ever report pids
- [ ] KILL lets any process kill any other, including init (pid 0)
- [ ] killing a parent leaves its children pointing at a dead ppid,
      there is no reparenting

## No owner yet

- [ ] Documentation for randomOS ( in near future )
