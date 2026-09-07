#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <core/kprint.h>
#include <core/lock.h>
#include <core/liballoc.h>
#include <core/limreqs.h>
#include <drivers/time/clock.h>
#include <drivers/pci.h>
#include <drivers/pic.h>

#include <uacpi/kernel_api.h>

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

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (!rsdp_req.response || !rsdp_req.response->address)
        return UACPI_STATUS_NOT_FOUND;

    // Limine reports the RSDP as a pointer into its own higher-half
    // mapping (offset by the HHDM offset), not as a plain physical
    // address. uACPI wants physical, so subtract the HHDM offset.
    *out_rsdp_address = (uacpi_phys_addr)(
        (u64)rsdp_req.response->address - hhdm_request.response->offset
    );
    return UACPI_STATUS_OK;
}

/* Filter out debug/trace spam so boot messages stay readable */
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *msg) {
    if (level <= UACPI_LOG_INFO) {
        kprint("uACPI: %s\n", msg);
    }
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

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) {
    if (req->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
        kprint("uACPI: fatal firmware request (type=%u code=0x%x arg=0x%lx)\n",
               req->fatal.type, req->fatal.code, req->fatal.arg);
    }
    return UACPI_STATUS_OK;
}

/*
 * PCI configuration space access.
 *
 * The kernel only has the legacy port-based config mechanism
 * (0xCF8/0xCFC), which can only reach the first 256 bytes of each
 * device's config space. uACPI rarely needs more than that.
 */

static inline u32 uacpi_pci_handle_addr(uacpi_handle device) {
    return (u32)(u64)device;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    if (address.segment != 0 || address.bus > 255 ||
        address.device > 31 || address.function > 7) {
        return UACPI_STATUS_NOT_FOUND;
    }

    // Probe: nonexistent devices read back as 0xFFFFFFFF
    if (pci_cfg_inl(address.bus, address.device, address.function, 0) == 0xFFFFFFFF) {
        return UACPI_STATUS_NOT_FOUND;
    }

    *out_handle = (uacpi_handle)(u64)(
        ((u32)address.bus << 16) |
        ((u32)address.device << 11) |
        ((u32)address.function << 8)
    );
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle device) {
    (void)device;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    *value = pci_cfg_inb((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    *value = pci_cfg_inw((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    *value = pci_cfg_inl((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    pci_cfg_outb((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    pci_cfg_outw((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
    u32 a = uacpi_pci_handle_addr(device);
    if (offset > 0xFF) return UACPI_STATUS_UNIMPLEMENTED;
    pci_cfg_outl((a >> 16) & 0xFF, (a >> 11) & 0x1F, (a >> 8) & 0x07, (u8)offset, value);
    return UACPI_STATUS_OK;
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

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    return (void*)(HHDM_START + addr);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    (void)addr; (void)len;
}

void *uacpi_kernel_alloc(uacpi_size size) {
    return malloc(size);
}

void uacpi_kernel_free(void *mem) {
    free(mem);
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

/*
 * Interrupt handlers.
 *
 * The kernel registers one IDT/IOAPIC entry per IRQ via init_irq(),
 * each pointing at a per-IRQ asm stub (uacpi_int.asm) that passes its
 * IRQ number to uacpi_irq_dispatch(). uACPI only ever installs the SCI
 * (and occasionally GPE IRQs), so a small static table is enough.
 */

typedef struct {
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    uacpi_bool used;
} uacpi_irq_slot_t;

#define UACPI_MAX_IRQ 24

extern void (*uacpi_irq_stubs[UACPI_MAX_IRQ])(void);

static uacpi_irq_slot_t uacpi_irq_slots[UACPI_MAX_IRQ];

// called from the asm stubs in uacpi_int.asm
void uacpi_irq_dispatch(u64 irq) {
    if (irq < UACPI_MAX_IRQ && uacpi_irq_slots[irq].used &&
        uacpi_irq_slots[irq].handler) {
        uacpi_irq_slots[irq].handler(uacpi_irq_slots[irq].ctx);
    }
    pic_send_eoi((u8)irq);
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    if (irq >= UACPI_MAX_IRQ)
        return UACPI_STATUS_UNIMPLEMENTED;
    if (uacpi_irq_slots[irq].used)
        return UACPI_STATUS_ALREADY_EXISTS;

    uacpi_irq_slots[irq].handler = handler;
    uacpi_irq_slots[irq].ctx = ctx;
    uacpi_irq_slots[irq].used = UACPI_TRUE;

    init_irq((s32)irq, uacpi_irq_stubs[irq]);
    irq_enable((u8)irq);

    if (out_irq_handle)
        *out_irq_handle = (uacpi_handle)(u64)(irq + 1);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle
) {
    u64 idx = (u64)irq_handle - 1;
    (void)handler;
    if (idx >= UACPI_MAX_IRQ || !uacpi_irq_slots[idx].used)
        return UACPI_STATUS_NOT_FOUND;

    uacpi_irq_slots[idx].used = UACPI_FALSE;
    irq_disable((u8)idx);
    return UACPI_STATUS_OK;
}

/*
 * Deferred work queue.
 *
 * uACPI schedules GPE/notify work, possibly from interrupt context.
 * We queue it here and drain it from krunpolls() (every scheduler
 * tick) and from uacpi_kernel_wait_for_work_completion().
 */

typedef struct uacpi_work_item {
    uacpi_work_handler handler;
    uacpi_handle ctx;
    struct uacpi_work_item *next;
} uacpi_work_item;

static lock_t uacpi_work_lock = {0};
static uacpi_work_item *uacpi_work_head = NULL;
static uacpi_work_item *uacpi_work_tail = NULL;

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx
) {
    (void)type;
    uacpi_work_item *item = malloc(sizeof(*item));
    if (!item) return UACPI_STATUS_OUT_OF_MEMORY;

    item->handler = handler;
    item->ctx = ctx;
    item->next = NULL;

    u64 flags;
    lock_acquire(&uacpi_work_lock, &flags);
    if (uacpi_work_tail)
        uacpi_work_tail->next = item;
    else
        uacpi_work_head = item;
    uacpi_work_tail = item;
    lock_release(&uacpi_work_lock, &flags);

    return UACPI_STATUS_OK;
}

void uacpi_drain_work(void) {
    for (;;) {
        uacpi_work_item *item;
        u64 flags;
        lock_acquire(&uacpi_work_lock, &flags);
        item = uacpi_work_head;
        if (item) {
            uacpi_work_head = item->next;
            if (!uacpi_work_head)
                uacpi_work_tail = NULL;
        }
        lock_release(&uacpi_work_lock, &flags);

        if (!item) break;
        item->handler(item->ctx);
        free(item);
    }
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    uacpi_drain_work();
    return UACPI_STATUS_OK;
}