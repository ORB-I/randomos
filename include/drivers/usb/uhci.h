#pragma once

#include <core/std.h>

#define UHCI_USBCMD    0x00
#define UHCI_USBSTS    0x02
#define UHCI_USBINTR   0x04
#define UHCI_FRNUM     0x06
#define UHCI_FLBASEADD 0x08
#define UHCI_SOFMOD    0x0C
#define UHCI_PORTSC1   0x10
#define UHCI_PORTSC2   0x12

#define UHCI_CMD_RS   (1 << 0)
#define UHCI_CMD_HCRESET (1 << 1)
#define UHCI_CMD_GRESET  (1 << 2)
#define UHCI_CMD_MAXP    (1 << 7)

#define UHCI_STS_USBINT (1 << 0)
#define UHCI_STS_ERROR  (1 << 1)
#define UHCI_STS_RD     (1 << 2)
#define UHCI_STS_HSE    (1 << 3)
#define UHCI_STS_HCPE   (1 << 4)
#define UHCI_STS_HCHALT (1 << 5)

#define UHCI_PORT_CONN   (1 << 0)
#define UHCI_PORT_CONNC  (1 << 1)
#define UHCI_PORT_ENABLE (1 << 2)
#define UHCI_PORT_ENABLC (1 << 3)
#define UHCI_PORT_LINE_DMINUS (1 << 4)
#define UHCI_PORT_LINE_DPLUS  (1 << 5)
#define UHCI_PORT_RD     (1 << 6)
#define UHCI_PORT_LOWSPD (1 << 8)
#define UHCI_PORT_RESET  (1 << 9)
#define UHCI_PORT_SUSP   (1 << 12)
#define UHCI_PORTSC_CCS  (1 << 0)
#define UHCI_PORTSC_CSC  (1 << 1)

#define UHCI_TD_PTR_T (1 << 0)
#define UHCI_TD_PTR_Q (1 << 1)
#define UHCI_TD_PTR_VF (1 << 2)

#define UHCI_TD_CTRL_ACT  (1 << 23)
#define UHCI_TD_CTRL_IOC  (1 << 24)
#define UHCI_TD_CTRL_ISO  (1 << 25)
#define UHCI_TD_CTRL_LS   (1 << 26)
#define UHCI_TD_CTRL_CERR (3 << 27)
#define UHCI_TD_CTRL_SPD  (1 << 29)

#define UHCI_PID_SETUP 0x2D
#define UHCI_PID_IN    0x69
#define UHCI_PID_OUT   0xE1

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DESC_DEVICE    0x01
#define USB_DESC_CONFIG    0x02
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05

#define USB_CLASS_MASS_STORAGE 0x08
#define USB_SUBCLASS_SCSI 0x06
#define USB_PROTOIF_BULK_ONLY 0x50

#define USB_EP_TYPE_BULK 2

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u16 bcdUSB;
    u8  bDeviceClass;
    u8  bDeviceSubClass;
    u8  bDeviceProtocol;
    u8  bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8  iManufacturer;
    u8  iProduct;
    u8  iSerialNumber;
    u8  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct usb_interface_descriptor_t {
    u8  bLength;
    u8  bDescriptorType;
    u8  bInterfaceNumber;
    u8  bAlternateSetting;
    u8  bNumEndpoints;
    u8  bInterfaceClass;
    u8  bInterfaceSubClass;
    u8  bInterfaceProtocol;
    u8  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    u8  bLength;
    u8  bDescriptorType;
    u16 wTotalLength;
    u8  bNumInterfaces;
    u8  bConfigurationValue;
    u8  iConfiguration;
    u8  bmAttributes;
    u8  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

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
    u32* frame_list;
    uintptr_t frame_list_phys;
    uhci_qh_t* queue_head;
    uintptr_t queue_head_phys;
    bool exists;
    u8 port_in_use[16];
    u8 nports;
    u8 addrs[17]; // like port_in_use but for addresses, addrs[0] MUST be 0 always since
                  // that must be available for USB enumeration
} uhci_controller_t;

typedef struct {
    u8 req_type;
    u8 req;
    u16 val;
    u16 idx;
    u16 len;
} __attribute__((packed)) usb_device_request_t;

typedef struct {
    uhci_controller_t* ctrl;
    int addr;
} usb_dev_info_t;

int init_uhci();
int uhci_control_transfer(uhci_controller_t* hc, u8 dev_addr, bool low_speed, usb_device_request_t* req, void* data, u16 len);
int uhci_bulk_transfer(uhci_controller_t* hc, u8 dev_addr, u8 ep, void* data, u32 len, int in);
usize uhci_get_controllers(uhci_controller_t** ctrlrs);
int is_usb_devicetype(uhci_controller_t* hc, u8 dev_addr, bool low_speed, u8 cls, u8 proto);
void uhci_reset_port(uhci_controller_t* hc, int port);
int usb_set_configuration(uhci_controller_t* hc, u8 addr, u8 config_val);
int usb_get_device_descriptor(uhci_controller_t* hc, u8 addr, usb_device_descriptor_t* desc);
int usb_set_address(uhci_controller_t* hc, u8 old_addr, u8 new_addr);
int uhci_get_portcnt(uhci_controller_t* hc);
int uhci_portcon(uhci_controller_t* hc, uint8_t port);

#define UHCI_REG_NODEV -1
#define UHCI_REG_INUSE -2
#define UHCI_REG_NTYPE -3
#define UHCI_REG_NSADR -4
#define UHCI_REG_NSCFG -5
int uhci_regdev(uhci_controller_t* hc, int port, int low_speed, u8 class, u8 proto);
