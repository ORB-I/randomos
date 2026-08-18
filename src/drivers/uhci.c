#include <core/std.h>
#include <core/asmh.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <drivers/pci.h>
#include <drivers/uhci.h>
#include <drivers/tsc.h>
#include <drivers/term.h>
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

static inline u32 uhci_inl(uhci_controller_t* hc, u16 reg) {
    return inl(hc->io_base + reg);
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
