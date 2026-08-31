#include <core/std.h>
#include <core/asmh.h>
#include <core/errno.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <drivers/pci.h>
#include <drivers/usb/uhci.h>
#include <drivers/time/clock.h>
#include <core/printf.h>
#include <drivers/hid/kbd.h>
#include <lib/string.h>

#define MAX_UHCI_CONTROLLERS 4
static uhci_controller_t controllers[MAX_UHCI_CONTROLLERS];
static usize num_controllers = 0;

static inline u16 uhci_inw(uhci_controller_t* hc, u16 reg) {
    return inw(hc->io_base + reg);
}

static inline void uhci_outw(uhci_controller_t* hc, u16 reg, u16 val) {
    outw(hc->io_base + reg, val);
}

static inline void uhci_outl(uhci_controller_t* hc, u16 reg, u32 val) {
    outl(hc->io_base + reg, val);
}

int usb_set_address(uhci_controller_t* hc, u8 old_addr, u8 new_addr) {
    usb_device_request_t req = {
        .req_type = 0x00,
        .req      = USB_REQ_SET_ADDRESS,
        .val      = new_addr,
        .idx      = 0,
        .len      = 0
    };
    int ret = uhci_control_transfer(hc, old_addr, true, &req, NULL, 0);
    sleepms(10);
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

void uhci_reset_port(uhci_controller_t* hc, int port) {
    u16 port_reg = UHCI_PORTSC1 + (port * 2);
    u16 val = uhci_inw(hc, port_reg);
    if (!(val & UHCI_PORT_CONN)) {
        return;
    }

    uhci_outw(hc, port_reg, UHCI_PORT_RESET);
    sleepms(50);
    uhci_outw(hc, port_reg, 0);
    sleepms(10);

    for (int i = 0; i < 10; i++) {
        val = uhci_inw(hc, port_reg);
        if (val & UHCI_PORT_ENABLE) {
            break;
        }
        uhci_outw(hc, port_reg, val | UHCI_PORT_ENABLE);
        sleepms(10);
    }
}

int uhci_get_portcnt(uhci_controller_t* hc) {
    u16 cnt = 0;
    while (cnt < 16) {
        u16 pscioaddr = hc->io_base + 0x10 + (cnt * 2);
        u16 psc = inw(pscioaddr);
        if ((psc & (1 << 7)) == 0 || psc == 0xFFFF) {
            break;
        }
        cnt++;
    }
    return cnt;
}

int uhci_portcon(uhci_controller_t* hc, uint8_t port) {
    uint16_t portsc_reg = hc->io_base + 0x10 + (port * 2);
    uint16_t status = inw(portsc_reg);
    if (status == 0xFFFF) {
        return false;
    }
    return (status & UHCI_PORTSC_CCS) != 0;
}

int uhci_regdev(uhci_controller_t* hc, int port, int low_speed, u8 class, u8 proto) {
    if (!uhci_portcon(hc, port)) {
        return UHCI_REG_NODEV;
    }

    if (hc->port_in_use[port]) {
        return UHCI_REG_INUSE;
    }

    uhci_reset_port(hc, port);
    if (!is_usb_devicetype(hc, 0, low_speed, class, proto)) {
        return UHCI_REG_NTYPE;
    }

    if (usb_set_address(hc, 0, port + 1) < 0) {
        return UHCI_REG_NSADR;
    }

    hc->port_in_use[port] = 1;
    hc->addrs[port + 1] = 1;

    return port + 1;
}

static int uhci_init_controller(u8 bus, u8 slot, u8 fn) {
    if (num_controllers >= MAX_UHCI_CONTROLLERS) {
        return -ERANGE;
    }

    u32 bar4 = pci_read_bar(bus, slot, fn, 4);
    if (!(bar4 & 1)) {
        return -EINVAL;
    }

    u16 io_base = (u16)(bar4 & ~0x3);
    if (io_base == 0) {
        return -ENOEXIST;
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
    sleepms(50);
    uhci_outw(hc, UHCI_USBCMD, 0);
    sleepms(10);

    uhci_outw(hc, UHCI_USBCMD, UHCI_CMD_HCRESET);
    for (int i = 0; i < 100; i++) {
        if (!(uhci_inw(hc, UHCI_USBCMD) & UHCI_CMD_HCRESET)) {
            break;
        }
        sleepms(1);
    }

    uhci_outw(hc, UHCI_USBINTR, 0);

    void* fl_phys = pmm_falloc(1);
    if (!fl_phys) {
        return -ENOMEM;
    }

    hc->frame_list_phys = (u64)fl_phys;
    hc->frame_list = (u32*)(hc->frame_list_phys + HHDM_START);
    memset(hc->frame_list, 0, 4096);

    void* qh_phys = pmm_falloc(1);
    if (!qh_phys) {
        return -ENOMEM;
    }

    hc->queue_head_phys = (u64)qh_phys;
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

    memset(hc->port_in_use, 0, sizeof(hc->port_in_use));
    hc->nports = uhci_get_portcnt(hc);
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
    return -ENOEXIST;
}

static int uhci_ctrl_err(u32 ctrl) {
    u32 err = (ctrl >> 27) & 0x03;
    return err;
}

int uhci_control_transfer(uhci_controller_t* hc, u8 dev_addr, bool low_speed, usb_device_request_t* req, void* data, u16 len) {
    if (!hc || !hc->exists) {
        return -EINVAL;
    }

    u64 dtvirt = (u64)data + HHDM_START;

    void* pgphys = pmm_falloc(1);
    if (!pgphys) return -ENOMEM;

    u64 pvirt = (u64)pgphys + HHDM_START;
    memset((void*)pvirt, 0, 4096);

    usb_device_request_t* reqbuf = (usb_device_request_t*)pvirt;
    *reqbuf = *req;

    u64 dtbovirt = pvirt + 0x400;
    u64 dtbophys = (u64)pgphys + 0x400;

    bool data_in = (req->req_type & 0x80) != 0;
    u8 data_pid = data_in ? UHCI_PID_IN : UHCI_PID_OUT;

    if (len && data && !data_in) {
        if (len > 2048) {
            pmm_ffree(pgphys, 1);
            return -ERANGE;
        }
        memcpy((void*)dtbovirt, (void*)dtvirt, len);
    }

    u32 ctrlb = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR;
    if (low_speed) {
        ctrlb |= UHCI_TD_CTRL_LS;
    }

    uhci_td_t* tds = (uhci_td_t*)(pvirt + 0x040);
    u64 tdsp = (u64)pgphys + 0x040;
    int tdcnt = 0;

    uhci_td_t* setup_td = &tds[tdcnt];
    u64 setup_tdp = tdsp + (tdcnt * sizeof(uhci_td_t));
    tdcnt++;

    setup_td->ctrl = ctrlb;
    setup_td->token = ((7) << 21) | (0 << 20) | (0 << 15) | ((u32)dev_addr << 8) | UHCI_PID_SETUP;
    setup_td->buffer = (u32)((u64)pgphys);

    uhci_td_t* last_td = setup_td;

    u16 brem = len;
    u16 dtoff = 0;
    u8 tgl = 1;

    while (brem > 0) {
        u16 pksz = brem;
        if (pksz > 8) {
            pksz = 8;
        }

        uhci_td_t* data_td = &tds[tdcnt];
        u64 data_tdp = tdsp + (tdcnt * sizeof(uhci_td_t));
        tdcnt++;

        u32 mlene = (u32)(pksz - 1) & 0x7FF;
        data_td->token = (mlene << 21) | ((u32)tgl << 20) | (0 << 15) | ((u32)dev_addr << 8) | data_pid;
        data_td->buffer = (u32)(dtbophys + dtoff);
        data_td->ctrl = ctrlb;

        last_td->link = (u32)(data_tdp | UHCI_TD_PTR_VF);
        last_td = data_td;

        tgl ^= 1;
        dtoff += pksz;
        brem -= pksz;
    }

    uhci_td_t* status_td = &tds[tdcnt];
    u64 status_td_phys = tdsp + (tdcnt * sizeof(uhci_td_t));
    tdcnt++;

    status_td->link = UHCI_TD_PTR_T;
    status_td->ctrl = ctrlb | UHCI_TD_CTRL_IOC;
    status_td->token = (0 << 21) | (1 << 20) | (0 << 15) | ((u32)dev_addr << 8) | (data_in ? UHCI_PID_OUT : UHCI_PID_IN);
    status_td->buffer = (u32)((u64)pgphys + 0x300);

    last_td->link = (u32)(status_td_phys | UHCI_TD_PTR_VF);
    hc->queue_head->element = (u32)setup_tdp;

    int ret = 0;

    u32 nerr = 0;
    while (1) {
        volatile u32* statctrl = (volatile u32*)&status_td->ctrl;
        if (!(*statctrl & UHCI_TD_CTRL_ACT)) {
            break;
        }

        u32 nnerr = uhci_ctrl_err(*statctrl);
        if (nnerr < nerr) {
            printf("New error detected: 0x%08x\n", *statctrl);
            if (nnerr == 0) {
                printf("Max errors reached, terminating (flags: 0x%08x, ec: 0x%x)\n",  status_td->ctrl, (status_td->ctrl >> 16) & 0x0000000F);
                ret = -1;
            }
            nerr = nnerr;
        }

        sleepms(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;
    if (status_td->ctrl & UHCI_TD_CTRL_ACT) {
        printf("UHCICT: Controller didn't clear TDActive (Timeout, flags: 0x%08x, error code: 0x%x)\n", status_td->ctrl, (status_td->ctrl >> 16) & 0x0000000F);
        ret = -1;
    } else if (status_td->ctrl & 0x1F0000) {
        printf("UHCICT: Transfer failed with status error flags: 0x%08x\n", status_td->ctrl);
        ret = -1;
    }

    if (ret == 0 && len && (void*)dtvirt && data_in) {
        memcpy((void*)dtvirt, (void*)dtbovirt, len);
    }

    pmm_ffree(pgphys, 1);
    return ret;
}

int is_usb_devicetype(uhci_controller_t* hc, u8 dev_addr, bool low_speed, u8 cls, u8 proto) {
    if (!hc || !hc->exists) return 0;

    void* buf_phys = pmm_falloc(1);
    if (!buf_phys) return 0;

    u64 buf_virt = (u64)buf_phys + HHDM_START;
    memset((void*)buf_virt, 0, 4096);

    usb_device_request_t req;
    req.req_type = 0x80;
    req.req = 6;
    req.val = (2 << 8) | 0;
    req.idx = 0;
    req.len = 255;

    if (uhci_control_transfer(hc, dev_addr, low_speed, &req, (void*)buf_phys, 255) < 0) {
        printf("Failed to read configuration space\n");
        pmm_ffree(buf_phys, 1);
        return 0;
    }

    u8* desc = (u8*)buf_virt;
    u16 total_len = (u16)desc[2] | ((u16)desc[3] << 8);

    // walk through looking for a matching interface descriptor
    u16 offset = 0;
    int found = 0;
    while (offset + 2 <= total_len) {
        u8 bLength = desc[offset];
        u8 bDescType = desc[offset + 1];

        if (bLength < 2) break;

        if (bDescType == USB_DESC_INTERFACE && offset + 9 <= total_len) {
            u8 iface_class = desc[offset + 5];
            u8 iface_proto = desc[offset + 7];
            if (iface_class == cls && iface_proto == proto) {
                found = 1;
                break;
            }
        }

        offset += bLength;
    }

    pmm_ffree(buf_phys, 1);
    return found;
}

usize uhci_get_controllers(uhci_controller_t** ctrlrs) {
    *ctrlrs = controllers;
    return num_controllers;
}

int uhci_bulk_transfer(uhci_controller_t* hc, u8 dev_addr, u8 ep, void* data, u32 len, int in) {
    if (!hc || !hc->exists) return -EINVAL;

    void* pgphys = pmm_falloc(1);
    if (!pgphys) return -ENOMEM;

    u64 pvirt = (u64)pgphys + HHDM_START;
    memset((void*)pvirt, 0, 4096);

    u64 dtbovirt = pvirt + 0x400;
    u64 dtbophys = (u64)pgphys + 0x400;

    if (data && !in) {
        memcpy((void*)dtbovirt, data, len);
    }

    u32 ctrlb = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR;
    u32 epid = in ? UHCI_PID_IN : UHCI_PID_OUT;
    u32 ep_bit = (u32)(ep & 0x0F) << 15;

    uhci_td_t* tds = (uhci_td_t*)(pvirt + 0x040);
    u64 tdsp = (u64)pgphys + 0x040;
    int tdcnt = 0;

    u32 brem = len;
    u32 dtoff = 0;
    u8 tgl = 1;

    while (brem > 0) {
        u16 pksz = brem > 64 ? 64 : (u16)brem;
        uhci_td_t* td = &tds[tdcnt];
        u64 td_phys = tdsp + (tdcnt * sizeof(uhci_td_t));
        tdcnt++;

        td->link = (tdcnt < 255) ? (u32)((td_phys + sizeof(uhci_td_t)) | UHCI_TD_PTR_VF) : UHCI_TD_PTR_T;
        td->ctrl = ctrlb;
        td->token = ((pksz - 1) << 21) | ((u32)tgl << 20) | (0 << 19) | ep_bit | ((u32)dev_addr << 8) | epid;
        td->buffer = (u32)(dtbophys + dtoff);

        tgl ^= 1;
        dtoff += pksz;
        brem -= pksz;
    }

    uhci_td_t* last_td = &tds[tdcnt - 1];
    last_td->link = UHCI_TD_PTR_T;

    hc->queue_head->element = (u32)((u64)pgphys + 0x040);

    int ret = 0;
    u32 nerr = 0;
    while (1) {
        if (!(last_td->ctrl & UHCI_TD_CTRL_ACT)) break;
        u32 nnerr = uhci_ctrl_err(last_td->ctrl);
        if (nnerr < nerr) {
            if (nnerr == 0) ret = -1;
            nerr = nnerr;
        }
        sleepms(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;

    if (last_td->ctrl & UHCI_TD_CTRL_ACT) ret = -1;
    else if (last_td->ctrl & 0x1F0000) ret = -1;

    if (ret == 0 && in && data) {
        memcpy(data, (void*)dtbovirt, len);
    }

    pmm_ffree(pgphys, 1);
    return ret;
}
