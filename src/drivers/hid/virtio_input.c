#include <core/errno.h>
#include <core/idt.h>
#include <core/kprint.h>
#include <core/limreqs.h>
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
#define MAX_INPUT_DEVICES 4

/* virtio-input config space selectors and layout (Linux uapi virtio_input.h):
 * config registers are bytes: u8 select @0, u8 subsel @1, u8 size @2,
 * reserved @3..7, payload union @8. */
#define VIRTIO_INPUT_CFG_ID_NAME    0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL  0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS  0x03
#define VIRTIO_INPUT_CFG_PROP_BITS  0x10
#define VIRTIO_INPUT_CFG_EV_BITS    0x11
#define VIRTIO_INPUT_CFG_ABS_INFO   0x12
#define VIRTIO_INPUT_CFG_SIZE       128

/* virtio-input device features */
#define VIRTIO_INPUT_F_EVENTS (1ULL << 0)
/* global / transport features */
#define VIRTIO_F_VERSION_1 (1ULL << 32)

#define EV_SYN 0
#define EV_KEY 1
#define EV_REL 2
#define EV_ABS 3

#define REL_X 0
#define REL_Y 1

#define ABS_X 0
#define ABS_Y 1

#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/* virtio-input EV_KEY codes are Linux input-event (evdev) codes. For the
 * main keyboard block these equal PS/2 set-1 scancodes, so the character
 * maps below are indexed exactly like the PS/2 driver's. */
#define KEY_LEFTSHIFT  0x2A
#define KEY_RIGHTSHIFT 0x36

#define ROLE_NONE     0
#define ROLE_KEYBOARD 1
#define ROLE_POINTER  2

typedef struct {
    u16 type;
    u16 code;
    u32 value;
} __packed virtio_input_event_t;

typedef struct {
    u16 bustype;
    u16 vendor;
    u16 product;
    u16 version;
} __packed virtio_input_devids_t;

typedef struct {
    u32 min;
    u32 max;
    u32 fuzz;
    u32 flat;
    u32 res;
} __packed virtio_input_absinfo_t;

typedef struct {
    virtio_dev_t dev;
    virtqueue_t eventq;
    u64 buffers_phys;
    virtio_input_event_t* buffers;
    int initialized;

    u8 role; /* ROLE_KEYBOARD / ROLE_POINTER */
    bool shift_down;

    /* pointer state */
    bool is_abs;     /* device reports absolute axes (tablet/touchscreen) */
    bool dirty;      /* something happened since the last EV_SYN */
    u8 buttons;
    s32 delta_x;     /* pending motion since the last EV_SYN, in screen px */
    s32 delta_y;
    bool has_abs_x, has_abs_y;
    s32 abs_x_min, abs_x_max;
    s32 abs_y_min, abs_y_max;
    bool abs_seen_x, abs_seen_y;
    s32 last_abs_x, last_abs_y;
    s32 scr_w, scr_h;
} virtio_input_device_t;

static virtio_input_device_t input_devices[MAX_INPUT_DEVICES];
static usize input_count;
static int input_initialized;
static usize kb_count;
static usize ptr_count;
static s32 scr_w, scr_h;

extern void virtio_input_hdlr(void);

/* evdev code -> character, plain and shifted. Same layout as the PS/2
 * driver's sc_map / sc_map_shift, because the codes are the same space. */
static const char char_map[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e',
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j',
    'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x',
    'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char char_map_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', 0, 0, 'Q', 'W', 'E',
    'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J',
    'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X',
    'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

/* evdev code -> byte for the shared raw-scancode queue. Codes in this
 * range already are set-1 scancodes; extended (E0-prefixed) keys have no
 * single byte and are dropped rather than faked. */
static u8 keycode_to_raw(u16 code) {
    if (code >= 1 && code <= 0x58) return (u8)code;
    return 0;
}

static u8 button_mask(u16 code) {
    if (code == BTN_LEFT) return MOUSE_BUTTON_LEFT;
    if (code == BTN_RIGHT) return MOUSE_BUTTON_RIGHT;
    if (code == BTN_MIDDLE) return MOUSE_BUTTON_MIDDLE;
    return 0;
}

/* Run a config-space query: write select/subsel, then read the reported
 * size and payload. */
static int input_cfg_query(virtio_input_device_t* in, u16 select, u16 subsel,
                           u16* out_size, u8* out, usize max) {
    volatile u8* cfg = in->dev.device_cfg;
    if (!cfg) return -EINVAL;

    cfg[0x00] = (u8)select;
    cfg[0x01] = (u8)subsel;

    u16 size = cfg[0x02];
    if (out_size) *out_size = size;
    if (!out) return 0;
    if (size > max) size = (u16)max;
    for (usize i = 0; i < size; i++) {
        out[i] = cfg[0x08 + i];
    }
    return 0;
}

static bool event_bit_present(const u8* bits, u16 size, u16 code) {
    u16 byte = code >> 3;
    if (byte >= size) return false;
    return (bits[byte] >> (code & 7)) & 1;
}

static s32 scale_abs(s32 value, s32 min, s32 max, s32 screen) {
    if (max <= min) return 0;
    if (value < min) value = min;
    if (value > max) value = max;
    if (screen <= 1) return value - min;
    return (s32)(((s64)(value - min) * (s64)(screen - 1)) / (s64)(max - min));
}

static void read_device_name(virtio_input_device_t* in) {
    char name[65];
    u16 len = 0;
    if (input_cfg_query(in, VIRTIO_INPUT_CFG_ID_NAME, 0, &len, (u8*)name, sizeof(name) - 1) < 0) {
        return;
    }
    if (len > sizeof(name) - 1) len = sizeof(name) - 1;
    name[len] = '\0';
    for (u16 i = 0; i < len; i++) {
        if (name[i] == '\0') {
            len = i;
            break;
        }
    }
    kprint("virtio-input: device name: \"%.*s\"\n", (int)len, name);
}

static void read_device_ids(virtio_input_device_t* in) {
    virtio_input_devids_t ids;
    u16 len = 0;
    if (input_cfg_query(in, VIRTIO_INPUT_CFG_ID_DEVIDS, 0, &len, (u8*)&ids, sizeof(ids)) < 0) {
        return;
    }
    if (len >= sizeof(ids)) {
        kprint("virtio-input: bus %04x vendor %04x product %04x version %04x\n",
               ids.bustype, ids.vendor, ids.product, ids.version);
    }
}

/* Decide what the device is and pull the geometry it needs, based on the
 * event-type capability bits. A pointer has relative or absolute X axes; a
 * keyboard reports EV_KEY codes below the BTN_* range. */
static u8 classify_device(virtio_input_device_t* in) {
    u8 ev[VIRTIO_INPUT_CFG_SIZE];
    u16 sz = 0;
    bool has_rel = false;
    bool has_abs = false;
    bool has_key = false;

    if (input_cfg_query(in, VIRTIO_INPUT_CFG_EV_BITS, EV_REL, &sz, ev, sizeof(ev)) == 0) {
        if (sz > sizeof(ev)) sz = sizeof(ev);
        if (event_bit_present(ev, sz, REL_X) || event_bit_present(ev, sz, REL_Y)) has_rel = true;
    }

    if (input_cfg_query(in, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, &sz, ev, sizeof(ev)) == 0) {
        if (sz > sizeof(ev)) sz = sizeof(ev);
        if (event_bit_present(ev, sz, ABS_X) || event_bit_present(ev, sz, ABS_Y)) has_abs = true;
    }

    if (input_cfg_query(in, VIRTIO_INPUT_CFG_EV_BITS, EV_KEY, &sz, ev, sizeof(ev)) == 0) {
        if (sz > sizeof(ev)) sz = sizeof(ev);
        for (u16 byte = 0; byte < sz && byte < (0x100 >> 3); byte++) {
            if (ev[byte]) {
                has_key = true;
                break;
            }
        }
    }

    if (has_abs || has_rel) {
        in->is_abs = has_abs;

        if (in->is_abs) {
            virtio_input_absinfo_t ai;
            u16 alen = 0;
            if (input_cfg_query(in, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS, &sz, ev, sizeof(ev)) == 0) {
                if (sz > sizeof(ev)) sz = sizeof(ev);
                if (event_bit_present(ev, sz, ABS_X) &&
                    input_cfg_query(in, VIRTIO_INPUT_CFG_ABS_INFO, ABS_X, &alen, (u8*)&ai, sizeof(ai)) == 0 &&
                    alen >= sizeof(ai)) {
                    in->has_abs_x = true;
                    in->abs_x_min = (s32)ai.min;
                    in->abs_x_max = (s32)ai.max;
                }
                if (event_bit_present(ev, sz, ABS_Y) &&
                    input_cfg_query(in, VIRTIO_INPUT_CFG_ABS_INFO, ABS_Y, &alen, (u8*)&ai, sizeof(ai)) == 0 &&
                    alen >= sizeof(ai)) {
                    in->has_abs_y = true;
                    in->abs_y_min = (s32)ai.min;
                    in->abs_y_max = (s32)ai.max;
                }
            }
            in->scr_w = scr_w;
            in->scr_h = scr_h;
        }
        return ROLE_POINTER;
    }

    if (has_key) return ROLE_KEYBOARD;
    return ROLE_NONE;
}

static void process_keyboard_event(virtio_input_device_t* in, const virtio_input_event_t* e) {
    u16 code = e->code;
    bool pressed = e->value != 0;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        in->shift_down = pressed;
    }

    u8 sc = keycode_to_raw(code);
    if (sc) {
        if (!pressed) sc |= 0x80;
        enqueue_sc(sc);
    }

    if (pressed && code < 0x59) {
        char c = in->shift_down ? char_map_shift[code] : char_map[code];
        if (c) enqueue_key(c);
    }
}

static void process_pointer_event(virtio_input_device_t* in, const virtio_input_event_t* e) {
    if (e->type == EV_KEY) {
        u8 mask = button_mask(e->code);
        if (!mask) return;
        u8 prev = in->buttons;
        if (e->value) in->buttons |= mask;
        else in->buttons &= (u8)~mask;
        if (prev != in->buttons) in->dirty = true;
        return;
    }

    if (e->type == EV_REL && !in->is_abs) {
        if (e->code == REL_X) {
            in->delta_x += (s32)e->value;
            if (e->value) in->dirty = true;
        } else if (e->code == REL_Y) {
            in->delta_y += (s32)e->value;
            if (e->value) in->dirty = true;
        }
        return;
    }

    if (e->type == EV_ABS && in->is_abs) {
        if (e->code == ABS_X && in->has_abs_x && in->scr_w > 0) {
            s32 px = scale_abs((s32)e->value, in->abs_x_min, in->abs_x_max, in->scr_w);
            if (!in->abs_seen_x) {
                in->abs_seen_x = true;
                in->last_abs_x = px;
                return;
            }
            s32 dx = px - in->last_abs_x;
            in->last_abs_x = px;
            in->delta_x += dx;
            if (dx) in->dirty = true;
        } else if (e->code == ABS_Y && in->has_abs_y && in->scr_h > 0) {
            s32 py = scale_abs((s32)e->value, in->abs_y_min, in->abs_y_max, in->scr_h);
            if (!in->abs_seen_y) {
                in->abs_seen_y = true;
                in->last_abs_y = py;
                return;
            }
            s32 dy = py - in->last_abs_y;
            in->last_abs_y = py;
            in->delta_y += dy;
            if (dy) in->dirty = true;
        }
    }
}

/* The shared mouse interface only carries s8 deltas per event, so split
 * large absolute jumps (or fast relative motion) into several small
 * steps instead of clipping them. */
static void flush_pointer(virtio_input_device_t* in) {
    if (!in->dirty) return;
    in->dirty = false;

    s32 dx = in->delta_x;
    s32 dy = in->delta_y;
    in->delta_x = 0;
    in->delta_y = 0;

    while (dx || dy) {
        s8 ex = (dx > 127) ? 127 : (dx < -127 ? -127 : (s8)dx);
        s8 ey = (dy > 127) ? 127 : (dy < -127 ? -127 : (s8)dy);
        enqueue_mouse((mouse_info_t){ex, ey, in->buttons});
        dx -= ex;
        dy -= ey;
    }
}

static void process_event(virtio_input_device_t* in, const virtio_input_event_t* e) {
    if (in->role == ROLE_POINTER) {
        if (e->type == EV_SYN) {
            flush_pointer(in);
        } else {
            process_pointer_event(in, e);
        }
    } else if (in->role == ROLE_KEYBOARD && e->type == EV_KEY) {
        process_keyboard_event(in, e);
    }
}

static void poll_device(virtio_input_device_t* in) {
    if (!in->initialized) return;

    while (virtqueue_has_used(&in->eventq)) {
        u32 len = 0;
        int desc = virtqueue_poll_used(&in->eventq, &len, 1);
        if (desc < 0) break;
        if (len >= sizeof(virtio_input_event_t)) {
            process_event(in, &in->buffers[desc]);
        }
        /* hand the buffer back to the device */
        in->eventq.desc[desc].flags = VRING_DESC_F_WRITE;
        in->eventq.desc[desc].len = sizeof(virtio_input_event_t);
        virtqueue_submit_chain(&in->eventq, (u16)desc);
    }
    virtqueue_kick(&in->eventq);
}

void c_virtio_input_hdlr(void) {
    for (usize i = 0; i < input_count; i++) {
        if (virtio_read_isr(&input_devices[i].dev)) poll_device(&input_devices[i]);
    }
    lapic_eoi();
}

void virtio_input_poll(void) {
    for (usize i = 0; i < input_count; i++) {
        poll_device(&input_devices[i]);
    }
}

static int init_input_device(virtio_input_device_t* in) {
    virtio_reset(&in->dev);
    virtio_set_status(&in->dev, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(&in->dev, VIRTIO_STATUS_DRIVER);

    /* We need VERSION_1 semantics for this transport; accept EVENTS when
     * the device offers it. */
    u64 feats = virtio_get_features64(&in->dev);
    u64 accept = feats & (VIRTIO_INPUT_F_EVENTS | VIRTIO_F_VERSION_1);
    if ((accept & VIRTIO_F_VERSION_1) == 0) {
        kprint("virtio-input: device does not offer VERSION_1, skipping\n");
        virtio_set_status(&in->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }
    virtio_set_features64(&in->dev, accept);
    virtio_add_status(&in->dev, VIRTIO_STATUS_FEATURES_OK);
    if (!(virtio_get_status(&in->dev) & VIRTIO_STATUS_FEATURES_OK)) {
        kprint("virtio-input: feature negotiation failed\n");
        virtio_set_status(&in->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }

    read_device_name(in);
    read_device_ids(in);

    in->role = classify_device(in);
    if (in->role == ROLE_NONE) {
        kprint("virtio-input: unrecognized device, skipping\n");
        virtio_set_status(&in->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }
    kprint("virtio-input: device is a %s\n",
           in->role == ROLE_KEYBOARD ? "keyboard" : "pointer");

    if (virtqueue_init(&in->dev, VIRTIO_INPUT_EVENTQ, &in->eventq) < 0) {
        return -EDISK;
    }

    in->buffers_phys = (u64)pmm_falloc(1);
    if (!in->buffers_phys) return -ENOMEM;
    in->buffers = (virtio_input_event_t*)(HHDM_START + in->buffers_phys);
    memset(in->buffers, 0, 4096);

    for (u16 i = 0; i < INPUT_EVENT_COUNT && i < in->eventq.size; i++) {
        s32 desc = virtqueue_alloc_desc(&in->eventq);
        if (desc < 0) break;
        in->eventq.desc[desc].addr = in->buffers_phys + (u64)i * sizeof(virtio_input_event_t);
        in->eventq.desc[desc].len = sizeof(virtio_input_event_t);
        in->eventq.desc[desc].flags = VRING_DESC_F_WRITE;
        virtqueue_submit_chain(&in->eventq, (u16)desc);
    }

    virtio_add_status(&in->dev, VIRTIO_STATUS_DRIVER_OK);
    virtqueue_kick(&in->eventq);

    if (in->dev.irq > 0 && in->dev.irq < 24) {
        u8 vector = 0x20 + in->dev.irq;
        idt_regintr(NULL, vector, virtio_input_hdlr, 0x8E, 1);
        ioapic_set_irq(in->dev.irq, vector, get_lapic_id(), 0);
        ioapic_unmask_irq(in->dev.irq);
    }

    in->initialized = 1;
    return 0;
}

int virtio_input_init(void) {
    if (input_initialized) return input_count != 0 ? 0 : -ENOEXIST;
    input_initialized = 1;

    /* Absolute pointers are scaled onto the display, which matches what
     * the GUI (wm) framebuffer reports. */
    scr_w = 0;
    scr_h = 0;
    if (fb_req.response && fb_req.response->framebuffer_count > 0) {
        scr_w = (s32)fb_req.response->framebuffers[0]->width;
        scr_h = (s32)fb_req.response->framebuffers[0]->height;
    }

    for (u8 nth = 0; nth < MAX_INPUT_DEVICES; nth++) {
        virtio_input_device_t* in = &input_devices[input_count];
        memset(in, 0, sizeof(*in));

        if (virtio_find_pci_device_nth(VIRTIO_DEV_INPUT, &in->dev, 1, nth) < 0) break;

        kprint("virtio-input: found device at %02x:%02x.%x\n",
               in->dev.bus, in->dev.slot, in->dev.fn);

        if (!in->dev.modern) {
            kprint("virtio-input: only modern devices are supported, skipping\n");
            continue;
        }

        if (init_input_device(in) < 0) continue;

        if (in->role == ROLE_KEYBOARD) kb_count++;
        else if (in->role == ROLE_POINTER) ptr_count++;
        input_count++;
    }

    if (input_count) kprint("virtio-input: initialized %zu device(s)\n", input_count);
    return input_count ? 0 : -ENOEXIST;
}

bool virtio_input_kb_available(void) {
    return kb_count != 0;
}

bool virtio_input_ptr_available(void) {
    return ptr_count != 0;
}
