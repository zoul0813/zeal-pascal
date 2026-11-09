/* Exact production staging code with deterministic SDK failure injection.
 * This is a component proof, not Pascal overlay compiler acceptance. */
#include "runtime/process/overlay.h"
static uint8_t modified;
#include "runtime/process/chain.h"
#include "overlay-fixtures.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t chain_buffer[128],chain_store;
static uint8_t pages[64][16384],owned[64];
static uint8_t caller[65536],before[65536];
static FILE *handle;
static const char *directory;
static unsigned checks,scenario,reads,closes,allocs,stats;
static unsigned fail_read,zero_read,oversize_read,fail_alloc,fail_stat,fail_close;
static int fail_drop=-1;
static uint8_t fail_open;
static uint16_t short_limit;
static int size_delta;
static OverlayLoad load;
static OverlayGroup group;
#define check(x) do {++checks;if(!(x)){fprintf(stderr,"overlay check line %u scenario %u: %s (error %u/%u)\n",__LINE__,scenario,#x,load.error,load.detail);abort();}}while(0)
static unsigned count(void) {unsigned i,n=0;for(i=32;i<64;++i)n+=owned[i]!=0;return n;}
zos_err_t palloc(uint8_t *page) {
    uint8_t i;
    if(++allocs==fail_alloc) {*page=31;return ERR_NO_MORE_MEMORY;}
    for(i=32;i<64;++i)if(!owned[i]) {owned[i]=1;*page=i;memset(pages[i],0xa5,16384);return 0;}
    return ERR_NO_MORE_MEMORY;
}
zos_err_t pfree(uint8_t page) {
    check(page>=32&&page<64&&owned[page]);
    if(page==fail_drop)return ERR_FAILURE;
    owned[page]=0;return 0;
}
zos_dev_t open(const char *path,uint8_t flags) {
    char name[1200];check(!handle&&flags==O_RDONLY&&!strncmp(path,"H:/",3));
    if(fail_open)return -ERR_NO_SUCH_ENTRY;
    snprintf(name,sizeof(name),"%s/%s",directory,path+3);
    handle=fopen(name,"rb");return handle?2:-ERR_NO_SUCH_ENTRY;
}
zos_err_t dstat(zos_dev_t fd,zos_stat_t *info) {
    long size;check(fd==2&&handle);if(++stats==fail_stat)return ERR_FAILURE;
    check(!fseek(handle,0,SEEK_END));size=ftell(handle);check(size>=0&&!fseek(handle,0,SEEK_SET));
    memset(info,0,sizeof(*info));info->s_size=(uint32_t)(size+size_delta);return 0;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *bytes) {
    check(fd==2&&handle);++reads;
    if(reads==fail_read)return ERR_FAILURE;
    if(reads==zero_read) {*bytes=0;return 0;}
    if(reads==oversize_read) {++*bytes;return 0;}
    if(short_limit&&*bytes>short_limit)*bytes=short_limit;
    *bytes=(uint16_t)fread(out,1,*bytes,handle);return ferror(handle)?ERR_FAILURE:0;
}
zos_err_t close(zos_dev_t fd) {
    check(fd==2&&handle&&!fclose(handle));handle=0;
    return ++closes==fail_close?ERR_FAILURE:0;
}
void chain_page_io(uint8_t page,uint16_t at,uint16_t bytes) {
    check(page>=32&&page<64&&owned[page]&&bytes&&bytes<=128&&at<16384&&bytes<=16384-at);
    if(chain_store)memcpy(pages[page]+at,chain_buffer,bytes);
    else memcpy(chain_buffer,pages[page]+at,bytes);
}
static void unchanged(void) {check(!memcmp(caller,before,sizeof(caller)));}
static void setup(const OverlayCase *c) {
    modified=!strcmp(c->name,"H:/trusted-member.000");
    check(!handle&&!count());++scenario;overlay_init(&load);
    check(!overlay_group_decode(c->record,&group));
    reads=closes=allocs=stats=0;fail_read=zero_read=oversize_read=fail_alloc=fail_stat=fail_close=0;
    fail_drop=-1;fail_open=0;short_limit=0;size_delta=0;
    memset(caller,0xa5,sizeof(caller));memcpy(before,caller,sizeof(caller));
}
static uint8_t prepare(const OverlayCase *c,uint16_t member) {
    return overlay_prepare(&load,c->name,&group,member);
}
static void rejected(const OverlayCase *c) {
    check(!prepare(c,0)&&load.error&&!load.ready&&!handle&&!count());unchanged();
}
static void staged(uint16_t member) {
    unsigned i,bytes=member?259:0x4105;uint16_t counter=0xe100+member;
    uint8_t prefix[]={0x3a,(uint8_t)counter,(uint8_t)(counter>>8),0x3c,0x32,(uint8_t)counter,(uint8_t)(counter>>8),0xc9};
    check(load.ready&&!load.error&&!handle&&count()==(member?1u:2u));
    check(load.origin==0x9000&&load.bytes==bytes&&load.entry==0x9000&&load.member==member);
    for(i=0;i<32;++i)check(load.metadata[i]==255);
    for(i=0;i<bytes;++i)check(pages[load.pages[i/16384]][i%16384]==(i<8?prefix[i]:(uint8_t)((i*7+member*43)^(modified&&member==0&&i==100))));
    unchanged();
}
static void decode_cases(void) {
    uint8_t r[48];unsigned at;OverlayGroup g;
    memcpy(r,overlay_cases[0].record,48);check(!overlay_group_decode(r,&g));
    for(at=44;at<48;++at) {r[at]=1;check(overlay_group_decode(r,&g));r[at]=0;}
    r[0]=100;check(overlay_group_decode(r,&g));r[0]=0;
    r[2]=0;check(overlay_group_decode(r,&g));r[2]=255;
    r[6]=255;r[7]=127;check(overlay_group_decode(r,&g));
    memcpy(r,overlay_cases[0].record,48);r[8]=0;r[9]=0;check(overlay_group_decode(r,&g));
    memcpy(r,overlay_cases[0].record,48);r[8]=255;r[9]=255;check(overlay_group_decode(r,&g));
    memcpy(r,overlay_cases[0].record,48);r[10]=0;r[11]=0;check(overlay_group_decode(r,&g));
}
int main(int argc,char **argv) {
    unsigned c,i,total_reads,total_allocs;uint8_t pending;const OverlayCase *good=overlay_cases;
    check(argc==2);directory=argv[1];overlay_init(&load);decode_cases();
    for(c=0;c<OVERLAY_CASE_COUNT;++c) {
        setup(overlay_cases+c);
        if(overlay_cases[c].valid)for(i=0;i<2;++i) {
            check(prepare(overlay_cases+c,(uint16_t)i));staged((uint16_t)i);
            check(overlay_discard(&load)&&!count());
        } else rejected(overlay_cases+c);
    }
    setup(good);check(prepare(good,0));total_reads=reads;total_allocs=allocs;
    staged(0);check(overlay_discard(&load)&&!count());
    for(i=1;i<=total_reads;++i) {setup(good);fail_read=i;rejected(good);}
    /* Zero reads in the body are truncation. The final EOF read is allowed. */
    for(i=1;i<total_reads;++i) {setup(good);zero_read=i;rejected(good);}
    for(i=1;i<=total_reads;++i) {setup(good);oversize_read=i;rejected(good);}
    for(i=1;i<=total_allocs;++i) {setup(good);fail_alloc=i;rejected(good);}
    for(i=1;i<=7;++i) {setup(good);short_limit=(uint16_t)i;check(prepare(good,0));staged(0);check(overlay_discard(&load)&&!count());}
    setup(good);fail_open=1;rejected(good);check(load.error==1);
    setup(good);fail_stat=1;rejected(good);
    setup(good);fail_close=1;rejected(good);
    setup(good);size_delta=-1;rejected(good);
    setup(good);size_delta=1;rejected(good);
    setup(good);check(!prepare(good,2)&&load.error&&!handle&&!count());
    setup(good);group.group=100;rejected(good);
    /* Metadata-release failure leaves its exact page in the ledger. A second
     * prepare must not forget it, reopen a file or publish ready code. */
    setup(good);fail_drop=32;
    check(!prepare(good,0)&&!load.ready&&!handle&&count()==1&&load.metadata[0]==32);
    check(!prepare(good,0)&&count()==1&&load.metadata[0]==32);
    fail_drop=-1;check(overlay_discard(&load)&&!count());
    check(prepare(good,0));staged(0);pending=load.pages[1];fail_drop=pending;
    check(!overlay_discard(&load)&&!load.ready&&count()==1&&load.pages[1]==pending);
    check(!prepare(good,0)&&count()==1&&load.pages[1]==pending);
    fail_drop=-1;check(overlay_discard(&load)&&!count());
    check(prepare(good,1));staged(1);check(overlay_discard(&load)&&!count());
    /* Record910 straddles the page boundary; record911 must reread and check
     * that split record while validating its own disjoint ranges and ID. */
    setup(&overlay_large_case);check(prepare(&overlay_large_case,0));staged(0);
    check(allocs==4&&overlay_discard(&load)&&!count());
    for(i=1;i<=4;++i) {
        setup(&overlay_large_case);fail_alloc=i;rejected(&overlay_large_case);
    }
    printf("PASS: %u overlay staging/ownership checks across %u scenarios (%u read and %u allocation fault positions)\n",checks,scenario,total_reads,total_allocs);
    return 0;
}
