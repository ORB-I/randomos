#include <core/std.h>
#include <core/asmh.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <drivers/pci.h>
#include <drivers/uhci.h>
#include <drivers/tsc.h>
#include <core/printf.h>
#include <drivers/kbd.h>

extern u8 kbd_raw_sc;
extern bool kbd_raw_ready;
#include <lib/string.h>

#define MAX_UHCI_CONTROLLERS 4
static uhci_controller_t controllers[MAX_UHCI_CONTROLLERS];
static usize num_controllers = 0;

static const char hid_scancode_map[256] = {
    0, 0, 0, 0,
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '\n', 0x1B, '\b', '\t', ' ', '-', '=', '[', ']', '\\',
    0, ';', '\'', '`', ',', '.', '/', 0
};

static const char hid_scancode_map_shift[256] = {
    0, 0, 0, 0,
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '\n', 0x1B, '\b', '\t', ' ', '_', '+', '{', '}', '|',
    0, ':', '"', '~', '<', '>', '?', 0
};

static usb_hid_kbd_report_t prev_report = {0};

static const u8 hid_to_ps2[256] = {
    [0x00]=0x00,
    [0x01]=0x00,
    [0x02]=0x00,
    [0x03]=0x00,
    [0x04]=0x1e,
    [0x05]=0x30,
    [0x06]=0x2e,
    [0x07]=0x20,
    [0x08]=0x12,
    [0x09]=0x21,
    [0x0a]=0x22,
    [0x0b]=0x23,
    [0x0c]=0x17,
    [0x0d]=0x24,
    [0x0e]=0x25,
    [0x0f]=0x26,
    [0x10]=0x32,
    [0x11]=0x31,
    [0x12]=0x18,
    [0x13]=0x19,
    [0x14]=0x10,
    [0x15]=0x13,
    [0x16]=0x1f,
    [0x17]=0x14,
    [0x18]=0x16,
    [0x19]=0x2f,
    [0x1a]=0x11,
    [0x1b]=0x2d,
    [0x1c]=0x2c,
    [0x1d]=0x15,
    [0x1e]=0x02,
    [0x1f]=0x03,
    [0x20]=0x04,
    [0x21]=0x05,
    [0x22]=0x06,
    [0x23]=0x07,
    [0x24]=0x08,
    [0x25]=0x09,
    [0x26]=0x0a,
    [0x27]=0x0b,
    [0x28]=0x1c,
    [0x29]=0x01,
    [0x2a]=0x0e,
    [0x2b]=0x0f,
    [0x2c]=0x39,
    [0x2d]=0x0c,
    [0x2e]=0x0d,
    [0x2f]=0x1a,
    [0x30]=0x1b,
    [0x31]=0x2b,
    [0x32]=0x00,
    [0x33]=0x27,
    [0x34]=0x28,
    [0x35]=0x29,
    [0x36]=0x33,
    [0x37]=0x34,
    [0x38]=0x35,
    [0x39]=0x3a,
    [0x3a]=0x3b,
    [0x3b]=0x3c,
    [0x3c]=0x3d,
    [0x3d]=0x3e,
    [0x3e]=0x3f,
    [0x3f]=0x40,
    [0x40]=0x41,
    [0x41]=0x42,
    [0x42]=0x43,
    [0x43]=0x44,
    [0x44]=0x45,
    [0x45]=0x46,
    [0x46]=0x54,
    [0x47]=0x47,
    [0x48]=0x48,
    [0x49]=0xd2,
    [0x4a]=0xc7,
    [0x4b]=0xc9,
    [0x4c]=0xd3,
    [0x4d]=0xcf,
    [0x4e]=0x4b,
    [0x4f]=0x50,
    [0x50]=0x4d,
    [0x51]=0x48,
    [0x52]=0x00,
    [0x53]=0x45,
    [0x54]=0x54,
    [0x55]=0x37,
    [0x56]=0x4a,
    [0x57]=0x4e,
    [0x58]=0x58,
    [0x59]=0x4f,
    [0x5a]=0x50,
    [0x5b]=0x51,
    [0x5c]=0x4b,
    [0x5d]=0x4c,
    [0x5e]=0x4d,
    [0x5f]=0x47,
    [0x60]=0x48,
    [0x61]=0x49,
    [0x62]=0x52,
    [0x63]=0x53,
    [0x64]=0x56,
    [0x65]=0x00,
    [0x66]=0x00,
    [0x67]=0x00,
    [0x68]=0x64,
    [0x69]=0x65,
    [0x6a]=0x66,
    [0x6b]=0x67,
    [0x6c]=0x68,
    [0x6d]=0x69,
    [0x6e]=0x6a,
    [0x6f]=0x6b,
    [0x70]=0x6c,
    [0x71]=0x6d,
    [0x72]=0x6e,
    [0x73]=0x6f,
    [0x74]=0x70,
    [0x75]=0x71,
    [0x76]=0x72,
    [0x77]=0x73,
    [0x78]=0x74,
    [0x79]=0x75,
    [0x7a]=0x76,
    [0x7b]=0x77,
    [0x7c]=0x78,
    [0x7d]=0x79,
    [0x7e]=0x7a,
    [0x7f]=0x7b,
    [0x80]=0x7c,
    [0x81]=0x7d,
    [0x82]=0x7e,
    [0x83]=0x7f,
    [0x84]=0x80,
    [0x85]=0x81,
    [0x86]=0x82,
    [0x87]=0x83,
    [0x88]=0x84,
    [0x89]=0x85,
    [0x8a]=0x86,
    [0x8b]=0x87,
    [0x8c]=0x88,
    [0x8d]=0x89,
    [0x8e]=0x8a,
    [0x8f]=0x8b,
    [0x90]=0x8c,
    [0x91]=0x8d,
    [0x92]=0x8e,
    [0x93]=0x8f,
    [0x94]=0x90,
    [0x95]=0x91,
    [0x96]=0x92,
    [0x97]=0x93,
    [0x98]=0x94,
    [0x99]=0x95,
    [0x9a]=0x96,
    [0x9b]=0x97,
    [0x9c]=0x98,
    [0x9d]=0x99,
    [0x9e]=0x9a,
    [0x9f]=0x9b,
    [0xa0]=0x9c,
    [0xa1]=0x9d,
    [0xa2]=0x9e,
    [0xa3]=0x9f,
    [0xa4]=0xa0,
    [0xa5]=0xa1,
    [0xa6]=0xa2,
    [0xa7]=0xa3,
    [0xa8]=0xa4,
    [0xa9]=0xa5,
    [0xaa]=0xa6,
    [0xab]=0xa7,
    [0xac]=0xa8,
    [0xad]=0xa9,
    [0xae]=0xaa,
    [0xaf]=0xab,
    [0xb0]=0xac,
    [0xb1]=0xad,
    [0xb2]=0xae,
    [0xb3]=0xaf,
    [0xb4]=0xb0,
    [0xb5]=0xb1,
    [0xb6]=0xb2,
    [0xb7]=0xb3,
    [0xb8]=0xb4,
    [0xb9]=0xb5,
    [0xba]=0xb6,
    [0xbb]=0xb7,
    [0xbc]=0xb8,
    [0xbd]=0xb9,
    [0xbe]=0xba,
    [0xbf]=0xbb,
    [0xc0]=0xbc,
    [0xc1]=0xbd,
    [0xc2]=0xbe,
    [0xc3]=0xbf,
    [0xc4]=0xc0,
    [0xc5]=0xc1,
    [0xc6]=0xc2,
    [0xc7]=0xc3,
    [0xc8]=0xc4,
    [0xc9]=0xc5,
    [0xca]=0xc6,
    [0xcb]=0xc7,
    [0xcc]=0xc8,
    [0xcd]=0xc9,
    [0xce]=0xca,
    [0xcf]=0xcb,
    [0xd0]=0xcc,
    [0xd1]=0xcd,
    [0xd2]=0xce,
    [0xd3]=0xcf,
    [0xd4]=0xd0,
    [0xd5]=0xd1,
    [0xd6]=0xd2,
    [0xd7]=0xd3,
    [0xd8]=0xd4,
    [0xd9]=0xd5,
    [0xda]=0xd6,
    [0xdb]=0xd7,
    [0xdc]=0xd8,
    [0xdd]=0xd9,
    [0xde]=0xda,
    [0xdf]=0xdb,
    [0xe0]=0xdc,
    [0xe1]=0xdd,
    [0xe2]=0xde,
    [0xe3]=0xdf,
    [0xe4]=0xe0,
    [0xe5]=0xe1,
    [0xe6]=0xe2,
    [0xe7]=0xe3,
    [0xe8]=0xe4,
    [0xe9]=0xe5,
    [0xea]=0xe6,
    [0xeb]=0xe7,
    [0xec]=0xe8,
    [0xed]=0xe9,
    [0xee]=0xea,
    [0xef]=0xeb,
    [0xf0]=0xec,
    [0xf1]=0xed,
    [0xf2]=0xee,
    [0xf3]=0xef,
    [0xf4]=0xf0,
    [0xf5]=0xf1,
    [0xf6]=0xf2,
    [0xf7]=0xf3,
    [0xf8]=0xf4,
    [0xf9]=0xf5,
    [0xfa]=0xf6,
    [0xfb]=0xf7,
    [0xfc]=0xf8,
    [0xfd]=0xf9,
    [0xfe]=0xfa,
    [0xff]=0xfb,
};


static inline u16 uhci_inw(uhci_controller_t* hc, u16 reg) {
    return inw(hc->io_base + reg);
}

static inline void uhci_outw(uhci_controller_t* hc, u16 reg, u16 val) {
    outw(hc->io_base + reg, val);
}

static inline void uhci_outl(uhci_controller_t* hc, u16 reg, u32 val) {
    outl(hc->io_base + reg, val);
}

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIGURATION    0x02

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

typedef struct {
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

int usb_set_address(uhci_controller_t* hc, u8 old_addr, u8 new_addr) {
    usb_device_request_t req = {
        .req_type = 0x00,
        .req      = USB_REQ_SET_ADDRESS,
        .val      = new_addr,
        .idx      = 0,
        .len      = 0
    };
    int ret = uhci_control_transfer(hc, old_addr, true, &req, NULL, 0);
    tsc_sleep(10);
    return ret;
}

int usb_get_device_descriptor(uhci_controller_t* hc, u8 addr, usb_device_descriptor_t* desc) {
    usb_device_request_t req = {
        .req_type = 0x80,
        .req      = USB_REQ_GET_DESCRIPTOR,
        .val      = (USB_DESC_DEVICE << 8) | 0,
        .idx      = 0,
        .len      = sizeof(usb_device_descriptor_t)
    };
    return uhci_control_transfer(hc, addr, true, &req, desc, sizeof(usb_device_descriptor_t));
}

int usb_set_configuration(uhci_controller_t* hc, u8 addr, u8 config_val) {
    usb_device_request_t req = {
        .req_type = 0x00,
        .req      = USB_REQ_SET_CONFIGURATION,
        .val      = config_val,
        .idx      = 0,
        .len      = 0
    };
    return uhci_control_transfer(hc, addr, true, &req, NULL, 0);
}

static void uhci_reset_port(uhci_controller_t* hc, u16 port_reg) {
    u16 val = uhci_inw(hc, port_reg);
    if (!(val & UHCI_PORT_CONN)) {
        return;
    }

    uhci_outw(hc, port_reg, UHCI_PORT_RESET);
    tsc_sleep(50);
    uhci_outw(hc, port_reg, 0);
    tsc_sleep(10);

    for (int i = 0; i < 10; i++) {
        val = uhci_inw(hc, port_reg);
        if (val & UHCI_PORT_ENABLE) {
            break;
        }
        uhci_outw(hc, port_reg, val | UHCI_PORT_ENABLE);
        tsc_sleep(10);
    }
}

int is_usb_devtype(uhci_controller_t* hc, u8 addr, u8 dev_class, u8 iface_class, u8 iface_subclass, u8 iface_proto) {
    u8 buf[256] = {0};

    usb_device_descriptor_t dev_desc;
    if (usb_get_device_descriptor(hc, addr, &dev_desc) != 0) {
        return 0;
    }

    if (dev_class != 0xFF && dev_desc.bDeviceClass != dev_class && dev_desc.bDeviceClass != 0x00) {
        return 0;
    }

    usb_device_request_t req = {
        .req_type = 0x80,
        .req      = USB_REQ_GET_DESCRIPTOR,
        .val      = USB_DESC_CONFIGURATION << 8,
        .idx      = 0,
        .len      = 9
    };

    if (uhci_control_transfer(hc, addr, true, &req, buf, 9) != 0) {
        printf("Failed to get config descriptor (1)\n");
        return 0;
    }

    usb_config_descriptor_t* cfg = (usb_config_descriptor_t*)buf;
    u16 total_len = cfg->wTotalLength;

    req.len = total_len;
    if (uhci_control_transfer(hc, addr, true, &req, buf, total_len) != 0) {
        printf("Failed to get config descriptor (2)\n");
        return 0;
    }

    u16 offset = 0;
    while (offset < total_len) {
        u8 len = buf[offset];
        u8 type = buf[offset + 1];

        if (len == 0) break;

        if (type == 0x04) {
            usb_interface_descriptor_t* iface = (usb_interface_descriptor_t*)&buf[offset];

            bool match_class    = (iface_class    == 0xFF || iface->bInterfaceClass    == iface_class);
            bool match_subclass = (iface_subclass == 0xFF || iface->bInterfaceSubClass == iface_subclass);
            bool match_proto    = (iface_proto    == 0xFF || iface->bInterfaceProtocol   == iface_proto);

            if (!match_class) printf("Wrong class\n");
            if (!match_subclass) printf("Wrong Subclass\n");
            if (!match_proto) printf("Wrong proto\n");

            if (match_class && match_subclass && match_proto) {
                return 1;
            }
        }
        offset += len;
    }

    return 0;
}

static int uhci_init_controller(u8 bus, u8 slot, u8 fn) {
    if (num_controllers >= MAX_UHCI_CONTROLLERS) {
        return -1;
    }

    u32 bar4 = pci_read_bar(bus, slot, fn, 4);
    if (!(bar4 & 1)) {
        return -1;
    }

    u16 io_base = (u16)(bar4 & ~0x3);
    if (io_base == 0) {
        return -1;
    }

    u16 cmd = pci_cfg_inw(bus, slot, fn, 0x04);
    cmd |= 0x05;
    pci_cfg_outw(bus, slot, fn, 0x04, cmd);

    uhci_controller_t* hc = &controllers[num_controllers];
    hc->bus = bus;
    hc->slot = slot;
    hc->fn = fn;
    hc->io_base = io_base;

    uhci_outw(hc, UHCI_USBCMD, UHCI_CMD_GRESET);
    tsc_sleep(50);
    uhci_outw(hc, UHCI_USBCMD, 0);
    tsc_sleep(10);

    uhci_outw(hc, UHCI_USBCMD, UHCI_CMD_HCRESET);
    for (int i = 0; i < 100; i++) {
        if (!(uhci_inw(hc, UHCI_USBCMD) & UHCI_CMD_HCRESET)) {
            break;
        }
        tsc_sleep(1);
    }

    uhci_outw(hc, UHCI_USBINTR, 0);

    void* fl_phys = pmm_falloc(1);
    if (!fl_phys) {
        return -1;
    }

    hc->frame_list_phys = (uintptr_t)fl_phys;
    hc->frame_list = (u32*)(hc->frame_list_phys + HHDM_START);
    memset(hc->frame_list, 0, 4096);

    void* qh_phys = pmm_falloc(1);
    if (!qh_phys) {
        return -1;
    }

    hc->queue_head_phys = (uintptr_t)qh_phys;
    hc->queue_head = (uhci_qh_t*)(hc->queue_head_phys + HHDM_START);
    memset(hc->queue_head, 0, 4096);

    hc->queue_head->head = UHCI_TD_PTR_T;
    hc->queue_head->element = UHCI_TD_PTR_T;

    for (int i = 0; i < 1024; i++) {
        hc->frame_list[i] = (u32)(hc->queue_head_phys | UHCI_TD_PTR_Q);
    }

    uhci_outl(hc, UHCI_FLBASEADD, (u32)hc->frame_list_phys);
    uhci_outw(hc, UHCI_FRNUM, 0);
    uhci_outw(hc, UHCI_USBCMD, UHCI_CMD_RS | UHCI_CMD_MAXP);

    hc->exists = true;
    num_controllers++;

    printf("UHCI: Controller initialized at I/O 0x%04x\n", io_base);

    uhci_reset_port(hc, UHCI_PORTSC1);
    uhci_reset_port(hc, UHCI_PORTSC2);

    return 0;
}

int init_uhci() {
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            pci_chdr_t hdr;
            pci_get_chdr(bus, slot, &hdr);
            if (hdr.vndid == 0xFFFF) {
                continue;
            }

            u8 max_fns = (hdr.hdrt & 0x80) ? 8 : 1;
            for (u8 fn = 0; fn < max_fns; fn++) {
                u32 r2 = pci_cfg_inl(bus, slot, fn, 0x08);
                u8 cls = (u8)((r2 >> 24) & 0xFF);
                u8 subcls = (u8)((r2 >> 16) & 0xFF);
                u8 progif = (u8)((r2 >> 8) & 0xFF);

                if (cls == 0x0C && subcls == 0x03 && progif == 0x00) {
                    u16 legsup = pci_cfg_inw(bus, slot, fn, 0xC0);
                    if (legsup & 0x2000) {
                        pci_cfg_outw(bus, slot, fn, 0xC0, 0x8F00);
                    }
                    return uhci_init_controller(bus, slot, fn);
                }
            }
        }
    }
    return -1;
}

int uhci_control_transfer(uhci_controller_t* hc, u8 dev_addr, bool low_speed, usb_device_request_t* req, void* data, u16 len) {
    if (!hc || !hc->exists) {
        return -1;
    }

    void* page_phys = pmm_falloc(1);
    if (!page_phys) {
        return -1;
    }

    uintptr_t page_virt = (uintptr_t)page_phys + HHDM_START;
    memset((void*)page_virt, 0, 4096);

    usb_device_request_t* req_buf = (usb_device_request_t*)page_virt;
    *req_buf = *req;

    // bmRequestType bit 7: 0 = host-to-device (OUT), 1 = device-to-host (IN)
    bool data_in = (req->req_type & 0x80) != 0;
    u8 data_pid = data_in ? UHCI_PID_IN : UHCI_PID_OUT;

    u32 ctrl_base = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR;
    if (low_speed) {
        ctrl_base |= UHCI_TD_CTRL_LS;
    }

    uhci_td_t* setup_td = (uhci_td_t*)(page_virt + 64);
    uintptr_t setup_td_phys = (uintptr_t)page_phys + 64;

    setup_td->link = (u32)((uintptr_t)page_phys + 128) | UHCI_TD_PTR_VF;
    setup_td->ctrl = ctrl_base;
    setup_td->token = (7 << 21) | (0 << 19) | ((u32)dev_addr << 8) | UHCI_PID_SETUP;
    setup_td->buffer = (u32)(uintptr_t)page_phys;

    uhci_td_t* status_td;
    uintptr_t status_td_phys;

    if (len && data) {
        // data stage
        uhci_td_t* data_td = (uhci_td_t*)(page_virt + 128);
        status_td = (uhci_td_t*)(page_virt + 192);
        status_td_phys = (uintptr_t)page_phys + 192;

        data_td->link = (u32)(status_td_phys | UHCI_TD_PTR_VF);
        data_td->ctrl = ctrl_base;
        data_td->token = ((u32)len << 21) | (0 << 19) | ((u32)dev_addr << 8) | data_pid;
        data_td->buffer = (u32)(uintptr_t)data;
    } else {
        status_td = (uhci_td_t*)(page_virt + 128);
        status_td_phys = (uintptr_t)page_phys + 128;
    }

    // status stage: zero-length packet, direction opposite of data stage
    status_td->link = UHCI_TD_PTR_T;
    status_td->ctrl = ctrl_base | UHCI_TD_CTRL_IOC;
    status_td->token = (0 << 21) | (1 << 19) | ((u32)dev_addr << 8) | (data_in ? UHCI_PID_OUT : UHCI_PID_IN);
    status_td->buffer = 0;

    hc->queue_head->element = (u32)setup_td_phys;

    for (int i = 0; i < 1000; i++) {
        if (!(status_td->ctrl & UHCI_TD_CTRL_ACT)) {
            break;
        }
        tsc_sleep(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;

    int ret = 0;
    if (status_td->ctrl & UHCI_TD_CTRL_ACT) {
        ret = -1;
    }

    pmm_ffree(page_phys, 1);
    return ret;
}

typedef struct {
    uhci_controller_t* ctrl;
    int port;
} usb_dev_info_t;

static usb_dev_info_t _uhci_usbhid_kbd = {NULL, -1};

int usb_hid_kbd_init() {
    for (usize i = 0; i < num_controllers; i++) {
        uhci_controller_t* hc = &controllers[i];

        uhci_reset_port(hc, UHCI_PORTSC1);

        if (!is_usb_devtype(hc, 0, 0x00, 3, 1, 1) &&
            !is_usb_devtype(hc, 0, 0x03, 3, 1, 1)) {
                printf("Wrong device type\n");
                continue;
        }

        if (usb_set_address(hc, 0, 1) != 0) {
            printf("Failed to set address\n");
            continue;
        }

        if (usb_set_configuration(hc, 1, 1) != 0) {
            printf("Failed to set config\n");
            continue;
        }

        usb_device_request_t idle_req = {
            .req_type = 0x21,
            .req      = 0x0A,
            .val      = 0,
            .idx      = 0,
            .len      = 0
        };

        if (uhci_control_transfer(hc, 1, true, &idle_req, NULL, 0) == 0) {
            printf("Idle request failed\n");
            return 0;
        }

        _uhci_usbhid_kbd.ctrl = hc;
        _uhci_usbhid_kbd.port = 1;
    }
    return -1;
}

void usb_hid_kbd_poll() {
    if (!_uhci_usbhid_kbd.ctrl || _uhci_usbhid_kbd.port == -1) {
        return;
    }

    usb_hid_kbd_report_t curr_report = {0};

    void* page_phys = pmm_falloc(1);
    if (!page_phys) {
        return;
    }

    uintptr_t page_virt = (uintptr_t)page_phys + HHDM_START;
    memset((void*)page_virt, 0, 4096);

    uhci_td_t* in_td = (uhci_td_t*)(page_virt + 64);
    uintptr_t in_td_phys = (uintptr_t)page_phys + 64;

    in_td->link = UHCI_TD_PTR_T;
    in_td->ctrl = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR | UHCI_TD_CTRL_LS | UHCI_TD_CTRL_IOC;
    in_td->token = (7 << 21) | (0 << 19) | (1 << 15) | (_uhci_usbhid_kbd.port << 8) | UHCI_PID_IN;
    in_td->buffer = (u32)(uintptr_t)page_phys;

    uhci_controller_t* hc = _uhci_usbhid_kbd.ctrl;
    hc->queue_head->element = (u32)in_td_phys;

    for (int i = 0; i < 10; i++) {
        if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
            break;
        }
        tsc_sleep(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;

    if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
        memcpy(&curr_report, (void*)page_virt, sizeof(usb_hid_kbd_report_t));

        bool shift = (curr_report.modifiers & 0x22) != 0;

        for (int i = 0; i < 6; i++) {
            u8 key = curr_report.keys[i];
            if (key == 0) {
                continue;
            }

            bool was_pressed = false;
            for (int j = 0; j < 6; j++) {
                if (prev_report.keys[j] == key) {
                    was_pressed = true;
                    break;
                }
            }

            if (!was_pressed) {
                char c = shift ? hid_scancode_map_shift[key] : hid_scancode_map[key];
                if (c) {
                    enqueue_key(c);
                }
            }
        }

        for (int i = 0; i < 6; i++) {
            u8 oldk = prev_report.keys[i];
            if (oldk == 0) {
                continue;
            }
            bool still_down = false;
            for (int j = 0; j < 6; j++) {
                if (curr_report.keys[j] == oldk) {
                    still_down = true;
                    break;
                }
            }
            if (!still_down) {
                u8 sc = hid_to_ps2[oldk];
                if (sc) {
                    kbd_raw_sc = sc | 0x80;
                    kbd_raw_ready = true;
                } else {
                    kbd_raw_ready = false;
                }
            }
        }

        for (int i = 0; i < 6; i++) {
            u8 newk = curr_report.keys[i];
            if (newk == 0) {
                continue;
            }
            bool was_down = false;
            for (int j = 0; j < 6; j++) {
                if (prev_report.keys[j] == newk) {
                    was_down = true;
                    break;
                }
            }
            if (!was_down) {
                u8 sc = hid_to_ps2[newk];
                if (sc) {
                    kbd_raw_sc = sc;
                    kbd_raw_ready = true;
                }
            }
        }

        prev_report = curr_report;
    }

    pmm_ffree(page_phys, 1);
}
