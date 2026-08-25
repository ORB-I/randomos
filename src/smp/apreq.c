#include <smp/smp.h>
#include <smp/apreq.h>
#include <smp/ipi.h>

void send_ap_request(u64 tgt_apicid, int type, void* data, usize data2) {
    usize i = 0;
    for (; i < ncores; i++) {
        if (apreqvec[i].apicid == tgt_apicid) {
            break;
        }
    }

    while (atomic_load(&apreqvec[i].lock));
    atomic_store(&apreqvec[i].lock, 1);

    atomic_store(&apreqvec[i].done, 0);
    apreqvec[i].apicid = get_apicid();
    apreqvec[i].type = type;
    apreqvec[i].data = data;
    apreqvec[i].data2 = data2;

    ipi_send(tgt_apicid, 0, IPI_TRIGGER_EDGE, IPI_LEVEL_DEASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_FIXED, AP_MSGVEC);
    
    while (!atomic_load(&apreqvec[i].done));
    atomic_store(&apreqvec[i].lock, 0);
}

// if no SMP is available, will return -1, else will return apicid of proc
int ap_run(void(*fn)(void*), void* arg) {
    usize i = 0;
    int found = 0;
    for (; i < (usize)ncores; ++i) {
        if (smp_info[i].status == SMP_STATUS_WAITING) {
            found = 1;
            break;
        }
    }
    if (!found) return -1;

    ap_runreq_t rreq = {fn, arg};
    send_ap_request(smp_info[i].apicid, AP_REQ_RUN, &rreq, 0);

    return smp_info[i].apicid;
}

void ap_pause(u64 apicid) {
    send_ap_request(apicid, AP_REQ_PAUSE, NULL, 0);
}

void ap_continue(u64 apicid) {
    send_ap_request(apicid, AP_REQ_CONT, NULL, 0);
}

void ap_stop(u64 apicid) {
    send_ap_request(apicid, AP_REQ_STOP, NULL, 0);
}