/* Coordinator faults with modeled SDK/file/staging services. The real staged
 * loader and native nonreturning transport are independently exercised. */
#include "transfer.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint8_t transfer_memory[65536];
extern ImageLoad transfer_load;
static uint8_t owned[64],bad_page=255,file_error,file_detail;
static unsigned checks,prepares,installs,commits,file_calls,drops[64];
#define check(x) do {++checks;if(!(x)){fprintf(stderr,"transfer model line %u\n",__LINE__);abort();}}while(0)
zos_err_t pfree(uint8_t page) {
    check(page>=32&&page<64&&owned[page]);++drops[page];
    if(page==bad_page)return ERR_FAILURE;
    owned[page]=0;return 0;
}
void chain_init(ImageLoad *load) {
    memset(load,0,sizeof(*load));load->pages[0]=load->pages[1]=255;load->fd=-1;
#ifndef ZP_CHAIN_ONLY
    load->pages[2]=255;load->kind=IMAGE_CHAIN;
#endif
}
uint8_t chain_discard(ImageLoad *load) {
    unsigned i;uint8_t ok=1;
    load->ready=0;
    for(i=0;i<IMAGE_LOAD_PAGES;++i)if(load->pages[i]!=255) {
        if(pfree(load->pages[i])) {
            if(!load->error){load->error=0x1ff;load->detail=ERR_FAILURE;}ok=0;
        } else load->pages[i]=255;
    }
    return ok;
}
uint8_t chain_prepare(ImageLoad *load,const char *path,const ChainCaller *caller) {
    ++prepares;check(!strcmp(path,"H:/CHAIN.CHN"));check(caller->profile[1]==0x91);
    if(!chain_discard(load))return 0;
    chain_init(load);load->origin=caller->origin;load->globals=64;
    load->bytes=0x4800;load->end=caller->end;
    load->floor=0xd800;load->frontier=0xe020;
    owned[32]=owned[33]=1;load->pages[0]=32;load->pages[1]=33;load->ready=1;return 1;
}
#ifndef ZP_CHAIN_ONLY
static unsigned overrides;
static uint8_t handoff_supported=1;
uint8_t execute_prepare(ImageLoad *load,const char *path) {
    uint8_t ok=chain_prepare(load,path,&(ChainCaller){.origin=0x9000,.end=0xfbff,.profile={[1]=0x91}});
    if(ok){load->kind=IMAGE_EXECUTABLE;load->handoff=handoff_supported;}
    return ok;
}
void transfer_original_pages(uint8_t originals[3]) {originals[0]=40;originals[1]=41;originals[2]=42;}
uint8_t execute_override(ImageLoad *load,const char *path,const uint8_t originals[3],
                         const uint8_t *owners,uint8_t owner_count) {
    ++overrides;check(load->ready&&load->kind==IMAGE_EXECUTABLE&&!strcmp(path,"H:/CHAIN.CHN"));
    check(originals[0]==40&&originals[1]==41&&originals[2]==42);
    check(owner_count==(transfer_memory[ZT_FILE]?1:0));
    if(owner_count)check(owners[0]==35&&owned[35]);
    load->error=1;load->detail=ERR_NO_SUCH_ENTRY;return 0;
}
#endif
void chain_install(ImageLoad *load) {check(load->ready);++installs;load->ready=0;}
void transfer_commit(void) {++commits;transfer_install();}
void transfer_file_call(uint8_t mode,void *pointer) {
    FileCleanup *job=pointer;
    ++file_calls;check(mode==1&&transfer_memory[ZT_FILE]==35);
    check(job->operation==ZF_END_PROGRAM||job->operation==ZF_END_EXECUTE);
    job->error=file_error;job->detail=file_detail;
}
static uint16_t word(uint16_t at) {return transfer_memory[at]|((uint16_t)transfer_memory[at+1]<<8);}
static void setup(TransferJob *job,uint8_t files) {
    memset(owned,0,sizeof(owned));memset(drops,0,sizeof(drops));memset(transfer_memory,0,sizeof(transfer_memory));
    chain_init(&transfer_load);memset(job,0,sizeof(*job));
    bad_page=255;file_error=file_detail=0;prepares=installs=commits=file_calls=0;
#ifndef ZP_CHAIN_ONLY
    handoff_supported=1;
#endif
    transfer_memory[ZT_PAGE]=34;owned[34]=1;
    if(files){transfer_memory[ZT_FILE]=35;owned[35]=1;}
    job->caller.origin=0x9000;job->caller.bytes=32;job->caller.globals=23;
    job->caller.end=0xfbff;job->caller.heap_floor=0xd800;
    job->caller.heap_frontier=0xe020;job->caller.stack_pointer=0xf600;
    job->caller.profile[1]=0x91;job->path=0xf680;
    strcpy((char*)transfer_memory+job->path,"H:/CHAIN.CHN");
}
static void prepare(TransferJob *job) {
    job->action=ZT_PREPARE;transfer_dispatch(job);
    check(!job->error&&transfer_load.ready&&owned[32]&&owned[33]);
}
static void clean(FileCleanup *job) {
    memset(job,0,sizeof(*job));job->operation=ZF_END_PROGRAM;
}
int main(void) {
    TransferJob job;FileCleanup ending;unsigned i;
    /* Initialize the real coordinator before injecting private retained state. */
    memset(&job,0,sizeof(job));job.action=ZT_DISCARD;transfer_dispatch(&job);
    setup(&job,1);transfer_memory[0xfe78]=1;transfer_dispatch(&job);
    check(job.error==21&&!prepares&&!file_calls&&owned[35]);
    setup(&job,0);job.action=ZT_COMMIT;transfer_dispatch(&job);check(job.error==0x20&&!commits);
    setup(&job,0);job.path=0xfb81;transfer_dispatch(&job);check(job.error==0x20&&!prepares);
    setup(&job,0);memset(transfer_memory+job.path,'X',128);transfer_dispatch(&job);check(job.error==0x20&&!prepares);
    setup(&job,0);job.caller.stack_pointer=0xcbfe;transfer_dispatch(&job);check(job.error==0x20&&!prepares);
    setup(&job,0);job.caller.heap_frontier=0xf400;transfer_dispatch(&job);check(job.error==0x20&&!prepares);
    setup(&job,0);prepare(&job);job.path=0;transfer_dispatch(&job);
    check(job.error==0x20&&!transfer_load.ready&&owned[32]&&owned[33]);
    job.action=ZT_COMMIT;transfer_dispatch(&job);check(job.error==0x20&&!commits);
    clean(&ending);check(!transfer_cleanup(&ending));
    setup(&job,1);prepare(&job);bad_page=32;file_error=0x20;file_detail=9;
    clean(&ending);check(transfer_cleanup(&ending)==1);
    check(ending.error==0x1ff&&ending.detail==ERR_FAILURE&&owned[32]&&!owned[33]&&owned[35]);
    check(!drops[35]&&!drops[34]);bad_page=255;file_error=0;
    clean(&ending);check(!transfer_cleanup(&ending)&&!ending.error&&!owned[32]&&!owned[35]);
    check(drops[32]==2&&drops[33]==1&&drops[35]==1&&!transfer_memory[ZT_FILE]&&owned[34]);
    setup(&job,1);prepare(&job);bad_page=35;
    clean(&ending);check(transfer_cleanup(&ending)&&ending.error==0x1ff&&transfer_memory[ZT_FILE]==35);
    bad_page=255;clean(&ending);check(!transfer_cleanup(&ending)&&!ending.error&&drops[35]==2);
    setup(&job,0);prepare(&job);bad_page=33;
    clean(&ending);check(transfer_cleanup(&ending)&&ending.error==0x1ff&&!file_calls);
    bad_page=255;clean(&ending);check(!transfer_cleanup(&ending)&&!ending.error&&!drops[34]);
    setup(&job,1);prepare(&job);file_error=2;file_detail=7;bad_page=32;
    job.action=ZT_COMMIT;transfer_dispatch(&job);
    check(!job.error&&commits==1&&installs==1&&!transfer_load.ready&&owned[32]&&!owned[33]&&!file_calls);
    file_error=0;bad_page=255;clean(&ending);check(!transfer_cleanup(&ending));
    setup(&job,1);prepare(&job);bad_page=32;job.action=ZT_COMMIT;transfer_dispatch(&job);
    check(commits==1&&installs==1&&owned[32]&&!owned[33]&&owned[34]&&owned[35]);
    check(transfer_memory[0xfe74]==35&&word(0xfe7c)==64);
    {static const uint16_t expected[]={0x4016,0x4800,0x401a,64,0x401c,0xd800,0x412c,0xe020,
        0x412e,0xf800,0x415c,0xd800,0x4164,0,0x4165,0,0x40be,0};
     for(i=0;i<sizeof(expected)/sizeof(*expected);++i)check(word(0xfc30+2*i)==expected[i]);}
    bad_page=255;clean(&ending);check(!transfer_cleanup(&ending)&&!ending.error&&!owned[32]&&!owned[35]&&owned[34]);
#ifndef ZP_CHAIN_ONLY
    setup(&job,1);handoff_supported=0;job.action=ZT_PREPARE_EXECUTE;
    transfer_dispatch(&job);
    check(!job.error&&!file_calls&&transfer_load.ready&&!transfer_load.handoff);
    check(owned[32]&&owned[33]&&owned[34]&&owned[35]);
    overrides=0;job.action=ZT_COMMIT;transfer_dispatch(&job);
    check(job.error==1&&job.detail==ERR_NO_SUCH_ENTRY&&overrides==1&&file_calls==1);
    check(!owned[32]&&!owned[33]&&owned[34]&&owned[35]);
    clean(&ending);check(!transfer_cleanup(&ending));
    for(i=0;i<2;++i) {
        setup(&job,(uint8_t)i);overrides=0;job.action=ZT_PREPARE_EXECUTE;
        transfer_dispatch(&job);check(!job.error&&transfer_load.kind==IMAGE_EXECUTABLE);
        job.action=ZT_COMMIT;transfer_dispatch(&job);
        check(job.error==1&&job.detail==ERR_NO_SUCH_ENTRY&&overrides==1&&!commits&&!installs);
        check(!owned[32]&&!owned[33]&&owned[34]&&owned[35]==(i!=0));
        clean(&ending);check(!transfer_cleanup(&ending));
    }
    setup(&job,1);overrides=0;job.action=ZT_PREPARE_EXECUTE;
    transfer_dispatch(&job);file_error=0xf0;file_detail=ERR_FAILURE;
    job.action=ZT_COMMIT;transfer_dispatch(&job);
    check(job.error==0xf0&&job.detail==ERR_FAILURE&&!overrides&&!owned[32]&&!owned[33]&&owned[35]);
    file_error=0;clean(&ending);check(!transfer_cleanup(&ending));
    setup(&job,0);prepare(&job);transfer_load.pages[2]=36;owned[36]=1;bad_page=36;
    clean(&ending);check(transfer_cleanup(&ending)&&ending.error==0x1ff&&owned[36]&&!owned[32]&&!owned[33]);
    bad_page=255;clean(&ending);check(!transfer_cleanup(&ending)&&!ending.error&&drops[36]==2&&owned[34]);
#endif
    printf("PASS: %u transfer coordinator ownership/control/fault assertions\n",checks);return 0;
}
