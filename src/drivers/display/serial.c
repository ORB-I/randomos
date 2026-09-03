#include <core/asmh.h>
#include <core/lock.h>
#define SERIAL_PORT 0x3F8

lock_t _serial_lock = {0};

static int serial_initialized = 0;

void serial_init() {
    outb(SERIAL_PORT + 1, 0x00);    // Disable all interrupts
    outb(SERIAL_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_PORT + 1, 0x00);    //                  (hi byte)
    outb(SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(SERIAL_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    serial_initialized = 1;
}

int serial_isempty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (!serial_initialized) {
        serial_init();
    }

    u64 rflags;
    lock_acquire(&_serial_lock, &rflags);

    while (serial_isempty() == 0);
    if (c == '\n') {
        outb(SERIAL_PORT, '\r');
        outb(SERIAL_PORT, '\n');
    } else {
        outb(SERIAL_PORT, c);
    }

    lock_release(&_serial_lock, &rflags);
}

void serial_puts(const char *str) {
    while (*str != '\0') {
        serial_putchar(*str++);
    }
}