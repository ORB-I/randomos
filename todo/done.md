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

## @Wdboyes13

- [x] Add USBHID and PS/2 mouse support
- [x] Make it so ET_EXECs get DT_NEEDED tags processed
- [x] Fix USBHID Mouse hanging
