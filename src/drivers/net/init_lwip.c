#include <core/errno.h>
#include <lwip/netif/ethernet.h>
#include <lwip/lwip/init.h>
#include <lwip/lwip/dhcp.h>
#include <lwip/lwip/ip4.h>
#include <lwip/lwip/netif.h>
#include <lwip/lwip/timeouts.h>
#include <drivers/time/clock.h>
#include <drivers/time/hpet.h>
#include <drivers/net/virtio_net.h>
#include <drivers/net/e1000.h>
#include <core/kprint.h>

err_t e1000_netifinit(struct netif* nf);
extern struct netif _e1000_netif;

static void netif_status_cb(struct netif *netif) {
    if (ip4_netif_get_local_ip(netif)) {
        const ip_addr_t* ip_addr = ip4_netif_get_local_ip(netif);
        if (ip_addr->type == IPADDR_TYPE_V4 || ip_addr->type == IPADDR_TYPE_ANY) {
            ip4_addr_t addr = ip_addr->u_addr.ip4;
            u8 a = ip4_addr1(&addr);
            u8 b = ip4_addr2(&addr);
            u8 c = ip4_addr3(&addr);
            u8 d = ip4_addr4(&addr);
            kprint("%u.%u.%u.%u\r\n", a, b, c, d);
        }
    }
}

void lwip_hpetcb() {
    sys_check_timeouts();
}

extern int _hpet_pollrun;
int init_lwip() {
    kprint("Initializing LwIP\n");
    lwip_init();

    ip4_addr_t ipv4;
    ip4_addr_t nmask;
    ip4_addr_t gw;

    IP4_ADDR(&ipv4, 0, 0, 0, 0);
    IP4_ADDR(&nmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    struct netif* nf = NULL;

    if (virtio_net_initialized) {
        kprint("Adding ethernet device VirtIO Net\n");
        nf = netif_add(&_virtio_netif, &ipv4,
            &nmask, &gw,
            NULL, virtio_net_netifinit,
            ethernet_input);
    } else {
        kprint("Adding ethernet device Intel(R) E1000\n");
        nf = netif_add(&_e1000_netif, &ipv4, 
            &nmask, &gw, 
            NULL, e1000_netifinit, 
            ethernet_input);
    }

    if (!nf) {
        kprint("Failed to add interface\n");
        return -ENOEXIST;
    }

    kprint("netif=%p\n", nf);
    
    kprint("Setting link up for ethernet\n");
    netif_set_status_callback(nf, netif_status_cb);
    kprint("Set status callback\n");
    netif_set_up(nf);
    kprint("Set up interface\n");
    netif_set_link_up(nf);
    kprint("Set link up interface\n");
    kprint("Setting default ethernet device\n");
    netif_set_default(nf);
    kprint("Set default NetIF\n");
    kprint("Starting DHCP\n");

    dhcp_start(nf);
    _hpet_pollrun = 1;
    return ERR_OK;
}

u32 sys_now() {
    return getms();
}