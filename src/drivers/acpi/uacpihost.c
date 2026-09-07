#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <drivers/time/clock.h>
#include <uacpi/kernel_api.h>
#include <core/lock.h>
#include <core/liballoc.h>

extern u64 _tsc_frq;

u64 uacpi_kernel_get_nanoseconds_since_boot() { 
    return (rdtsc() * 1000000000ULL) / _tsc_frq;
}

void uacpi_kernel_stall(u8 us) {
    u64 start = rdtsc();
    u64 ticks = (us * _tsc_frq) / 1000000ULL;
    while (rdtsc() - start < ticks) asm volatile("pause");
}

void uacpi_kernel_sleep(u64 msec) {
    sleepms(msec);
}

// uACPI will only be called from the
// BSP so ignore synchronization primitives

uacpi_handle uacpi_kernel_create_mutex(void) {
    return (uacpi_handle)0x1;
}

void uacpi_kernel_free_mutex(uacpi_handle _) {
    (void)_;
}

uacpi_handle uacpi_kernel_create_event(void) {
    return (uacpi_handle)0x1;
}

void uacpi_kernel_free_event(uacpi_handle _) {
    (void)_;
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return 0;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    u64 rflags;
    asm volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        : "=r"(rflags)
        :: "cc"
    );
    int _uacpi_intrst = rflags & (1 << 9);
    asm("cli");
    return _uacpi_intrst;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    if (state) {
        asm("sti");
    }
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle _, uacpi_u16 __) {
    (void)_; (void)__;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle _) {
    (void)_;
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle _, uacpi_u16 __) {
    (void)_; (void)__;
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle _) {
    (void)_;
}

void uacpi_kernel_reset_event(uacpi_handle _) {
    (void)_;
}


uacpi_handle uacpi_kernel_create_spinlock(void) {
    lock_t* lk = malloc(sizeof(*lk));
    if (!lk) return NULL;
    return lk;
}

void uacpi_kernel_free_spinlock(uacpi_handle hdl) {
    free((lock_t*)hdl);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle hdl) {
    u64 rflags = 0;
    lock_acquire((lock_t*)hdl, &rflags);
    return rflags;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle hdl, uacpi_cpu_flags flgs) {
    lock_release((lock_t*)hdl, &flgs);
}

void *uacpi_kernel_alloc(uacpi_size size) {
    return malloc(size);
}

void uacpi_kernel_free(void *mem) {
    free(mem);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    return (void*)(HHDM_START + addr);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)addr; (void)len;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    (void)len;
    *out_handle = (void*)(u64)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    (void)handle;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle hdl, uacpi_size offset, uacpi_u8 *out_value) {
    u16 io = (u16)(u64)hdl;
    *out_value = inb(io + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle hdl, uacpi_size offset, uacpi_u16 *out_value) {
    u16 io = (u16)(u64)hdl;
    *out_value = inw(io + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle hdl, uacpi_size offset, uacpi_u32 *out_value) {
    u16 io = (u16)(u64)hdl;
    *out_value = inl(io + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle hdl, uacpi_size offset, uacpi_u8 in_value) {
    u16 io = (u16)(u64)hdl;
    outb(io + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle hdl, uacpi_size offset, uacpi_u16 in_value) {
    u16 io = (u16)(u64)hdl;
    outw(io + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle hdl, uacpi_size offset, uacpi_u32 in_value) {
    u16 io = (u16)(u64)hdl;
    outl(io + offset, in_value);
    return UACPI_STATUS_OK;
}