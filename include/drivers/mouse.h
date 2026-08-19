#pragma once
#include <core/std.h>

#define MOUSETYPE_PS2    1
#define MOUSETYPE_USBHID 2

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04
typedef struct {
    s16 x, y;
    u8 buttons;
} mouse_info_t;

void enqueue_mouse(mouse_info_t info);
mouse_info_t dequeue_mouse();
int init_mouse(int type);
int get_mouse_info(mouse_info_t* buf);