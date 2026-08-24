#include <lwip/lwip/timeouts.h>
#include <core/printf.h>
#include <lwip/lwip/ip4.h>
#include <lwip/lwip/dhcp.h>
#include <lwip/lwip/prot/dhcp.h>

extern struct netif _e1000_netif;

void krunpolls() {
    sys_check_timeouts();
}