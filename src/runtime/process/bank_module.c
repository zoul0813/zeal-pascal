#include "runtime/process/bank_module.h"
#include "runtime/process/chain.h"
#include <stddef.h>
_Static_assert(offsetof(OverlayJob,operation)==8&&offsetof(OverlayJob,value)==10&&
    offsetof(OverlayJob,page)==12&&offsetof(OverlayJob,first_load)==13&&
    offsetof(OverlayJob,error)==14,"overlay dispatch ABI changed");
OverlayBanks overlay_bank_state;
uint8_t chain_buffer[128],chain_store;
const uint16_t overlay_context_bytes=sizeof(OverlayBanks);
const uint16_t overlay_job_bytes=sizeof(OverlayJob);
void overlay_dispatch(OverlayJob *job) {
    OverlayBanks *b=&overlay_bank_state;uint8_t owned;
    job->page=255;job->first_load=0;job->error=0;
    if(!b->directory) {
        overlay_banks_init(b);
        if(!overlay_banks_bind(b,job->basename,job->directory,job->directory_bytes)) {
            job->error=b->error;return;
        }
    }
    if(job->operation==0) {
        if(job->value>=b->count){job->error=1;return;}
        owned=b->owned_count;
        job->page=overlay_banks_enter(b,job->value);
        /* Typed-constant initialization belongs to first materialization,
         * never to a later change of the group's selected member. */
        job->first_load=job->page!=255&&b->owned_count!=owned;
    } else if(job->operation==1)overlay_banks_leave(b,job->value);
    else if(job->operation==2)overlay_banks_set_drive(b,job->value);
    else if(job->operation==3)overlay_banks_close(b);
    else {job->error=1;return;}
    job->error=b->error;
}
