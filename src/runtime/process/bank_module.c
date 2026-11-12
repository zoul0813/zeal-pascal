#include "runtime/process/bank_module.h"
#include "runtime/process/chain.h"
OverlayBanks overlay_bank_state;
uint8_t chain_buffer[128],chain_store;
const uint16_t overlay_context_bytes=sizeof(OverlayBanks);
const uint16_t overlay_job_bytes=sizeof(OverlayJob);
void overlay_dispatch(OverlayJob *job) {
    OverlayBanks *b=&overlay_bank_state;uint16_t group;
    job->page=255;job->changed=0;job->error=0;
    if(!b->directory) {
        overlay_banks_init(b);
        if(!overlay_banks_bind(b,job->basename,job->directory,job->directory_bytes)) {
            job->error=b->error;return;
        }
    }
    if(job->operation==0) {
        if(job->value>=b->count){job->error=1;return;}
        group=b->directory[16u+16u*job->value]|((uint16_t)b->directory[17u+16u*job->value]<<8);
        job->changed=b->selected[group]!=job->value;
        job->page=overlay_banks_enter(b,job->value);
    } else if(job->operation==1)overlay_banks_leave(b,job->value);
    else if(job->operation==2)overlay_banks_set_drive(b,job->value);
    else if(job->operation==3)overlay_banks_close(b);
    else {job->error=1;return;}
    job->error=b->error;
}
