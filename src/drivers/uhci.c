#include <core/std.h>
#include <core/asmh.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <drivers/pci.h>
#include <drivers/uhci.h>
#include <drivers/tsc.h>
#include <drivers/term.h>
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
    [0x32]=0x27,
    [0x33]=0x28,
    [0x34]=0x29,
    [0x35]=0x33,
    [0x36]=0x34,
    [0x37]=0x35,
    [0x38]=0x3a,
    [0x39]=0x3b,
    [0x3a]=0x3c,
    [0x3b]=0x3d,
    [0x3c]=0x3e,
    [0x3d]=0x3f,
    [0x3e]=0x40,
    [0x3f]=0x41,
    [0x40]=0x42,
    [0x41]=0x43,
    [0x42]=0x44,
    [0x43]=0x57,
    [0x44]=0x58,
    [0x45]=0x00,
    [0x46]=0x00,
    [0x47]=0x00,
    [0x48]=0x00,
    [0x49]=0x00,
    [0x4a]=0x00,
    [0x4b]=0x00,
    [0x4c]=0x00,
    [0x4d]=0x00,
    [0x4e]=0x00,
    [0x4f]=0x4b,
    [0x50]=0x48,
    [0x51]=0x4d,
    [0x52]=0x50,
    [0x53]=0xd2,
    [0x54]=0xc7,
    [0x55]=0xc9,
    [0x56]=0xcf,
    [0x57]=0xd3,
    [0x58]=0xd7,
    [0x59]=0xd1,
    [0x5a]=0xaa,
    [0x5b]=0xb0,
    [0x5c]=0xb1,
    [0x5d]=0xb2,
    [0x5e]=0xb3,
    [0x5f]=0xb4,
    [0x60]=0xb5,
    [0x61]=0xb6,
    [0x62]=0xb7,
    [0x63]=0xb8,
    [0x64]=0xb9,
    [0x65]=0xba,
    [0x66]=0xbb,
    [0x67]=0xbc,
    [0x68]=0xbd,
    [0x69]=0xbe,
    [0x6a]=0xbf,
    [0x6b]=0xc0,
    [0x6c]=0xc1,
    [0x6d]=0xc2,
    [0x6e]=0xc3,
    [0x6f]=0xc4,
    [0x70]=0xc5,
    [0x71]=0xc6,
    [0x72]=0xc7,
    [0x73]=0xc8,
    [0x74]=0xc9,
    [0x75]=0xca,
    [0x76]=0xcb,
    [0x77]=0xcc,
    [0x78]=0xcd,
    [0x79]=0xce,
    [0x7a]=0xcf,
    [0x7b]=0xd0,
    [0x7c]=0xd1,
    [0x7d]=0xd2,
    [0x7e]=0xd3,
    [0x7f]=0xd4,
    [0x80]=0xd5,
    [0x81]=0xd6,
    [0x82]=0xd7,
    [0x83]=0xd8,
    [0x84]=0xd9,
    [0x85]=0xda,
    [0x86]=0xdb,
    [0x87]=0xdc,
    [0x88]=0xdd,
    [0x89]=0xde,
    [0x8a]=0xdf,
    [0x8b]=0xe0,
    [0x8c]=0xe1,
    [0x8d]=0xe2,
    [0x8e]=0xe3,
    [0x8f]=0xe4,
    [0x90]=0xe5,
    [0x91]=0xe6,
    [0x92]=0xe7,
    [0x93]=0xe8,
    [0x94]=0xe9,
    [0x95]=0xea,
    [0x96]=0xeb,
    [0x97]=0xec,
    [0x98]=0xed,
    [0x99]=0xee,
    [0x9a]=0xef,
    [0x9b]=0xf0,
    [0x9c]=0xf1,
    [0x9d]=0xf2,
    [0x9e]=0xf3,
    [0x9f]=0xf4,
    [0xa0]=0xf5,
    [0xa1]=0xf6,
    [0xa2]=0xf7,
    [0xa3]=0xf8,
    [0xa4]=0xf9,
    [0xa5]=0xfa,
    [0xa6]=0xfb,
    [0xa7]=0xfc,
    [0xa8]=0xfd,
    [0xa9]=0xfe,
    [0xaa]=0xff,
    [0xab]=0x10,
    [0xac]=0x11,
    [0xad]=0x12,
    [0xae]=0x13,
    [0xaf]=0x14,
    [0xb0]=0x15,
    [0xb1]=0x16,
    [0xb2]=0x17,
    [0xb3]=0x18,
    [0xb4]=0x19,
    [0xb5]=0x1a,
    [0xb6]=0x1b,
    [0xb7]=0x1c,
    [0xb8]=0x1d,
    [0xb9]=0x1e,
    [0xba]=0x1f,
    [0xbb]=0x20,
    [0xbc]=0x21,
    [0xbd]=0x22,
    [0xbe]=0x23,
    [0xbf]=0x24,
    [0xc0]=0x25,
    [0xc1]=0x26,
    [0xc2]=0x27,
    [0xc3]=0x28,
    [0xc4]=0x29,
    [0xc5]=0x2a,
    [0xc6]=0x2b,
    [0xc7]=0x2c,
    [0xc8]=0x2d,
    [0xc9]=0x2e,
    [0xca]=0x2f,
    [0xcb]=0x30,
    [0xcc]=0x31,
    [0xcd]=0x32,
    [0xce]=0x33,
    [0xcf]=0x34,
    [0xd0]=0x35,
    [0xd1]=0x36,
    [0xd2]=0x37,
    [0xd3]=0x38,
    [0xd4]=0x39,
    [0xd5]=0x3a,
    [0xd6]=0x3b,
    [0xd7]=0x3c,
    [0xd8]=0x3d,
    [0xd9]=0x3e,
    [0xda]=0x3f,
    [0xdb]=0x40,
    [0xdc]=0x41,
    [0xdd]=0x42,
    [0xde]=0x43,
    [0xdf]=0x44,
    [0xe0]=0x45,
    [0xe1]=0x46,
    [0xe2]=0x47,
    [0xe3]=0x48,
    [0xe4]=0x49,
    [0xe5]=0x4a,
    [0xe6]=0x4b,
    [0xe7]=0x4c,
    [0xe8]=0x4d,
    [0xe9]=0x4e,
    [0xea]=0x4f,
    [0xeb]=0x50,
    [0xec]=0x51,
    [0xed]=0x52,
    [0xee]=0x53,
    [0xef]=0x54,
    [0xf0]=0x55,
    [0xf1]=0x56,
    [0xf2]=0x57,
    [0xf3]=0x58,
    [0xf4]=0x59,
    [0xf5]=0x5a,
    [0xf6]=0x5b,
    [0xf7]=0x5c,
    [0xf8]=0x5d,
    [0xf9]=0x5e,
    [0xfa]=0x5f,
    [0xfb]=0x60,
    [0xfc]=0x61,
    [0xfd]=0x62,
    [0xfe]=0x63,
    [0xff]=0x64,
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

void init_uhci() {
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
                    uhci_init_controller(bus, slot, fn);
                }
            }
        }
    }
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

    uhci_td_t* setup_td = (uhci_td_t*)(page_virt + 64);
    uhci_td_t* status_td = (uhci_td_t*)(page_virt + 128);

    uintptr_t setup_td_phys = (uintptr_t)page_phys + 64;
    uintptr_t status_td_phys = (uintptr_t)page_phys + 128;

    u32 ctrl_base = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR;
    if (low_speed) {
        ctrl_base |= UHCI_TD_CTRL_LS;
    }

    setup_td->link = (u32)(status_td_phys | UHCI_TD_PTR_VF);
    setup_td->ctrl = ctrl_base;
    setup_td->token = (7 << 21) | (0 << 19) | ((u32)dev_addr << 8) | UHCI_PID_SETUP;
    setup_td->buffer = (u32)(uintptr_t)page_phys;

    status_td->link = UHCI_TD_PTR_T;
    status_td->ctrl = ctrl_base | UHCI_TD_CTRL_IOC;
    status_td->token = (0x7FF << 21) | (1 << 19) | ((u32)dev_addr << 8) | (req->req_type & 0x80 ? UHCI_PID_OUT : UHCI_PID_IN);
    status_td->buffer = 0;

    (void)data;
    (void)len;

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

void usb_hid_kbd_init() {
    usb_device_request_t req;
    req.req_type = 0x21;
    req.req = 0x0A;
    req.val = 0;
    req.idx = 0;
    req.len = 0;

    for (usize i = 0; i < num_controllers; i++) {
        uhci_control_transfer(&controllers[i], 0, true, &req, NULL, 0);
    }
}

void usb_hid_kbd_poll() {
    if (num_controllers == 0) {
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
    in_td->token = (7 << 21) | (0 << 19) | (1 << 15) | (0 << 8) | UHCI_PID_IN;
    in_td->buffer = (u32)(uintptr_t)page_phys;

    uhci_controller_t* hc = &controllers[0];
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
