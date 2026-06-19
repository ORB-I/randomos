#pragma once
#include <core/std.h>

typedef struct {
    char sig[4];
    u32 len;
    u8 rev;
    u8 chksum;
    char oem_id[6];
    char oem_tblid[8];
    u32 oem_rev;
    u32 creat_id;
    u32 creat_rev;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    char sig[8];
    u8 chksum;
    char oem[6];
    u8 rev;
    u32 rsdt_addr;

    u32 len;
    u64 xsdt_addr;
    u8 ext_chksum;
    u8 reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    sdt_header_t hdr;
    uint64_t entries[];
} __attribute__((packed)) xsdt_t;

typedef struct {
    sdt_header_t hdr;
    u32 entries[];
} __attribute__((packed)) rsdt_t;

static inline u32 xsdt_entries(xsdt_t* xsdt) {
    return (xsdt->hdr.len - sizeof(sdt_header_t)) / 4;
}

#define ADDRSPACE_SYS     0
#define ADDRSPACE_IO      1
#define ADDRSPACE_PCICFG  2 
#define ADDRSPACE_CTL     3
#define ADDRSPACE_SMB     4
#define ADDRSPACE_CMOS    5
#define ADDRSPACE_PCIDEV  6
#define ADDRSPACE_IPMI    7
#define ADDRSPACE_GPIO    8
#define ADDRSPACE_GSB     9
#define ADDRSPACE_PCC     10

#define ACCESS8  1
#define ACCESS16 2
#define ACCESS32 3
#define ACCESS64 4

typedef struct {
  u8 space;
  u8 bitwdth;
  u8 bitoff;
  u8 accsz;
  uint64_t addr;
} __attribute__((packed)) genaddr_t;

typedef struct {
    sdt_header_t hdr;
    u32 fmw_ctrl;
    u32 dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    u8  __rsvd;

    u8  pwr_mgmt_prof;
    u16 sci_int;
    u32 smi_port;
    u8  acpi_en;
    u8  acpi_dis;
    u8  s4bios_req;
    u8  pstate_ctrl;
    u32 pm1a_evt_block;
    u32 pm1b_evt_block;
    u32 pm1a_ctrl_block;
    u32 pm1b_ctrl_block;
    u32 pm2_ctrl_block;
    u32 pmtimer_block;
    u32 gpe0_block;
    u32 gpe1_block;
    u8  pm1_evt_len;
    u8  pm1_ctrl_len;
    u8  pm2_ctrl_len;
    u8  pmtimer_len;
    u8  gpe0_len;
    u8  gpe1_len;
    u8  gpe1_base;
    u8  cstate_strl;
    u16 worst_c2ltc;
    u16 worst_c3ltc;
    u16 flushsz;
    u16 flush_strd;
    u8  duty_off;
    u8  duty_wdth;
    u8  day_alrm;
    u8  mon_alrm;
    u8  century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    u16 boot_arch_flgs;

    u8  __resvd2;
    u32 flgs;

    // 12 byte structure; see below for details
    genaddr_t rst_reg;

    u8  rst_val;
    u8  __rsvd3[3];
  
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                xfmw_ctrl;
    uint64_t                xdsdt;

    genaddr_t xpm1a_evt_block;
    genaddr_t xpm1b_evt_block;
    genaddr_t xpm1a_ctrl_block;
    genaddr_t xpm1b_ctrl_block;
    genaddr_t xpm2_ctrl_block;
    genaddr_t xpmtimer_block;
    genaddr_t xgpe0_block;
    genaddr_t xgpe1_block;
} __attribute__((packed)) fadt_t;


typedef struct {
    rsdp_t* rsdp;
    xsdt_t* xsdt;
    fadt_t* fadt;
} core_acpi_t;

void init_acpi(core_acpi_t* acpi);

s32 acpi_write8(genaddr_t* addr, u8 val);
s32 acpi_write16(genaddr_t* addr, u16 val);
s32 acpi_write32(genaddr_t* addr, u32 val);

s32 acpi_read8(genaddr_t* addr, u8* out);
s32 acpi_read16(genaddr_t* addr, u16* out);
s32 acpi_read32(genaddr_t* addr, u32* out);

void set_lai_acpi(core_acpi_t* acpi);