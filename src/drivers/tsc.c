#include <core/asmh.h>
#include <drivers/pic.h>

int tsc_cangetfrqviacpuid() {
    u32 eax, ebx, ecx, edx;
    cpuid(0x01, 0, &eax, &ebx, &ecx, &edx);

    u8 family = (eax & 0x00000F00) >> 8;
    u8 model  = (eax & 0x000000F0) >> 4;

    if ((family == 6 && model >= 0x5E) || family == 18) {
        return 1;
    }

    return 0;
}

u64 tsc_getfrqviacpuid() {
    u32 eax, ebx, ecx, edx;
    cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
    if (eax == 0 || ebx == 0 || ecx == 0) {
        return 0; // not supported
    }
    return ((u64)ecx * ebx) / eax;
}

u64 tsc_getfrqviapit() {
    irq_disable(0);
    outb(0x43, 0xB0);

    u16 ticks = 11932;
    outb(0x42, ticks & 0xFF);
    outb(0x42, (ticks >> 8) & 0xFF);

    u8 p61 = inb(0x61);
    outb(0x61, (p61 & 0xfd) | 1);

    u64 tscst = rdtsc();
    while ((inb(0x61) & 0x20) == 0);
    u64 tsced = rdtsc();
    
    outb(0x61, p61 & 0xFC);

    u64 dtsc = tsced - tscst;
    return dtsc / 10;
}

u64 _tsc_frq = 0;
u64 init_tsc() {
    if (tsc_cangetfrqviacpuid()) {
        _tsc_frq = tsc_getfrqviacpuid();
        if (_tsc_frq != 0) {
            return _tsc_frq;
        }
    }
    _tsc_frq = tsc_getfrqviapit();
    return _tsc_frq;
}

u64 get_tscms() {
    if (_tsc_frq == 0) return 0;
    u64 tsc = rdtsc();
    if (tsc > (UINT64_MAX / 1000)) {
        return (tsc / _tsc_frq) * 1000;
    }
    return (tsc * 1000) / _tsc_frq;
}

void tsc_sleep(u64 ms) {
    u64 st = get_tscms();
    while ((get_tscms() - st) < ms);
}