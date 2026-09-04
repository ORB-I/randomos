#pragma once
#include <core/std.h>

#define KBD_USBHID 1
#define KBD_PS2    2
#define KBD_VIRTIO 3

void init_kbd(int kbd_type);

void noecho(int on);
char getchar(void);
usize getstr(char* buf, usize ntoread);

void enqueue_key(char c);
char* readline(const char* prompt);

u8 kbd_get_raw(void);

void enqueue_sc(u8 sc);
char dequeue_sc(void);
bool kb_has_sc(void);
void kbd_setshift(int shift);
int kbd_getshift();
u8 kbd_getrawto(u64 timeout);