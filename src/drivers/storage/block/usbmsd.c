#include <core/std.h>
#include <core/asmh.h>
#include <core/panic.h>
#include <core/kprint.h>
#include <core/errno.h>
#include <core/mem/pmm.h>
#include <lib/string.h>
#include <core/mem/vmm.h>
#include <core/liballoc.h>

#include <drivers/usb/uhci.h>
#include <drivers/storage/block/usbmsd.h>
#include <drivers/time/clock.h>
#include <drivers/storage/block/block.h>

#define USBMSD_TIMEOUT 5000000

typedef struct {
    uhci_controller_t* ctrl;
    u8 addr;
    u8 epin;
    u8 epout;
    u8 mxpkt;
    u64 tag;
} usbmsd_dev_t;

static int usbmsd_read_cfg_desc(uhci_controller_t* hc, u8 addr, u8** buf) {
    usb_device_request_t req;
    req.req_type = 0x80;
    req.req = USB_REQ_GET_DESCRIPTOR;
    req.val = (USB_DESC_CONFIG << 8) | 0;
    req.idx = 0;
    req.len = 255;

    void* page_phys = pmm_falloc(1);
    if (!page_phys) return -ENOMEM;

    int ret = 0;
    if ((ret = uhci_control_transfer(hc, addr, false, &req, page_phys, 255)) < 0) {
        pmm_ffree(page_phys, 1);
        return ret;
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

    if (!*ep_in || !*ep_out) return -EINVAL;
    return 0;
}

static int usbmsd_do_bot(usbmsd_dev_t* dev, usbmsd_cbw_t* cbw, void* data, u32 len) {
    int ret = 0;
    if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, dev->epout, cbw, sizeof(*cbw), 0)) < 0) {
        return ret;
    }

    if (len > 0) {
        int in = (cbw->flags & 0x80) ? 1 : 0;
        if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, in ? dev->epin : dev->epout, data, len, in)) < 0) {
            return ret;
        }
    }

    usbmsd_csw_t csw;
    if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, dev->epin, &csw, sizeof(csw), 1)) < 0) {
        return ret;
    }

    if (csw.signature != CSW_SIGNATURE) return -EINVAL;
    if (csw.tag != cbw->tag) return -EINVAL;
    if (csw.status != CSW_CMD_PASSED) return -EINVAL;

    return 0;
}

static int usbmsd_scsi_cmd(usbmsd_dev_t* dev, u8 cmd, void* data, u32 len) {
    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = dev->tag++;
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

    int ret = 0;
    if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, dev->epout, &cbw, sizeof(cbw), 0)) < 0) {
        return ret;
    }

    if (len > 0) {
        int in = (cbw.flags & 0x80) ? 1 : 0;
        if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, in ? dev->epin : dev->epout, data, len, in)) < 0) {
            return ret;
        }
    }

    usbmsd_csw_t csw;
    if ((ret = uhci_bulk_transfer(dev->ctrl, dev->addr, dev->epin, &csw, sizeof(csw), 1)) < 0) {
        return ret;
    }

    if (csw.signature != CSW_SIGNATURE) return -EINVAL;
    if (csw.tag != cbw.tag) return -EINVAL;
    if (csw.status != CSW_CMD_PASSED) return -EINVAL;

    return 0;
}

static int usbmsd_reset(uhci_controller_t* hc, u8 addr, u8 iface) {
    usb_device_request_t req;
    req.req_type = 0x21; // Class, Interface, Host-to-Device
    req.req = 0xFF;      // Bulk-Only Mass Storage Reset
    req.val = 0;
    req.idx = iface;
    req.len = 0;

    return uhci_control_transfer(hc, addr, false, &req, NULL, 0);
}

static int usbmsd_wait_ready(usbmsd_dev_t* dev) {
    for (int i = 0; i < 20; i++) {
        if (usbmsd_scsi_cmd(dev, SCSI_TEST_UNIT_READY, NULL, 0) == 0) {
            return 0;
        }
        sleepms(50);
    }
    return -ETIME;
}

void usbmsd_enumerate() {
    uhci_controller_t* conts;
    usize nconts = uhci_get_controllers(&conts);

    for (usize i =0; i < nconts; i++) {
        uhci_controller_t* hc = &conts[i];
        int nports = uhci_get_portcnt(hc);
        for (int p = 0; p < nports; p++) {
            if (hc->port_in_use[p]) continue;
            if (!uhci_portcon(hc, p)) continue;

            if (!is_usb_devicetype(hc, 0, false, USB_CLASS_MASS_STORAGE, USB_PROTOIF_BULK_ONLY)) {
                continue;
            }

            usbmsd_dev_t* dev = malloc(sizeof(*dev));
            if (!dev) return;

            u8 naddr = p + 1;
            uhci_reset_port(hc, p);

            if (usb_set_address(hc, 0, naddr) < 0) continue;
            if (usb_set_configuration(hc, naddr, 1) < 0) continue;

            u8* cfgbuf = NULL;
            if (usbmsd_read_cfg_desc(hc, naddr, &cfgbuf) < 0) {
                continue;
            }

            u16 cfgt = (u16)cfgbuf[2] | ((u16)cfgbuf[3] << 8);
            u8 epin, epout, maxpkt;
            if (usbmsd_parse_endpoints(cfgbuf, cfgt, &epin, &epout, &maxpkt) < 0) {
                pmm_ffree((void*)((u64)cfgbuf - HHDM_START), 1);
                continue;
            }

            pmm_ffree((void*)((u64)cfgbuf - HHDM_START), 1);
            usbmsd_reset(hc, naddr, 0);

            dev->ctrl = hc;
            dev->addr = naddr;
            dev->epin = epin;
            dev->epout = epout;
            dev->mxpkt = maxpkt;
            dev->tag = 0;

            if (usbmsd_wait_ready(dev) < 0) {
                free(dev);
                continue;
            }

            hc->port_in_use[p] = 1;
            hc->addrs[naddr] = 1;

            if (block_register(DRV_USBMSD, (u64)dev) < 0) {
                free(dev);
                return;
            }
        }
    }
}

int usbmsd_secread(u64 drv, u32 lba, u8* buf) {
    usbmsd_dev_t* dev = (usbmsd_dev_t*)drv;

    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = dev->tag++;
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

    int ret = 0;
    if ((ret = usbmsd_do_bot(dev, &cbw, buf, 512)) < 0) {
        kprint("USBMSD: Read error at LBA %d\n", lba);
    }
    return ret;
}

int usbmsd_secwrite(u64 drv, u32 lba, u8* buf) {
    usbmsd_dev_t* dev = (usbmsd_dev_t*)drv;

    usbmsd_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = dev->tag++;
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

    int ret = 0;
    if ((ret = usbmsd_do_bot(dev, &cbw, buf, 512)) < 0) {
        kprint("USBMSD: Write error at LBA %d\n", lba);
    }

    return ret;
}
