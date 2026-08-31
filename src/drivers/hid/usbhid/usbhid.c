#include <drivers/usb/uhci.h>
#include <core/mem/vmm.h>
#include <core/mem/pmm.h>
#include <lib/string.h>
#include <drivers/time/clock.h>

void* usbhid_poll(usb_dev_info_t* dev, u64 timeout) {
    if (!dev->ctrl || dev->addr < 0) {
        return NULL;
    }

    void* page_phys = pmm_falloc(1);
    if (!page_phys) {
        return NULL;
    }

    u64 page_virt = (u64)page_phys + HHDM_START;
    memset((void*)page_virt, 0, 4096);

    uhci_td_t* in_td = (uhci_td_t*)(page_virt + 64);
    u64 in_td_phys = (u64)page_phys + 64;

    in_td->link = UHCI_TD_PTR_T;
    in_td->ctrl = UHCI_TD_CTRL_ACT | UHCI_TD_CTRL_CERR | UHCI_TD_CTRL_LS | UHCI_TD_CTRL_IOC;
    in_td->token = (7 << 21) | (0 << 19) | (1 << 15) | (dev->addr << 8) | UHCI_PID_IN;
    in_td->buffer = (u32)(u64)page_phys;

    uhci_controller_t* hc = dev->ctrl;
    hc->queue_head->element = (u32)in_td_phys;

    for (u64 i = 0; i < timeout; i++) {
        if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
            break;
        }
        sleepms(1);
    }

    hc->queue_head->element = UHCI_TD_PTR_T;

    if (!(in_td->ctrl & UHCI_TD_CTRL_ACT)) {
        return (void*)page_virt;
    } else {
        pmm_ffree(page_phys, 1);
        return NULL;
    }
}

void usbhid_pollfree(void* ptr) {
    pmm_ffree((void*)((u64)ptr - HHDM_START), 1);
}
