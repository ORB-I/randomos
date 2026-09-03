#include <stdatomic.h>
#include <core/std.h>
#include <smp/smp.h>
#include <smp/bspreq.h>
#include <smp/apreq.h>
#include <smp/ipi.h>
#include <core/panic.h>

bsp_request_t global_bspreq_buf = {0};

void send_bsp_request(u64 apicid, int type, void* data, usize datasz) {
    u64 rflags;
    lock_acquire(&global_bspreq_buf.lock, &rflags);

    global_bspreq_buf.apicid = apicid;
    global_bspreq_buf.type = type;
    global_bspreq_buf.data = data;
    global_bspreq_buf.data_sz = datasz;
    atomic_store(&global_bspreq_buf.done, 0);
    
    ipi_send(bsp_apicid, IPI_SHRTDST_NONE, IPI_TRIGGER_EDGE, IPI_LEVEL_ASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_FIXED, BSP_REQVEC);

    while (!atomic_load(&global_bspreq_buf.done));
    lock_release(&global_bspreq_buf.lock, &rflags);
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
            ipi_send(0, IPI_SHRTDST_ALLE, IPI_TRIGGER_EDGE, IPI_LEVEL_ASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_NMI, AP_STOPVEC);
            panic("SMP %lu Panicked", global_bspreq_buf.apicid);
        }
    }
    atomic_store(&global_bspreq_buf.done, 1);
}