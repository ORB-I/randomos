#include <core/std.h>
#include <core/asmh.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/mem/pmm.h>
#include <lib/string.h>

#include <drivers/usb/uhci.h>
#include <drivers/storage/usbmsd.h>
#include <drivers/time/clock.h>

#define USBMSD_TIMEOUT 5000000

typedef struct {
    uhci_controller_t* ctrl;
    u8 addr;
    u8 ep_in;
    u8 ep_out;
    u8 max_packet;
} usbmsd_dev_t;

static usbmsd_dev_t usbmsd_dev;
static u32 usbmsd_tag = 1;

static int usbmsd_read_cfg_desc(uhci_controller_t* hc, u8 addr, u8** buf) {
    usb_device_request_t req;
    req.req_type = 0x80;
    req.req = USB_REQ_GET_DESCRIPTOR;
    req.val = (USB_DESC_CONFIG << 8) | 0;
    req.idx = 0;
    req.len = 255;

    void* page_phys = pmm_falloc(1);
    if (!page_phys) return -1;

    if (uhci_control_transfer(hc, addr, false, &req, page_phys, 255) < 0) {
        pmm_ffree(page_phys, 1);
        return -1;
    }

    *buf = (u8*)((u64)page_phys + HHDM_START);
    return 0;
}

static int usbmsd_parse_endpoints(u8* cfg, u16 total_len, u8* ep_in, u8* ep_out, u8* max_pkt) {
    u16 offset = 0;
    u8 found_iface = 0;
    *ep_in = 0;
    *ep_out = 0;
    *max_pkt = 64;

    while (offset + 2 <= total_len) {
        u8 blen = cfg[offset];
        u8 btype = cfg[offset + 1];

        if (blen < 2) break;

        if (btype == USB_DESC_INTERFACE && offset + 9 <= total_len) {
            u8 cls = cfg[offset + 5];
            u8 subcls = cfg[offset + 6];
            u8 proto = cfg[offset + 7];
            if (cls == USB_CLASS_MASS_STORAGE && subcls == USB_SUBCLASS_SCSI && proto == USB_PROTOIF_BULK_ONLY) {
                found_iface = 1;
            } else {
                found_iface = 0;
            }
        } else if (btype == USB_DESC_ENDPOINT && found_iface && offset + 7 <= total_len) {
            u8 ep_addr = cfg[offset + 2];
            u8 ep_attr = cfg[offset + 3];
            u16 ep_maxpkt = (u16)cfg[offset + 4] | ((u16)cfg[offset + 5] << 8);

            if ((ep_attr & 0x3) == USB_EP_TYPE_BULK) {
                if (ep_addr & 0x80) {
                    *ep_in = ep_addr & 0x0F;
                } else {
                    *ep_out = ep_addr & 0x0F;
                }
                if (ep_maxpkt > 0 && ep_maxpkt < 512) {
                    *max_pkt = (u8)ep_maxpkt;
                }
            }
        }

        offset += blen;
    }

    if (!*ep_in || !*ep_out) return -1;
    return 0;
}

static int usbmsd_do_bot(usbmsd_cbw_t* cbw, void* data, u32 len) {
    usbmsd_dev_t* dev = &usbmsd_dev;

    if (uhci_bulk_transfer(dev->ctrl, dev->addr, dev->ep_out, cbw, sizeof(*cbw), 0) < 0) {
        return -1;
    }

    if (len > 0) {
        int in = (cbw->flags & 0x80) ? 1 : 0;
        if (uhci_bulk_transfer(dev->ctrl, dev->addr, in ? dev->ep_in : dev->ep_out, data, len, in) < 0) {
            return -1;
        }
    }

    usbmsd_csw_t csw;
    if (uhci_bulk_transfer(dev->ctrl, dev->addr, dev->ep_in, &csw, sizeof(csw), 1) < 0) {
        return -1;
    }

    if (csw.signature != CSW_SIGNATURE) return -1;
    if (csw.tag != cbw->tag) return -1;
    if (csw.status != CSW_CMD_PASSED) return -1;

    return 0;
}

static int usbmsd_scsi_cmd(u8 cmd, void* data, u32 len) {
    usbmsd_dev_t* dev = &usbmsd_dev;
    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = usbmsd_tag++;
    cbw.data_len = len;
    cbw.flags = (cmd == SCSI_INQUIRY || cmd == SCSI_READ_CAPACITY || cmd == SCSI_REQUEST_SENSE) ? 0x80 : 0x00;
    cbw.lun = 0;
    cbw.cdb_len = 6;
    cbw.cdb[0] = cmd;

    if (cmd == SCSI_INQUIRY) {
        cbw.cdb[4] = (u8)(len & 0xFF);
    } else if (cmd == SCSI_READ_CAPACITY) {
        cbw.cdb_len = 10;
        cbw.cdb[8] = (u8)(len & 0xFF);
    }

    if (uhci_bulk_transfer(dev->ctrl, dev->addr, dev->ep_out, &cbw, sizeof(cbw), 0) < 0) {
        return -1;
    }

    if (len > 0) {
        int in = (cbw.flags & 0x80) ? 1 : 0;
        if (uhci_bulk_transfer(dev->ctrl, dev->addr, in ? dev->ep_in : dev->ep_out, data, len, in) < 0) {
            return -1;
        }
    }

    usbmsd_csw_t csw;
    if (uhci_bulk_transfer(dev->ctrl, dev->addr, dev->ep_in, &csw, sizeof(csw), 1) < 0) {
        return -1;
    }

    if (csw.signature != CSW_SIGNATURE) return -1;
    if (csw.tag != cbw.tag) return -1;
    if (csw.status != CSW_CMD_PASSED) return -1;

    return 0;
}

static int usbmsd_wait_ready(uhci_controller_t* hc, u8 addr) {
    for (int i = 0; i < 20; i++) {
        if (usbmsd_scsi_cmd(SCSI_TEST_UNIT_READY, NULL, 0) == 0) {
            return 0;
        }
        sleepms(100);
    }
    return -1;
}

int usbmsd_init(void) {
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
            if (hc->port_in_use[p]) continue;
            if (!uhci_portcon(hc, p)) continue;

            if (!is_usb_devicetype(hc, 0, false, USB_CLASS_MASS_STORAGE, USB_PROTOIF_BULK_ONLY)) {
                continue;
            }

            u8 new_addr = p + 1;
            uhci_reset_port(hc, p);

            if (usb_set_address(hc, 0, new_addr) < 0) continue;

            if (usb_set_configuration(hc, new_addr, 1) < 0) continue;

            u8* cfg_buf;
            if (usbmsd_read_cfg_desc(hc, new_addr, &cfg_buf) < 0) {
                continue;
            }

            u16 cfg_total = (u16)cfg_buf[2] | ((u16)cfg_buf[3] << 8);
            u8 ep_in, ep_out, max_pkt;
            if (usbmsd_parse_endpoints(cfg_buf, cfg_total, &ep_in, &ep_out, &max_pkt) < 0) {
                pmm_ffree((void*)((u64)cfg_buf - HHDM_START), 1);
                continue;
            }

            pmm_ffree((void*)((u64)cfg_buf - HHDM_START), 1);

            if (usbmsd_wait_ready(hc, new_addr) < 0) continue;

            memset(&usbmsd_dev, 0, sizeof(usbmsd_dev));
            usbmsd_dev.ctrl = hc;
            usbmsd_dev.addr = new_addr;
            usbmsd_dev.ep_in = ep_in;
            usbmsd_dev.ep_out = ep_out;
            usbmsd_dev.max_packet = max_pkt;

            hc->port_in_use[p] = 1;
            hc->addrs[new_addr] = 1;

            printf("USBMSD: Found device on port %d, addr %d, ep_in=%d ep_out=%d\n", p, new_addr, ep_in, ep_out);
            return (int)new_addr;
        }
    }

    printf("USBMSD: No device found\n");
    return -1;
}

void usbmsd_secread(u8 drv, u32 lba, u8* buf) {
    if (usbmsd_dev.addr == 0) return;

    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = usbmsd_tag++;
    cbw.data_len = 512;
    cbw.flags = 0x80;
    cbw.lun = 0;
    cbw.cdb_len = 10;
    cbw.cdb[0] = SCSI_READ_10;
    cbw.cdb[2] = (lba >> 24) & 0xFF;
    cbw.cdb[3] = (lba >> 16) & 0xFF;
    cbw.cdb[4] = (lba >> 8) & 0xFF;
    cbw.cdb[5] = lba & 0xFF;
    cbw.cdb[7] = 0x00;
    cbw.cdb[8] = 1;

    if (usbmsd_do_bot(&cbw, buf, 512) < 0) {
        printf("USBMSD: Read error at LBA %d\n", lba);
    }
}

void usbmsd_secwrite(u8 drv, u32 lba, u8* buf) {
    if (usbmsd_dev.addr == 0) return;

    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = usbmsd_tag++;
    cbw.data_len = 512;
    cbw.flags = 0x00;
    cbw.lun = 0;
    cbw.cdb_len = 10;
    cbw.cdb[0] = SCSI_WRITE_10;
    cbw.cdb[2] = (lba >> 24) & 0xFF;
    cbw.cdb[3] = (lba >> 16) & 0xFF;
    cbw.cdb[4] = (lba >> 8) & 0xFF;
    cbw.cdb[5] = lba & 0xFF;
    cbw.cdb[7] = 0x00;
    cbw.cdb[8] = 1;

    if (usbmsd_do_bot(&cbw, buf, 512) < 0) {
        printf("USBMSD: Write error at LBA %d\n", lba);
    }
}
