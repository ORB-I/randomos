#include <drivers/usb/uhci.h>
#include <drivers/usb/usbhid_mouse.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <core/mem/pmm.h>
#include <drivers/mouse.h>
#include <drivers/tsc.h>

usb_dev_info_t _uhci_usbhid_mouse = {NULL, -1};

int usb_hid_mouse_init() {
    usb_device_request_t req;
    req.req_type = 0x21;
    req.req = 0x0A;
    req.val = 0;
    req.idx = 0;
    req.len = 0;

    uhci_controller_t* conts;
    usize nconts = uhci_get_controllers(&conts);

    for (usize i = 0; i < nconts; i++) {
        uhci_controller_t* hc = &conts[i];
        int nports = uhci_get_portcnt(hc);
        for (int p = 0; p < nports; p++) {
            if (uhci_regdev(hc, p, 1, 0x03, 0x02) < 0) {
                continue;
            }

            if (usb_set_configuration(hc, p, 1) != 0) {
                continue;
            }

            if (uhci_control_transfer(&conts[i], p, 1, &req, NULL, 0) != 0) {
                continue;
            }

            _uhci_usbhid_mouse.ctrl = hc;
            _uhci_usbhid_mouse.port = p;
            return 0;
        }
    }
    return -1;
}

void usb_hid_mouse_poll() {
    if (!_uhci_usbhid_mouse.ctrl || _uhci_usbhid_mouse.port == -1) {
        return;
    }

    usb_hid_mouse_report_t rprt = {0};

    void* page_phys = pmm_falloc(1);
    if (!page_phys) {
        return;
    }

    u64 page_virt = (u64)page_phys + HHDM_START;
    memset((void*)page_virt, 0, 4096);

    uhci_td_t* in_td = (uhci_td_t*)(page_virt + 64);
    u64 in_td_phys = (u64)page_phys + 64;

    in_td->link = UHCI_TD_PTR_T;
    in_td->ctrl = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR | UHCI_TD_CTRL_LS | UHCI_TD_CTRL_IOC;
    in_td->token = (7 << 21) | (0 << 19) | (1 << 15) | (_uhci_usbhid_mouse.port << 8) | UHCI_PID_IN;
    in_td->buffer = (u32)(u64)page_phys;

    uhci_controller_t* hc = _uhci_usbhid_mouse.ctrl;
    hc->queue_head->element = (u32)in_td_phys;

    for (int i = 0; i < 10; i++) {
        if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
            break;
        }
        tsc_sleep(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;

    if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
        memcpy(&rprt, (void*)page_virt, sizeof(usb_hid_mouse_report_t));
        int btns = 0;
        if (rprt.buttons & 1) btns |= MOUSE_BUTTON_LEFT;
        if (rprt.buttons & 2) btns |= MOUSE_BUTTON_RIGHT;
        if (rprt.buttons & 4) btns |= MOUSE_BUTTON_MIDDLE;

        enqueue_mouse((mouse_info_t){
            rprt.x, rprt.y,
            btns
        });
    }

    pmm_ffree(page_phys, 1);
}