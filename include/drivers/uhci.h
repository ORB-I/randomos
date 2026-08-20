#pragma once

#include <core/std.h>

#define UHCI_USBCMD 0x00
#define UHCI_USBSTS 0x02
#define UHCI_USBINTR 0x04
#define UHCI_FRNUM 0x06
#define UHCI_FLBASEADD 0x08
#define UHCI_SOFMOD 0x0C
#define UHCI_PORTSC1 0x10
#define UHCI_PORTSC2 0x12

#define UHCI_CMD_RS (1 << 0)
#define UHCI_CMD_HCRESET (1 << 1)
#define UHCI_CMD_GRESET (1 << 2)
#define UHCI_CMD_MAXP (1 << 7)

#define UHCI_STS_USBINT (1 << 0)
#define UHCI_STS_ERROR (1 << 1)
#define UHCI_STS_RD (1 << 2)
#define UHCI_STS_HSE (1 << 3)
#define UHCI_STS_HCPE (1 << 4)
#define UHCI_STS_HCHALT (1 << 5)

#define UHCI_PORT_CONN (1 << 0)
#define UHCI_PORT_CONNC (1 << 1)
#define UHCI_PORT_ENABLE (1 << 2)
#define UHCI_PORT_ENABLC (1 << 3)
#define UHCI_PORT_LINE_DMINUS (1 << 4)
#define UHCI_PORT_LINE_DPLUS (1 << 5)
#define UHCI_PORT_RD (1 << 6)
#define UHCI_PORT_LOWSPD (1 << 8)
#define UHCI_PORT_RESET (1 << 9)
#define UHCI_PORT_SUSP (1 << 12)

#define UHCI_TD_PTR_T (1 << 0)
#define UHCI_TD_PTR_Q (1 << 1)
#define UHCI_TD_PTR_VF (1 << 2)

#define UHCI_TD_CTRL_ACT (1 << 23)
#define UHCI_TD_CTRL_IOC (1 << 24)
#define UHCI_TD_CTRL_ISO (1 << 25)
#define UHCI_TD_CTRL_LS (1 << 26)
#define UHCI_TD_CTRL_CERR (3 << 27)
#define UHCI_TD_CTRL_SPD (1 << 29)

#define UHCI_PID_SETUP 0x2D
#define UHCI_PID_IN 0x69
#define UHCI_PID_OUT 0xE1

typedef struct uhci_td {
  u32 link;
  u32 ctrl;
  u32 token;
  u32 buffer;
} __attribute__((packed, aligned(16))) uhci_td_t;

typedef struct uhci_qh {
  u32 head;
  u32 element;
  u32 resv[2];
} __attribute__((aligned(16), packed)) uhci_qh_t;

typedef struct {
  u8 bus;
  u8 slot;
  u8 fn;
  u16 io_base;
  u32 *frame_list;
  uintptr_t frame_list_phys;
  uhci_qh_t *queue_head;
  uintptr_t queue_head_phys;
  bool exists;
} uhci_controller_t;

typedef struct {
  u8 req_type;
  u8 req;
  u16 val;
  u16 idx;
  u16 len;
} __attribute__((packed)) usb_device_request_t;

typedef struct {
  u8 modifiers;
  u8 reserved;
  u8 keys[6];
} __attribute__((packed)) usb_hid_kbd_report_t;

int init_uhci();
int uhci_control_transfer(uhci_controller_t *hc, u8 dev_addr, bool low_speed,
                          usb_device_request_t *req, void *data, u16 len);
int usb_hid_kbd_init();
void usb_hid_kbd_poll();
