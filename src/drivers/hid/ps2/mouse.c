#include <core/std.h>
#include <drivers/hid/ps2/ps2.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <drivers/apic.h>
#include <drivers/hid/mouse.h>

extern void mouse_hdlr();
int has_ps2mouse() {
    ps2_cmdwrite(0xD4);
    ps2_datawrite(0xFF);

    if (ps2_datareadto(1000) != 0xFA) {
        return false;
    }
    u8 stest = ps2_datareadto(1000);
    u8 devid = ps2_datareadto(1000);

    if (stest == 0xAA && (devid == 0x00 || devid == 0x03 || devid == 0x04)) {
        return 1;
    }
    return 0;
}

void init_mouseps2() {
    while (inb(0x64) & 1) inb(0x60);

    ps2_wait_write();
    ps2_cmdwrite(0xA8);
    ps2_wait_write();
    ps2_cmdwrite(0x20);
    ps2_wait_read();
    u8 cb = ps2_dataread();

    cb |= 0x2;
    cb &= ~0x20;

    ps2_wait_read();
    ps2_cmdwrite(0x60);
    ps2_wait_write();
    ps2_cmdwrite(cb);

    ps2_wait_write();
    ps2_cmdwrite(0xD4);
    ps2_wait_write();
    ps2_cmdwrite(0xF4);

    ps2_wait_read();
    ps2_dataread();

    idt_regintr(0x2C, mouse_hdlr, 0x8E, 1);
    ioapic_set_irq(12, 0x2C, get_lapic_id(), 0);
    ioapic_unmask_irq(12);
}

typedef struct {
    uint8_t left_button   : 1;
    uint8_t right_button  : 1;
    uint8_t middle_button : 1;
    uint8_t always_1      : 1; // must be 1 in a valid packet
    uint8_t x_sign        : 1;
    uint8_t y_sign        : 1;
    uint8_t x_overflow    : 1;
    uint8_t y_overflow    : 1;
} __attribute__((packed)) mouse_status_t;

static u8 ps2mscycle = 0;
static u8 ps2mspkt[3];

void c_mouse_hdlr() {
    u8 stat = ps2_statread();
    if ((stat & 0x01) && (stat & 0x20)) {
        u8 data = ps2_dataread();
        switch (ps2mscycle) {
            case 0: {
                ps2mspkt[0] = data;
                if (data & 0x08) {
                    ps2mscycle++;
                }
                break;
            }
            case 1: {
                ps2mspkt[1] = data;
                ps2mscycle++;
                break;
            }
            case 2: {
                ps2mspkt[2] = data;
                ps2mscycle = 0;

                mouse_status_t* pkt = (mouse_status_t*)&ps2mspkt[0];
                s16 relx = ps2mspkt[1];
                s16 rely = ps2mspkt[2];

                if (pkt->x_sign) relx |= 0xFF00;
                if (pkt->y_sign) rely |= 0xFF00;

                u8 btns = 0;
                if (pkt->left_button) btns |= MOUSE_BUTTON_LEFT;
                if (pkt->right_button) btns |= MOUSE_BUTTON_RIGHT;
                if (pkt->middle_button) btns |= MOUSE_BUTTON_MIDDLE;

                enqueue_mouse((mouse_info_t){
                    (s8)(relx / 2), (s8)(rely / 2), btns
                });
                break;
            }
        }
    }

    lapic_eoi();
}
