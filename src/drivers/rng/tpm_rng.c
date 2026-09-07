#include <drivers/rng/tpm_rng.h>
#include <core/mem/vmm.h>
#include <core/kprint.h>
#include <lib/string.h>

#define TPM_TIS_PHYS_BASE 0xFED40000ULL

#define TPM_ACCESS_REG    0x0000
#define TPM_STS_REG       0x0018
#define TPM_DATA_FIFO_REG 0x0024
#define TPM_DID_VID_REG   0x0F00

#define TPM_ACCESS_REQUEST_USE   (1 << 1)
#define TPM_ACCESS_ACTIVE_LOCAL  (1 << 5)

#define TPM_STS_VALID            (1 << 7)
#define TPM_STS_COMMAND_READY    (1 << 6)
#define TPM_STS_GO               (1 << 5)
#define TPM_STS_DATA_AVAIL       (1 << 4)
#define TPM_STS_EXPECT           (1 << 3)

static volatile u8* tpm_base = NULL;
static bool tpm_is_2_0 = false;

static inline u8 tpm_read8(usize reg) {
    return *(volatile u8*)(tpm_base + reg);
}

static inline void tpm_write8(usize reg, u8 val) {
    *(volatile u8*)(tpm_base + reg) = val;
}

static inline u32 tpm_read32(usize reg) {
    return *(volatile u32*)(tpm_base + reg);
}

static int tpm_request_locality(void) {
    u8 access = tpm_read8(TPM_ACCESS_REG);
    if (access & TPM_ACCESS_ACTIVE_LOCAL) return 0;

    tpm_write8(TPM_ACCESS_REG, TPM_ACCESS_REQUEST_USE);
    for (int i = 0; i < 1000; i++) {
        access = tpm_read8(TPM_ACCESS_REG);
        if (access & TPM_ACCESS_ACTIVE_LOCAL) return 0;
        asm volatile("pause");
    }
    return -1;
}

bool tpm_rng_available(void) {
    volatile u8* mmio = (volatile u8*)(HHDM_START + TPM_TIS_PHYS_BASE);
    u32 did_vid = *(volatile u32*)(mmio + TPM_DID_VID_REG);
    if (did_vid == 0xFFFFFFFF || did_vid == 0x00000000) {
        return false;
    }
    return true;
}

int tpm_rng_init(void) {
    if (!tpm_rng_available()) {
        return -1;
    }
    tpm_base = (volatile u8*)(HHDM_START + TPM_TIS_PHYS_BASE);
    if (tpm_request_locality() < 0) {
        tpm_base = NULL;
        return -1;
    }

    u32 did_vid = tpm_read32(TPM_DID_VID_REG);
    u16 vid = did_vid & 0xFFFF;
    u16 did = (did_vid >> 16) & 0xFFFF;
    kprint("[RNG] TPM Hardware TRNG detected (VID: 0x%x, DID: 0x%x)\n", vid, did);
    return 0;
}

static int tpm_send_command(const u8* cmd, usize cmd_len) {
    tpm_write8(TPM_STS_REG, TPM_STS_COMMAND_READY);

    for (usize i = 0; i < cmd_len; i++) {
        tpm_write8(TPM_DATA_FIFO_REG, cmd[i]);
    }

    tpm_write8(TPM_STS_REG, TPM_STS_GO);
    return 0;
}

static int tpm_read_response(u8* rsp, usize max_len) {
    // Wait for data available
    bool ready = false;
    for (int retry = 0; retry < 5000; retry++) {
        u8 sts = tpm_read8(TPM_STS_REG);
        if ((sts & TPM_STS_VALID) && (sts & TPM_STS_DATA_AVAIL)) {
            ready = true;
            break;
        }
        asm volatile("pause");
    }
    if (!ready) return -1;

    usize read_bytes = 0;
    while (read_bytes < max_len) {
        u8 sts = tpm_read8(TPM_STS_REG);
        if (!(sts & TPM_STS_VALID) || !(sts & TPM_STS_DATA_AVAIL)) {
            break;
        }
        rsp[read_bytes++] = tpm_read8(TPM_DATA_FIFO_REG);
    }
    return (int)read_bytes;
}

int tpm_rng_read(u8* buf, usize len) {
    if (!tpm_base) return 0;

    usize offset = 0;
    while (offset < len) {
        u32 req = len - offset;
        if (req > 32) req = 32;

        u8 cmd[16];
        u8 resp[64];
        int resp_len = 0;

        if (!tpm_is_2_0) {
            // TPM 1.2 GetRandom
            cmd[0] = 0x00; cmd[1] = 0xC1;
            cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 14;
            cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x00; cmd[9] = 0x46;
            cmd[10] = (req >> 24) & 0xFF; cmd[11] = (req >> 16) & 0xFF;
            cmd[12] = (req >> 8) & 0xFF; cmd[13] = req & 0xFF;

            tpm_send_command(cmd, 14);
            resp_len = tpm_read_response(resp, sizeof(resp));

            if (resp_len >= 14) {
                u32 ret_code = (resp[6] << 24) | (resp[7] << 16) | (resp[8] << 8) | resp[9];
                if (ret_code == 0) {
                    u32 got_bytes = (resp[10] << 24) | (resp[11] << 16) | (resp[12] << 8) | resp[13];
                    if (got_bytes > (usize)(resp_len - 14)) got_bytes = resp_len - 14;
                    memcpy(buf + offset, resp + 14, got_bytes);
                    offset += got_bytes;
                    continue;
                } else if (ret_code == 0x1e) {
                    tpm_is_2_0 = true;
                }
            }
        }

        if (tpm_is_2_0) {
            // TPM 2.0 GetRandom
            cmd[0] = 0x80; cmd[1] = 0x01;
            cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 12;
            cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x7B;
            cmd[10] = (req >> 8) & 0xFF; cmd[11] = req & 0xFF;

            tpm_send_command(cmd, 12);
            resp_len = tpm_read_response(resp, sizeof(resp));

            if (resp_len >= 12) {
                u32 ret_code = (resp[6] << 24) | (resp[7] << 16) | (resp[8] << 8) | resp[9];
                if (ret_code == 0 && resp_len >= 14) {
                    u16 got_bytes = (resp[10] << 8) | resp[11];
                    if (got_bytes > (usize)(resp_len - 14)) got_bytes = resp_len - 14;
                    memcpy(buf + offset, resp + 14, got_bytes);
                    offset += got_bytes;
                    continue;
                }
            }
        }


        break;
    }

    return (int)offset;
}
