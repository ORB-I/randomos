#include <core/asmh.h>
#include <core/lock.h>
#define SERIAL_PORT 0x3F8

lock_t _serial_lock = {0};

int serial_isempty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    lock_acquire(&_serial_lock);

    while (serial_isempty() == 0);
    if (c == '\n') {
        outb(SERIAL_PORT, '\r');
        outb(SERIAL_PORT, '\n');
    } else {
        outb(SERIAL_PORT, c);
    }

    lock_release(&_serial_lock);
}

void serial_puts(const char *str) {
    while (*str != '\0') {
        serial_putchar(*str++);
    }
}