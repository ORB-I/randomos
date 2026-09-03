#pragma once

#include <core/std.h>

/* E1000 Register Offsets */
#define E1000_REG_CTRL          0x0000  /* Device Control */
#define E1000_REG_STATUS        0x0008  /* Device Status */
#define E1000_REG_EECD          0x0010  /* EEPROM Control */
#define E1000_REG_EERD          0x0014  /* EEPROM Read */
#define E1000_REG_CTRL_EXT      0x0018  /* Extended Device Control */
#define E1000_REG_FLA           0x001C  /* Flash Access */
#define E1000_REG_MDIC          0x0020  /* MDI Control */
#define E1000_REG_ICR           0x00C0  /* Interrupt Cause Read */
#define E1000_REG_ITR           0x00C4  /* Interrupt Throttling Rate */
#define E1000_REG_ICS           0x00C8  /* Interrupt Cause Set */
#define E1000_REG_IMS           0x00D0  /* Interrupt Mask Set */
#define E1000_REG_IMC           0x00D8  /* Interrupt Mask Clear */
#define E1000_REG_RCTL          0x0100  /* Receive Control */
#define E1000_REG_FCTTV         0x0170  /* Flow Control Transmit Timer Value */
#define E1000_REG_TXCW          0x0178  /* Transmit Configuration Word */
#define E1000_REG_RXCW          0x0180  /* Receive Configuration Word */
#define E1000_REG_TCTL          0x0400  /* Transmit Control */
#define E1000_REG_TIPG          0x0410  /* Transmit Inter Packet Gap */
#define E1000_REG_AIT           0x0458  /* Adaptive IFS Throttle */
#define E1000_REG_LEDCTL        0x0E00  /* LED Control */
#define E1000_REG_PBA           0x1000  /* Packet Buffer Allocation */
#define E1000_REG_RDBAL         0x2800  /* Receive Descriptor Base Address Low */
#define E1000_REG_RDBAH         0x2804  /* Receive Descriptor Base Address High */
#define E1000_REG_RDLEN         0x2808  /* Receive Descriptor Length */
#define E1000_REG_RDH           0x2810  /* Receive Descriptor Head */
#define E1000_REG_RDT           0x2818  /* Receive Descriptor Tail */
#define E1000_REG_RDTR          0x2820  /* Receive Delay Timer */
#define E1000_REG_RADV          0x282C  /* Receive Interrupt Absolute Delay Timer */
#define E1000_REG_TDBAL         0x3800  /* Transmit Descriptor Base Address Low */
#define E1000_REG_TDBAH         0x3804  /* Transmit Descriptor Base Address High */
#define E1000_REG_TDLEN         0x3808  /* Transmit Descriptor Length */
#define E1000_REG_TDH           0x3810  /* Transmit Descriptor Head */
#define E1000_REG_TDT           0x3818  /* Transmit Descriptor Tail */
#define E1000_REG_TIDV          0x3820  /* Transmit Interrupt Delay Value */
#define E1000_REG_TXDCTL        0x3828  /* Transmit Descriptor Control */
#define E1000_REG_TADV          0x382C  /* Transmit Absolute Interrupt Delay Value */
#define E1000_REG_RXCSUM        0x5000  /* Receive Checksum Control */
#define E1000_REG_MTA           0x5200  /* Multicast Table Array (128 entries: 0x5200 + 4*i) */
#define E1000_REG_RAL           0x5400  /* Receive Address Low 0 */
#define E1000_REG_RAH           0x5404  /* Receive Address High 0 */

/* Control Register (CTRL) Flags */
#define E1000_CTRL_FD           (1U << 0)   /* Full Duplex */
#define E1000_CTRL_LRST         (1U << 3)   /* Link Reset */
#define E1000_CTRL_ASDE         (1U << 5)   /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU          (1U << 6)   /* Set Link Up */
#define E1000_CTRL_ILOS         (1U << 7)   /* Invert Loss-of-Signal */
#define E1000_CTRL_RST          (1U << 26)  /* Device Reset */
#define E1000_CTRL_VME          (1U << 30)  /* VLAN Mode Enable */
#define E1000_CTRL_PHY_RST      (1U << 31)  /* PHY Reset */

/* Status Register (STATUS) Flags */
#define E1000_STATUS_FD         (1U << 0)   /* Full Duplex */
#define E1000_STATUS_LU         (1U << 1)   /* Link Up */
#define E1000_STATUS_SPEED_MASK (3U << 6)
#define E1000_STATUS_SPEED_10M  (0U << 6)
#define E1000_STATUS_SPEED_100M (1U << 6)
#define E1000_STATUS_SPEED_1G   (2U << 6)

/* EEPROM Read Register (EERD) Flags */
#define E1000_EERD_START        (1U << 0)   /* Start EEPROM Read */
#define E1000_EERD_DONE         (1U << 4)   /* EEPROM Read Done */
#define E1000_EERD_ADDR_SHIFT   8
#define E1000_EERD_DATA_SHIFT   16

/* Interrupt Cause / Mask Flags */
#define E1000_ICR_TXDW          (1U << 0)   /* Transmit Descriptor Written Back */
#define E1000_ICR_TXQE          (1U << 1)   /* Transmit Queue Empty */
#define E1000_ICR_LSC           (1U << 2)   /* Link Status Change */
#define E1000_ICR_RXSEQ         (1U << 3)   /* Receive Sequence Error */
#define E1000_ICR_RXDMT0        (1U << 4)   /* Receive Descriptor Minimum Threshold */
#define E1000_ICR_RXO           (1U << 6)   /* Receive Overrun */
#define E1000_ICR_RXT0          (1U << 7)   /* Receiver Timer / Packet Received */

/* Receive Control Register (RCTL) Flags */
#define E1000_RCTL_EN           (1U << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP          (1U << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE          (1U << 3)   /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE          (1U << 4)   /* Multicast Promiscuous Enable */
#define E1000_RCTL_LPE          (1U << 5)   /* Long Packet Enable */
#define E1000_RCTL_LBM_NONE     (0U << 6)   /* No Loopback */
#define E1000_RCTL_RDMTS_HALF   (0U << 8)   /* Free Buffer Threshold Half */
#define E1000_RCTL_BAM          (1U << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048   (0U << 16)  /* Buffer Size 2048 bytes */
#define E1000_RCTL_BSIZE_1024   (1U << 16)
#define E1000_RCTL_BSIZE_512    (2U << 16)
#define E1000_RCTL_BSIZE_256    (3U << 16)
#define E1000_RCTL_BSEX         (1U << 25)  /* Buffer Size Extension */
#define E1000_RCTL_SECRC        (1U << 26)  /* Strip Ethernet CRC */

/* Transmit Control Register (TCTL) Flags */
#define E1000_TCTL_EN           (1U << 1)   /* Transmitter Enable */
#define E1000_TCTL_PSP          (1U << 3)   /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT     4           /* Collision Threshold */
#define E1000_TCTL_COLD_SHIFT   12          /* Collision Distance */
#define E1000_TCTL_SWXOFF       (1U << 22)  /* Software XOFF Transmission */
#define E1000_TCTL_RTLC         (1U << 24)  /* Re-transmit on Late Collision */

/* Receive Address High (RAH) Flags */
#define E1000_RAH_AV            (1U << 31)  /* Address Valid */

/* Receive Descriptor Status */
#define E1000_RXD_STAT_DD       (1U << 0)   /* Descriptor Done */
#define E1000_RXD_STAT_EOP      (1U << 1)   /* End of Packet */
#define E1000_RXD_STAT_IXSM     (1U << 2)   /* Ignore Checksum */
#define E1000_RXD_STAT_VP       (1U << 3)   /* 802.1Q Packet */
#define E1000_RXD_STAT_TCPCS    (1U << 5)   /* TCP Checksum */
#define E1000_RXD_STAT_IPCS     (1U << 6)   /* IP Checksum */
#define E1000_RXD_STAT_PIF      (1U << 7)   /* Passed In-exact Filter */

/* Transmit Descriptor Command */
#define E1000_TXD_CMD_EOP       (1U << 0)   /* End of Packet */
#define E1000_TXD_CMD_IFCS      (1U << 1)   /* Insert FCS / CRC */
#define E1000_TXD_CMD_IC        (1U << 2)   /* Insert Checksum */
#define E1000_TXD_CMD_RS        (1U << 3)   /* Report Status */
#define E1000_TXD_CMD_RPS       (1U << 4)   /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT      (1U << 5)   /* Descriptor Extension */
#define E1000_TXD_CMD_VLE       (1U << 6)   /* VLAN Packet Enable */
#define E1000_TXD_CMD_IDE       (1U << 7)   /* Interrupt Delay Enable */

/* Transmit Descriptor Status */
#define E1000_TXD_STAT_DD       (1U << 0)   /* Descriptor Done */
#define E1000_TXD_STAT_EC       (1U << 1)   /* Excess Collisions */
#define E1000_TXD_STAT_LC       (1U << 2)   /* Late Collision */
#define E1000_TXD_STAT_TU       (1U << 3)   /* Transmit Underrun */

#define E1000_NUM_RX_DESC       32
#define E1000_NUM_TX_DESC       32
#define E1000_PKT_BUF_SIZE      2048

/* Legacy Receive Descriptor */
typedef struct {
    volatile u64 addr;
    volatile u16 length;
    volatile u16 checksum;
    volatile u8  status;
    volatile u8  errors;
    volatile u16 special;
} __packed e1000_rx_desc_t;

/* Legacy Transmit Descriptor */
typedef struct {
    volatile u64 addr;
    volatile u16 length;
    volatile u8  cso;
    volatile u8  cmd;
    volatile u8  status;
    volatile u8  css;
    volatile u16 special;
} __packed e1000_tx_desc_t;

typedef void (*e1000_rx_callback_t)(const void* packet, u16 len);

/* Public Driver Interface */
int  e1000_init(void);
int  e1000_send(const void* data, u16 len);
int  e1000_receive(void* buf, u16 max_len);
void e1000_get_mac(u8* mac_out);
bool e1000_link_up(void);
void e1000_set_rx_callback(e1000_rx_callback_t cb);
