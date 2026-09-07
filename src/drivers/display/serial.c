#include <core/asmh.h>
#include <core/lock.h>
#define SERIAL_PORT 0x3F8

lock_t _serial_lock = {0};

static int serial_initialized = 0;
static int serial_exists = 0;

void serial_init() {
    outb(SERIAL_PORT + 1, 0x00);    // Disable all interrupts
    outb(SERIAL_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_PORT + 1, 0x00);    //                  (hi byte)
    outb(SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold

    // Hardware loopback probe to check if UART port physically exists
    outb(SERIAL_PORT + 4, 0x1E);    // Set loopback mode, test RTS/DTR
    outb(SERIAL_PORT + 0, 0xAE);    // Write test byte
    if (inb(SERIAL_PORT + 0) != 0xAE) {
        serial_exists = 0;
        serial_initialized = 1;
        return;
    }

    // Restore normal operation mode
    outb(SERIAL_PORT + 4, 0x0F);
    serial_exists = 1;
    serial_initialized = 1;
}

int serial_isempty() {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (!serial_initialized) {
        serial_init();
    }
    if (!serial_exists) {
        return;
    }

    u64 rflags;
    lock_acquire(&_serial_lock, &rflags);

    u32 timeout = 100000;
    while (serial_isempty() == 0 && --timeout) {
        asm volatile("pause");
    }

    if (timeout > 0) {
        if (c == '\n') {
            outb(SERIAL_PORT, '\r');
            outb(SERIAL_PORT, '\n');
        } else {
            outb(SERIAL_PORT, c);
        }
    }

    lock_release(&_serial_lock, &rflags);
}

void serial_puts(const char *str) {
    while (*str != '\0') {
        serial_putchar(*str++);
    }
}