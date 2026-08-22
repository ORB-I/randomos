#include <core/std.h>
#include <core/asmh.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/mem/pmm.h>
#include <lib/string.h>
#include <core/mem/vmm.h>

#include <drivers/pci.h>
#include <drivers/storage/ahci.h>

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_PROGIF_AHCI 0x01

#define HBA_CAP 0x00
#define HBA_GHC 0x04
#define HBA_IS 0x08
#define HBA_PI 0x0C

#define HBA_PORT_CMD 0x18
#define HBA_PORT_TFD 0x20
#define HBA_PORT_SIG 0x24
#define HBA_PORT_SSTS 0x28
#define HBA_PORT_SCTL 0x2C
#define HBA_PORT_SERR 0x30
#define HBA_PORT_SACT 0x34
#define HBA_PORT_CI 0x38

#define HBA_PORT_CMD_ST (1U << 0)
#define HBA_PORT_CMD_FRE (1U << 4)
#define HBA_PORT_CMD_CCS (1U << 16)
#define HBA_PORT_CMD_CR (1U << 15)
#define HBA_PORT_CMD_FR (1U << 14)

#define HBA_PORT_TFD_BSY (1U << 0)
#define HBA_PORT_TFD_DRQ (1U << 3)
#define HBA_PORT_TFD_ERR (1U << 8)

#define HBA_PORT_SSTS_DET_MASK 0xF
#define HBA_PORT_SSTS_SPD_MASK 0xF0

#define FIS_TYPE_REG_H2D 0x27
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define CMD_HEADER_CF_MASK 0x1F
#define CMD_HEADER_W (1U << 6)
#define CMD_HEADER_P (1U << 7)

typedef struct {
    u8 type;
    u8 pmport;
    u8 cmd;
    u8 featl;
    u8 feath;
    u8 lba0;
    u8 lba1;
    u8 lba2;
    u8 device;
    u8 lba3;
    u8 lba4;
    u8 lba5;
    u8 countl;
    u8 counth;
    u8 icc;
    u8 ctrl;
    u8 reserved[44];
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct {
    volatile u16 info;
    volatile u16 prdtl;
    volatile u32 prdbc;
    volatile u64 ctba;
    volatile u8 reserved[16];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    u8 cfis[64];
    u8 acmd[16];
    u8 reserved[48];
} __attribute__((packed)) hba_cmd_table_t;

typedef struct {
    volatile u64 dba;
    volatile u32 dbc;
    volatile u32 reserved;
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct {
    volatile u32 cap;
    volatile u32 ghc;
    volatile u32 is;
    volatile u32 pi;
    volatile u32 vs;
    volatile u32 ccc_ctl;
    volatile u32 ccc_ports;
    volatile u32 em_loc;
    volatile u32 em_ctl;
    volatile u32 ext_cap;
    u8 reserved[0xA0 - 0x28];
    volatile u32 orch;
    u8 reserved2[0x100 - 0xAC];
} __attribute__((packed)) hba_global_t;

typedef struct {
    volatile u32 clb;
    volatile u32 clbu;
    volatile u32 fb;
    volatile u32 fbu;
    volatile u32 is;
    volatile u32 ie;
    volatile u32 cmd;
    volatile u32 rsv0;
    volatile u32 tfd;
    volatile u32 sig;
    volatile u32 ssts;
    volatile u32 sctl;
    volatile u32 serr;
    volatile u32 sact;
    volatile u32 ci;
    volatile u32 snf;
    volatile u32 fbs;
    u8 reserved[0x80 - 0x44];
} __attribute__((packed)) ahci_port_regs_t;

static volatile hba_global_t* hba;
static int ahci_port = -1;
static u8 ahci_bus, ahci_slot, ahci_fn;

static u64 cmd_list_phys;
static u64 cmd_table_phys;
static u64 fis_phys;
static u64 dma_buf_phys;
static void* cmd_list_virt;
static void* cmd_table_virt;
static void* fis_virt;
static u8* dma_buf;

#define AHCI_PORT(port) ((volatile ahci_port_regs_t*)((u8*)hba + 0x100 + (port) * 0x80))

static int ahci_find_controller(void) {
    for (u8 bus = 0; bus < 4; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            for (u8 fn = 0; fn < 8; fn++) {
                pci_chdr_t hdr;
                pci_get_chdr_fn(bus, slot, fn, &hdr);
                if (hdr.vndid == 0xFFFF) continue;
                if (hdr.cls == PCI_CLASS_MASS_STORAGE &&
                    hdr.subcls == PCI_SUBCLASS_SATA &&
                    hdr.progif == PCI_PROGIF_AHCI) {
                    ahci_bus = bus;
                    ahci_slot = slot;
                    ahci_fn = fn;
                    return 0;
                }
            }
        }
    }
    return -1;
}

static u64 ahci_read_bar5(void) {
    u32 bar5_low = pci_cfg_inl(ahci_bus, ahci_slot, ahci_fn, 0x24);
    u32 bar5_high = 0;
    if (bar5_low & 0x4) {
        bar5_high = pci_cfg_inl(ahci_bus, ahci_slot, ahci_fn, 0x28);
    }
    return ((u64)bar5_high << 32) | (bar5_low & 0xFFFFFFF0);
}

static int ahci_wait_idle(int port, u32 timeout) {
    volatile ahci_port_regs_t* p = AHCI_PORT(port);
    for (u32 i = 0; i < timeout; i++) {
        u32 tfd = p->tfd;
        if (!(tfd & (HBA_PORT_TFD_BSY | HBA_PORT_TFD_DRQ))) {
            return 0;
        }
    }
    return -1;
}

static int ahci_port_init(int port) {
    volatile ahci_port_regs_t* p = AHCI_PORT(port);

    // Stop port
    p->cmd = 0;

    // Wait for CMD.CR and CMD.FR to clear
    for (u32 i = 0; i < 100000; i++) {
        if (!(p->cmd & (HBA_PORT_CMD_CR | HBA_PORT_CMD_FR))) break;
    }

    // Wait for port idle
    if (ahci_wait_idle(port, 100000) < 0) {
        return -1;
    }

    // Clear interrupts
    p->is = 0xFFFFFFFF;

    // Set command list base address
    p->clb = (u32)(cmd_list_phys & 0xFFFFFFFF);
    p->clbu = (u32)((cmd_list_phys >> 32) & 0xFFFFFFFF);

    // Set FIS base address
    p->fb = (u32)(fis_phys & 0xFFFFFFFF);
    p->fbu = (u32)((fis_phys >> 32) & 0xFFFFFFFF);

    // Enable FIS receive
    p->cmd = HBA_PORT_CMD_FRE;

    // Wait for FRE to be set
    for (u32 i = 0; i < 100000; i++) {
        if (p->cmd & HBA_PORT_CMD_FRE) break;
    }

    // Wait for device detect
    for (u32 i = 0; i < 100000; i++) {
        if ((p->ssts & HBA_PORT_SSTS_DET_MASK) == 0x3) break;
    }

    if ((p->ssts & HBA_PORT_SSTS_DET_MASK) != 0x3) {
        return -1;
    }

    // Start port
    p->cmd = HBA_PORT_CMD_FRE | HBA_PORT_CMD_ST;

    // Wait for ST to be set
    for (u32 i = 0; i < 100000; i++) {
        if (p->cmd & HBA_PORT_CMD_ST) break;
    }

    return 0;
}

static int ahci_issue_cmd(int port, u64 lba, u32 count, u8* buf, int write) {
    volatile ahci_port_regs_t* p = AHCI_PORT(port);

    // Build command FIS
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)fis_virt;
    memset(fis, 0, sizeof(*fis));
    fis->type = FIS_TYPE_REG_H2D;
    fis->pmport = 0x80;
    fis->cmd = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->device = 0x40;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->lba4 = (lba >> 32) & 0xFF;
    fis->lba5 = (lba >> 40) & 0xFF;
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    // Build command header
    hba_cmd_header_t* cmd = (hba_cmd_header_t*)cmd_list_virt;
    memset(cmd, 0, sizeof(*cmd));
    cmd->info = write ? (CMD_HEADER_P | CMD_HEADER_W | 3) : (CMD_HEADER_P | 3);
    cmd->prdtl = 1;
    cmd->ctba = cmd_table_phys;

    // Build command table
    hba_cmd_table_t* tbl = (hba_cmd_table_t*)cmd_table_virt;
    memset(tbl, 0, sizeof(*tbl));
    memcpy(tbl->cfis, fis, sizeof(*fis));

    // Build PRDT entry
    hba_prdt_entry_t* prdt = (hba_prdt_entry_t*)((u8*)tbl + sizeof(hba_cmd_table_t));
    prdt->dba = dma_buf_phys;
    prdt->dbc = (count * 512 - 1) | (1U << 31);

    // Copy data for write
    if (write) {
        memcpy(dma_buf, buf, count * 512);
    }

    // Issue command
    p->ci = 1;

    // Wait for completion
    for (u32 i = 0; i < 1000000; i++) {
        if (!(p->ci & 1)) break;
    }

    // Check errors
    u32 tfd = p->tfd;
    if (tfd & HBA_PORT_TFD_ERR) {
        p->serr = 0xFFFFFFFF;
        return -1;
    }

    // Copy data for read
    if (!write) {
        memcpy(buf, dma_buf, count * 512);
    }

    return 0;
}

int ahci_init(void) {
    if (ahci_find_controller() < 0) {
        printf("AHCI: No controller found\n");
        return -1;
    }

    u64 hba_phys = ahci_read_bar5();
    if (!hba_phys) {
        printf("AHCI: Invalid BAR5\n");
        return -1;
    }

    hba = (volatile hba_global_t*)(HHDM_START + hba_phys);

    // Allocate DMA buffers
    cmd_list_phys = (u64)pmm_falloc(1);
    cmd_table_phys = (u64)pmm_falloc(1);
    fis_phys = (u64)pmm_falloc(1);
    dma_buf_phys = (u64)pmm_falloc(1);

    if (!cmd_list_phys || !cmd_table_phys || !fis_phys || !dma_buf_phys) {
        printf("AHCI: DMA allocation failed\n");
        return -1;
    }

    cmd_list_virt = (void*)(HHDM_START + cmd_list_phys);
    cmd_table_virt = (void*)(HHDM_START + cmd_table_phys);
    fis_virt = (void*)(HHDM_START + fis_phys);
    dma_buf = (u8*)(HHDM_START + dma_buf_phys);

    // Find and initialize a port
    u32 pi = hba->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1U << i)) {
            if (ahci_port_init(i) == 0) {
                ahci_port = i;
                printf("AHCI: Initialized on port %d\n", i);
                return 0;
            }
        }
    }

    printf("AHCI: No device found on any port\n");
    return -1;
}

void ahci_secread(u8 drv, u64 lba, u8* buf) {
    if (ahci_port < 0) return;
    if (ahci_issue_cmd(ahci_port, lba, 1, buf, 0) < 0) {
        printf("AHCI: Read error at LBA %d\n", lba);
    }
}

void ahci_secwrite(u8 drv, u64 lba, u8* buf) {
    if (ahci_port < 0) return;
    if (ahci_issue_cmd(ahci_port, lba, 1, buf, 1) < 0) {
        printf("AHCI: Write error at LBA %d\n", lba);
    }
}
