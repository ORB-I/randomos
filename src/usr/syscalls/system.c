#include "ssc.h"
#include "../ensurance.h"
#include <uacpi/sleep.h>
#include <core/mem/vmm.h>
#include <drivers/rng/rng.h>

DEFSYSCALL(sys_reboot) {
    (void)args;
    if (uacpi_likely_success(uacpi_reboot())) return 0;
    return -EUNKNOWN;
}

DEFSYSCALL(sys_poweroff) {
    (void)args;
    uacpi_status st = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_likely_error(st)) return -EUNKNOWN;
    st = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_likely_error(st)) return -EUNKNOWN;
    return 0;
}

DEFSYSCALL(sys_mmap) {
    return (u64)user_mmap(vmm_cpml4v(), (void*)args->a0, args->a1, args->a2, args->a3);
}

DEFSYSCALL(sys_munmap) {
    return user_munmap(vmm_cpml4v(), (void*)args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_mprotect) {
    return user_mprotect(vmm_cpml4v(), (void*)args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_random64) {
    (void)args;
    return random64();
}

DEFSYSCALL(sys_randombytes) {
    if (!ensure_pointer((void*)args->a0, args->a1, 1)) return -EINVAL;
    return random_bytes((u8*)args->a0, args->a1);
}