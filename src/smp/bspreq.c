#include <stdatomic.h>
#include <core/std.h>
#include <smp/smp.h>
#include <smp/bspreq.h>
#include <smp/apreq.h>
#include <smp/ipi.h>
#include <core/panic.h>

bsp_request_t global_bspreq_buf = {0};

void send_bsp_request(u64 apicid, int type, void* data, usize datasz) {
    while (atomic_load(&global_bspreq_buf.lock));
    atomic_store(&global_bspreq_buf.lock, 1);

    global_bspreq_buf.apicid = apicid;
    global_bspreq_buf.type = type;
    global_bspreq_buf.data = data;
    global_bspreq_buf.data_sz = datasz;
    atomic_store(&global_bspreq_buf.done, 0);
    
    ipi_send(bsp_apicid, IPI_SHRTDST_NONE, IPI_TRIGGER_EDGE, IPI_LEVEL_DEASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_FIXED, BSP_REQVEC);

    while (!atomic_load(&global_bspreq_buf.done));
    atomic_store(&global_bspreq_buf.lock, 0);
}

void bsp_request_hdlr_c() {
    switch (global_bspreq_buf.type) {
        case BSP_REQ_SETSTAT: {
            for (usize i = 0; i < ncores; i++) {
                if (smp_info[i].apicid == global_bspreq_buf.apicid) {
                    smp_info[i].status = (u8)global_bspreq_buf.data_sz;
                    break;
                }
            }
            break;
        }
        case BSP_REQ_PANIC: {
            ipi_send(0, IPI_SHRTDST_ALLE, IPI_TRIGGER_EDGE, IPI_LEVEL_DEASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_FIXED, AP_STOPVEC);
            panic("SMP %d Panicked", global_bspreq_buf.apicid);
        }
    }
    atomic_store(&global_bspreq_buf.done, 1);
}