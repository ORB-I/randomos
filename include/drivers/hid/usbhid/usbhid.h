#pragma once
#include <drivers/usb/uhci.h>

void* usbhid_poll(usb_dev_info_t* dev, u64 timeout);
void usbhid_pollfree(void* ptr);
