#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/*
 * ============================================================================
 * OS / THREADING
 * ============================================================================
 */

#define NO_SYS                      1
#define SYS_LIGHTWEIGHT_PROT        0

#define LWIP_TIMERS                 1
#define LWIP_TIMERS_CUSTOM          0

#define LWIP_TCPIP_CORE_LOCKING     0
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0

/*
 * ============================================================================
 * MEMORY
 * ============================================================================
 *
 * Let lwIP use the kernel's malloc/free instead of lwIP's internal heap.
 */

#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1

#define MEM_ALIGNMENT               8

/*
 * These aren't used as the primary allocator when MEM_LIBC_MALLOC=1,
 * but leave reasonable values for pools/internal bookkeeping.
 */
#define MEM_SIZE                    (1024 * 1024)

/*
 * ============================================================================
 * PBUF
 * ============================================================================
 */

#define LWIP_SUPPORT_CUSTOM_PBUF    1

#define PBUF_POOL_SIZE              256
#define PBUF_POOL_BUFSIZE           1536
#define PBUF_LINK_HLEN              14

/*
 * ============================================================================
 * ARP / ETHERNET
 * ============================================================================
 */

#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              10
#define ARP_QUEUEING                1

#define ETHARP_SUPPORT_STATIC_ENTRIES 1
#define ETHARP_TABLE_MATCH_NETIF    1

/*
 * ============================================================================
 * IPv4
 * ============================================================================
 */

#define LWIP_IPV4                   1

#define IP_FORWARD                  0
#define IP_REASSEMBLY               1
#define IP_FRAG                     1

#define IP_OPTIONS_ALLOWED          1
#define IP_DEFAULT_TTL              255

/*
 * ============================================================================
 * IPv6
 * ============================================================================
 */

#define LWIP_IPV6                   1

#define LWIP_IPV6_MLD               1

/*
 * ============================================================================
 * ICMP
 * ============================================================================
 */

#define LWIP_ICMP                   1

#define ICMP_TTL                    255

#define LWIP_BROADCAST_PING         1
#define LWIP_MULTICAST_PING         1

/*
 * ============================================================================
 * RAW API
 * ============================================================================
 */

#define LWIP_RAW                    1

#define RAW_TTL                     255

/*
 * ============================================================================
 * UDP
 * ============================================================================
 */

#define LWIP_UDP                    1
#define LWIP_UDPLITE                1

#define UDP_TTL                     255

/*
 * ============================================================================
 * TCP
 * ============================================================================
 */

#define LWIP_TCP                    1

#define TCP_TTL                     255

#define TCP_QUEUE_OOSEQ             1
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (16 * TCP_MSS)
#define TCP_WND                     (16 * TCP_MSS)

#define TCP_SND_QUEUELEN            \
    ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define TCP_MAXRTX                  12
#define TCP_SYNMAXRTX               6

#define TCP_LISTEN_BACKLOG          1

#define LWIP_TCP_KEEPALIVE          1
#define LWIP_TCP_TIMESTAMPS         1

/*
 * ============================================================================
 * DHCP
 * ============================================================================
 */

#define LWIP_DHCP                   1

#define DHCP_DOES_ARP_CHECK         1
#define LWIP_DHCP_CHECK_LINK_UP     1

/*
 * ============================================================================
 * AUTOIP
 * ============================================================================
 */

#define LWIP_AUTOIP                 1
#define LWIP_DHCP_AUTOIP_COOP       1
#define LWIP_DHCP_AUTOIP_COOP_TRIES 9

/*
 * ============================================================================
 * DNS
 * ============================================================================
 */

#define LWIP_DNS                    1

#define DNS_TABLE_SIZE              8
#define DNS_MAX_SERVERS             3
#define DNS_MAX_NAME_LENGTH         256

/*
 * ============================================================================
 * NETIF
 * ============================================================================
 */

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

#define LWIP_NETIF_HOSTNAME         1

#define LWIP_NETIF_HWADDRHINT       1

/*
 * ============================================================================
 * CHECKSUMS
 * ============================================================================
 *
 * Software checksums. If your NIC supports checksum offload, these can
 * later be changed to 0 for the appropriate operations.
 */

#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_GEN_ICMP           1
#define CHECKSUM_GEN_ICMP6          1

#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#define CHECKSUM_CHECK_ICMP         1
#define CHECKSUM_CHECK_ICMP6        1

/*
 * ============================================================================
 * APIs
 * ============================================================================
 *
 * RAW works with NO_SYS=1.
 *
 * NETCONN and SOCKET require the OS abstraction/threading layer, so they
 * cannot be used in a true NO_SYS build.
 */

#define LWIP_RAW                    1

#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0

/*
 * ============================================================================
 * STATISTICS
 * ============================================================================
 */

#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/*
 * ============================================================================
 * DEBUG
 * ============================================================================
 */

#define LWIP_DEBUG                  0

/*
 * ============================================================================
 * LOOPBACK
 * ============================================================================
 */

#define LWIP_NETIF_LOOPBACK         1
#define LWIP_LOOPBACK_MAX_PBUFS     8

#include <drivers/rng/rng.h>
#define LWIP_RAND() (random64() & 0xFFFFFFFF)

#endif