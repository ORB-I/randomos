#include <core/errno.h>
#include <drivers/usb/uhci.h>
#include <drivers/hid/usbhid/usbhid_mouse.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <core/mem/pmm.h>
#include <drivers/hid/mouse.h>
#include <drivers/hid/usbhid/usbhid.h>
#include <drivers/time/clock.h>

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
            int addr = 0;
            if ((addr = uhci_regdev(hc, p, 1, 0x03, 0x02)) < 0) {
                continue;
            }

            if (usb_set_configuration(hc, addr, 1) != 0) {
                continue;
            }

            if (uhci_control_transfer(hc, addr, 1, &req, NULL, 0) != 0) {
                continue;
            }

            _uhci_usbhid_mouse.ctrl = hc;
            _uhci_usbhid_mouse.addr = addr;
            return 0;
        }
    }
    return -ENOEXIST;
}

void usb_hid_mouse_poll() {
    usb_hid_mouse_report_t rprt = {0};
    void* res = usbhid_poll(&_uhci_usbhid_mouse, 10);
    if (res) {
        memcpy(&rprt, (void*)res, sizeof(usb_hid_mouse_report_t));
        int btns = 0;
        if (rprt.buttons & 1) btns |= MOUSE_BUTTON_LEFT;
        if (rprt.buttons & 2) btns |= MOUSE_BUTTON_RIGHT;
        if (rprt.buttons & 4) btns |= MOUSE_BUTTON_MIDDLE;

        enqueue_mouse((mouse_info_t){
            rprt.x, rprt.y,
            btns
        });
        usbhid_pollfree(res);
    }
}
