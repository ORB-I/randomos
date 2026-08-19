#pragma once
#include <sys/types.h>

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04
typedef struct {
    s8 x, y;
    u8 buttons;
} __attribute__((packed)) mouse_info_t;

int get_mouse_info(mouse_info_t* buf);