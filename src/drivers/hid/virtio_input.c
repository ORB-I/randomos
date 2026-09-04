#include <core/errno.h>
#include <core/idt.h>
#include <core/kprint.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <drivers/apic.h>
#include <drivers/hid/kbd.h>
#include <drivers/hid/mouse.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <lib/string.h>

#define VIRTIO_INPUT_EVENTQ 0
#define INPUT_EVENT_COUNT 32
#define EV_SYN 0
#define EV_KEY 1
#define EV_REL 2
#define REL_X 0
#define REL_Y 1
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define VIRTIO_F_VERSION_1 (1ULL << 32)

typedef struct {
    u16 type;
    u16 code;
    u32 value;
} __packed virtio_input_event_t;

typedef struct {
    virtio_dev_t dev;
    virtqueue_t eventq;
    u64 buffers_phys;
    virtio_input_event_t* buffers;
    u16 descs[INPUT_EVENT_COUNT];
    int initialized;
    s8 mouse_x;
    s8 mouse_y;
    u8 mouse_buttons;
    bool mouse_dirty;
} virtio_input_device_t;

static virtio_input_device_t input_devices[2];
static usize input_count;
static int input_initialized;

extern void virtio_input_hdlr(void);

static u8 key_to_scancode(u16 code) {
    switch (code) {
    case 0x04: return 0x1e;
    case 0x05: return 0x30;
    case 0x06: return 0x2e;
    case 0x07: return 0x20;
    case 0x08: return 0x12;
    case 0x09: return 0x21;
    case 0x0a: return 0x22;
    case 0x0b: return 0x23;
    case 0x0c: return 0x17;
    case 0x0d: return 0x24;
    case 0x0e: return 0x25;
    case 0x0f: return 0x26;
    case 0x10: return 0x32;
    case 0x11: return 0x31;
    case 0x12: return 0x18;
    case 0x13: return 0x19;
    case 0x14: return 0x10;
    case 0x15: return 0x13;
    case 0x16: return 0x1f;
    case 0x17: return 0x14;
    case 0x18: return 0x16;
    case 0x19: return 0x2f;
    case 0x1a: return 0x11;
    case 0x1b: return 0x2d;
    case 0x1c: return 0x2c;
    case 0x1d: return 0x15;
    case 0x1e: return 0x02;
    case 0x1f: return 0x03;
    case 0x20: return 0x04;
    case 0x21: return 0x05;
    case 0x22: return 0x06;
    case 0x23: return 0x07;
    case 0x24: return 0x08;
    case 0x25: return 0x09;
    case 0x26: return 0x0a;
    case 0x27: return 0x0b;
        case 0x28: return 0x1c;
        case 0x29: return 0x01;
        case 0x2a: return 0x0e;
        case 0x2b: return 0x0f;
        case 0x2c: return 0x39;
        case 0x2d: return 0x0c;
        case 0x2e: return 0x0d;
        case 0x2f: return 0x1a;
        case 0x30: return 0x1b;
        case 0x33: return 0x27;
        case 0x34: return 0x28;
        case 0x35: return 0x29;
        case 0x36: return 0x33;
        case 0x37: return 0x34;
        case 0x38: return 0x35;
        case 0x39: return 0x3a;
        case 0x3a: return 0x3b;
        case 0x3b: return 0x3c;
        case 0x3c: return 0x3d;
        case 0x3d: return 0x3e;
        case 0x3e: return 0x3f;
        case 0x3f: return 0x40;
        case 0x40: return 0x41;
        case 0x41: return 0x42;
        case 0x42: return 0x43;
        case 0x43: return 0x44;
        case 0x44: return 0x45;
        case 0x45: return 0x46;
        case 0x46: return 0x47;
        case 0x47: return 0x48;
        case 0x48: return 0x49;
        case 0x49: return 0x4a;
        case 0x4a: return 0x4b;
        case 0x4b: return 0x4c;
        case 0x4c: return 0x4d;
        case 0x4d: return 0x4e;
        case 0x4e: return 0x4f;
        case 0x4f: return 0x50;
        case 0x50: return 0x51;
        case 0x51: return 0x52;
        case 0x52: return 0x53;
        case 0x53: return 0x1c;
        case 0x54: return 0x35;
        case 0x55: return 0x37;
        case 0x56: return 0x4a;
        case 0x57: return 0x4e;
        case 0x58: return 0x58;
        case 0x59: return 0x4f;
        case 0x5a: return 0x50;
        case 0x5b: return 0x51;
        case 0x5c: return 0x4b;
        case 0x5d: return 0x4c;
        case 0x5e: return 0x4d;
        case 0x5f: return 0x47;
        case 0x60: return 0x48;
        case 0x61: return 0x49;
        case 0x62: return 0x52;
        case 0x63: return 0x53;
        case 0xe0: return 0x1d;
        case 0xe4: return 0x1d;
        default: return 0;
    }
}

static void process_event(virtio_input_device_t* input, virtio_input_event_t* event) {
    if (event->type == EV_KEY) {
        u8 scancode = key_to_scancode(event->code);
        if (scancode) {
            if (event->value == 0) scancode |= 0x80;
            enqueue_sc(scancode);
        }
        if (event->code == BTN_LEFT || event->code == BTN_RIGHT || event->code == BTN_MIDDLE) {
            u8 mask = event->code == BTN_LEFT ? MOUSE_BUTTON_LEFT :
                      event->code == BTN_RIGHT ? MOUSE_BUTTON_RIGHT : MOUSE_BUTTON_MIDDLE;
            if (event->value) input->mouse_buttons |= mask;
            else input->mouse_buttons &= (u8)~mask;
            input->mouse_dirty = true;
        }
    } else if (event->type == EV_REL) {
        if (event->code == REL_X) input->mouse_x += (s8)event->value;
        if (event->code == REL_Y) input->mouse_y += (s8)event->value;
        input->mouse_dirty = true;
    } else if (event->type == EV_SYN && input->mouse_dirty) {
        enqueue_mouse((mouse_info_t){input->mouse_x, input->mouse_y, input->mouse_buttons});
        input->mouse_x = 0;
        input->mouse_y = 0;
        input->mouse_dirty = false;
    }
}

static void poll_device(virtio_input_device_t* input) {
    while (virtqueue_has_used(&input->eventq)) {
        u32 len = 0;
        int desc = virtqueue_poll_used(&input->eventq, &len, 1);
        if (desc < 0) break;
        if (len >= sizeof(virtio_input_event_t)) process_event(input, &input->buffers[desc]);
        input->eventq.desc[desc].flags = VRING_DESC_F_WRITE;
        input->eventq.desc[desc].len = sizeof(virtio_input_event_t);
        virtqueue_submit_chain(&input->eventq, (u16)desc);
    }
    virtqueue_kick(&input->eventq);
}

void c_virtio_input_hdlr(void) {
    for (usize i = 0; i < input_count; i++) {
        if (virtio_read_isr(&input_devices[i].dev)) poll_device(&input_devices[i]);
    }
    lapic_eoi();
}

void virtio_input_poll(void) {
    for (usize i = 0; i < input_count; i++) poll_device(&input_devices[i]);
}

int virtio_input_init(void) {
    if (input_initialized) return input_count != 0 ? 0 : -ENOEXIST;
    input_initialized = 1;

    for (u8 nth = 0; nth < 2 && input_count < 2; nth++) {
        virtio_input_device_t* input = &input_devices[input_count];
        if (virtio_find_pci_device_nth(VIRTIO_DEV_INPUT, &input->dev, 1, nth) < 0) break;

        virtio_reset(&input->dev);
        virtio_set_status(&input->dev, VIRTIO_STATUS_ACKNOWLEDGE);
        virtio_add_status(&input->dev, VIRTIO_STATUS_DRIVER);
        u64 features = virtio_get_features64(&input->dev);
        virtio_set_features64(&input->dev, features & VIRTIO_F_VERSION_1);
        virtio_add_status(&input->dev, VIRTIO_STATUS_FEATURES_OK);
        if (!(virtio_get_status(&input->dev) & VIRTIO_STATUS_FEATURES_OK)) {
            virtio_set_status(&input->dev, VIRTIO_STATUS_FAILED);
            return -EINVAL;
        }
        if (virtqueue_init(&input->dev, VIRTIO_INPUT_EVENTQ, &input->eventq) < 0) return -EDISK;

        input->buffers_phys = (u64)pmm_falloc(1);
        if (!input->buffers_phys) return -ENOMEM;
        input->buffers = (virtio_input_event_t*)(HHDM_START + input->buffers_phys);
        memset(input->buffers, 0, 4096);
        for (u16 i = 0; i < INPUT_EVENT_COUNT && i < input->eventq.size; i++) {
            s32 desc = virtqueue_alloc_desc(&input->eventq);
            if (desc < 0) break;
            input->descs[i] = (u16)desc;
            input->eventq.desc[desc].addr = input->buffers_phys + i * sizeof(virtio_input_event_t);
            input->eventq.desc[desc].len = sizeof(virtio_input_event_t);
            input->eventq.desc[desc].flags = VRING_DESC_F_WRITE;
            virtqueue_submit_chain(&input->eventq, (u16)desc);
        }
        virtqueue_kick(&input->eventq);
        virtio_add_status(&input->dev, VIRTIO_STATUS_DRIVER_OK);

        if (input->dev.irq > 0 && input->dev.irq < 24) {
            u8 vector = 0x20 + input->dev.irq;
            idt_regintr(NULL, vector, virtio_input_hdlr, 0x8E, 1);
            ioapic_set_irq(input->dev.irq, vector, get_lapic_id(), 0);
            ioapic_unmask_irq(input->dev.irq);
        }
        input->initialized = 1;
        input_count++;
    }

    if (input_count) kprint("virtio-input: initialized %zu device(s)\n", input_count);
    return input_count ? 0 : -ENOEXIST;
}

bool virtio_input_available(void) {
    return input_count != 0;
}