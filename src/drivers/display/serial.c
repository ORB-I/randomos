#include <core/asmh.h>
#include <stdatomic.h>
#define SERIAL_PORT 0x3F8

struct {
    atomic_int locked;
} __attribute__((aligned(4))) _serial_lock = {0};

int serial_isempty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    while (atomic_load(&_serial_lock.locked));
    atomic_store(&_serial_lock.locked, 1);

    while (serial_isempty() == 0);
    if (c == '\n') {
        outb(SERIAL_PORT, '\r');
        outb(SERIAL_PORT, '\n');
    } else {
        outb(SERIAL_PORT, c);
    }

    atomic_store(&_serial_lock.locked, 0);
}

void serial_puts(const char *str) {
    while (*str != '\0') {
        serial_putchar(*str++);
    }
}