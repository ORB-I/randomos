#include <core/asmh.h>
#define SERIAL_PORT 0x3F8

int serial_isempty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    while (serial_isempty() == 0);
    outb(SERIAL_PORT, c);
}

void serial_puts(const char *str) {
    while (*str != '\0') {
        serial_putchar(*str++);
    }
}