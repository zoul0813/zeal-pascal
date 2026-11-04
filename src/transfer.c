#include "transfer.h"
#include <string.h>
#include <zos_sys.h>

#ifdef ZP_TRANSFER_HOST
extern uint8_t transfer_memory[65536];
#define PTR(at) (transfer_memory+(at))
#else
#define PTR(at) ((uint8_t *)(at))
#endif
#define BYTE(at) (*PTR(at))
ImageLoad transfer_load;
static ChainCaller caller;
static char path[128];
static uint8_t initialized;
static void initialize(void) {
    if(!initialized) {chain_init(&transfer_load);initialized=1;}
}
static void first(FileCleanup *job,uint16_t error,uint16_t detail) {
    if(!job->error) {job->error=error;job->detail=detail;}
}
uint8_t transfer_cleanup(FileCleanup *job) CHAIN_CALL {
    FileCleanup files;
    uint8_t error;
    initialize();
    transfer_load.error=0;transfer_load.detail=0;
    chain_discard(&transfer_load);
    if(transfer_load.error)first(job,transfer_load.error,transfer_load.detail);
    if(BYTE(ZT_FILE)) {
        memset(&files,0,sizeof(files));files.operation=ZF_END_PROGRAM;
        /* Mode1 never frees a file module on an undocumented return register.
         * Its cleanup uses the owned job; no idle disk descriptors remain. */
        transfer_file_call(1,&files);
        if(files.error)first(job,files.error,files.detail);
        else {
            error=pfree(BYTE(ZT_FILE));
            if(error)first(job,0x1ff,error);else BYTE(ZT_FILE)=0;
        }
    }
    return transfer_load.pages[0]!=255||transfer_load.pages[1]!=255||
#ifndef ZP_CHAIN_ONLY
        transfer_load.pages[2]!=255||
#endif
        BYTE(ZT_FILE)!=0;
}
void transfer_dispatch(TransferJob *job) CHAIN_CALL {
    uint16_t i;
#ifndef ZP_CHAIN_ONLY
    FileCleanup executing;uint8_t originals[3],owners[1],owner_count;
#endif
#ifndef ZP_TRANSFER_HOST
    if((uint16_t)job<0xc800u||(uint16_t)job>0xfc00u-sizeof(*job)) {
        BYTE(0xfe7f)=ERR_ENTRY_CORRUPTED;return;
    }
#endif
    initialize();job->error=job->detail=0;
    /* Direct Memory must fail before staging or retiring any caller owner. */
    if(BYTE(0xfe78)) {job->error=21;return;}
    if(job->action==ZT_DISCARD) {
        transfer_load.error=0;transfer_load.detail=0;
        chain_discard(&transfer_load);
    } else if(job->action==ZT_PREPARE
#ifndef ZP_CHAIN_ONLY
              ||job->action==ZT_PREPARE_EXECUTE
#endif
    ) {
        transfer_load.ready=0; /* a rejected new request cannot commit old code */
        if(job->path<0xc800u||job->path>0xfb80u||
           job->caller.stack_pointer<0xcc00u||
           job->caller.heap_frontier>job->caller.stack_pointer-256u) {
            job->error=0x20;job->detail=ERR_ENTRY_CORRUPTED;return;
        }
        for(i=0;i<sizeof(path);++i) {path[i]=PTR(job->path)[i];if(!path[i])break;}
        if(i==sizeof(path)) {job->error=0x20;job->detail=ERR_ENTRY_CORRUPTED;return;}
        caller=job->caller;
        transfer_load.error=0;transfer_load.detail=0;
#ifndef ZP_CHAIN_ONLY
        if(job->action==ZT_PREPARE_EXECUTE) {
            execute_prepare(&transfer_load,path);
        }
        else
#endif
        chain_prepare(&transfer_load,path,&caller);
    } else if(job->action==ZT_COMMIT&&transfer_load.ready) {
#ifndef ZP_CHAIN_ONLY
        if(transfer_load.kind==IMAGE_EXECUTABLE) {
            memset(&executing,0,sizeof(executing));executing.operation=ZF_END_EXECUTE;
            owner_count=0;
            if(BYTE(ZT_FILE)) {
                transfer_file_call(1,&executing);
                if(!executing.error) {
                    owners[owner_count++]=BYTE(ZT_FILE);
                }
            }
            if(!executing.error) {
                transfer_original_pages(originals);
                execute_override(&transfer_load,path,originals,owners,owner_count);
            } else {transfer_load.error=executing.error;transfer_load.detail=(uint8_t)executing.detail;}
            chain_discard(&transfer_load);
            job->error=transfer_load.error;job->detail=transfer_load.detail;return;
        }
        if(transfer_load.kind!=IMAGE_CHAIN) {
            job->error=0x20;job->detail=ERR_ENTRY_CORRUPTED;return;
        }
#endif
        transfer_commit();return;

    } else {job->error=0x20;job->detail=ERR_ENTRY_CORRUPTED;return;}
    job->error=transfer_load.error;job->detail=transfer_load.detail;
}
/* Runs on the new retained stack. No old caller code, job, frame, or mutable
 * standard table is touched after install. Failed releases remain owned here. */
void transfer_install(void) {
    static const uint16_t targets[]={0x4016,0x401a,0x401c,0x412c,0x412e,0x415c,0x4164,0x4165,0x40be};
    uint16_t values[9],i,at=0xfc30;
    chain_install(&transfer_load);
    chain_discard(&transfer_load);
    values[0]=transfer_load.bytes;values[1]=transfer_load.globals;
    values[2]=transfer_load.origin+transfer_load.bytes;values[3]=transfer_load.frontier;
    values[4]=transfer_load.end+1u-1024u;values[5]=transfer_load.floor;
    values[6]=values[7]=values[8]=0;
    for(i=0;i<9;++i) {
        BYTE(at++)=(uint8_t)targets[i];BYTE(at++)=(uint8_t)(targets[i]>>8);
        BYTE(at++)=(uint8_t)values[i];BYTE(at++)=(uint8_t)(values[i]>>8);
    }
    BYTE(0xfe74)=BYTE(ZT_FILE);
    BYTE(0xfe7c)=64;BYTE(0xfe7d)=0; /* ordinary Pascal helper headroom */
}
