#include <lwip/lwip/timeouts.h>
#include <core/printf.h>
#include <lwip/lwip/ip4.h>
#include <lwip/lwip/dhcp.h>
#include <lwip/lwip/prot/dhcp.h>

extern struct netif _e1000_netif;
int smp_getactive();

void krunpolls() {
    // lwIP is built NO_SYS: its timeout list, heap and pbuf pools have
    // zero internal locking. The e1000 rx irq drives the same core from
    // interrupt context, so timers must never be pumped with IF set or
    // the rx irq can nest mid-walk and corrupt next_timeout.
    u64 flags;
    asm volatile("pushfq\n\tpopq %0\n\tcli" : "=r"(flags) :: "memory");
    sys_check_timeouts();
    if (flags & 0x200) asm volatile("sti" ::: "memory");
}