#include "ssc.h"
#include "../ensurance.h"
#include <lai/helpers/pm.h>
#include <core/mem/vmm.h>
#include <drivers/rng/rng.h>

DEFSYSCALL(sys_reboot) {
    (void)args;
    if (lai_acpi_reset() == 0) return 0;
    return -1;
}

DEFSYSCALL(sys_poweroff) {
    (void)args;
    if (lai_enter_sleep(5) == 0) return 0;
    else return -1;
}

DEFSYSCALL(sys_mmap) {
    return (u64)user_mmap(vmm_cpml4v(), (void*)args->a0, args->a1);
}

DEFSYSCALL(sys_munmap) {
    return user_munmap(vmm_cpml4v(), (void*)args->a0, args->a1);
}

DEFSYSCALL(sys_random64) {
    (void)args;
    return random64();
}

DEFSYSCALL(sys_randombytes) {
    if (!ensure_pointer((void*)args->a0, args->a1, 1)) return -1;
    return random_bytes((u8*)args->a0, args->a1);
}