/* Actual loader against a deterministic SDK; caller memory is independent of
 * the staged pages and must remain byte-identical until authenticated commit. */
#include "chain.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t pages[64][16384],owned[64],user[65536],before[65536];
static FILE *handle;
static char directory[1024];
static unsigned checks,scenarios,opens,reads,closes,allocs,drops;
static unsigned fail_open,fail_read,fail_close,fail_alloc,zero_read,oversize_read;
static int fail_drop=-1;
static uint16_t short_limit;
static ChainLoad load;
static ChainCaller caller;
#define check(x) do {++checks;if(!(x)){fprintf(stderr,"chain check at line %u (scenario %u, error %u/%u)\n",__LINE__,scenarios,load.error,load.detail);abort();}}while(0)
static unsigned count(void) {unsigned i,n=0;for(i=0;i<64;++i)n+=owned[i]!=0;return n;}
zos_err_t palloc(uint8_t *page) {
    uint8_t i;if(++allocs==fail_alloc)return ERR_FAILURE;
    for(i=32;i<64;++i)if(!owned[i]) {owned[i]=1;*page=i;memset(pages[i],0xa5,16384);return 0;}
    return ERR_NO_MORE_ENTRIES;
}
zos_err_t pfree(uint8_t page) {
    check(page>=32&&page<64&&owned[page]);++drops;
    if(page==fail_drop)return ERR_FAILURE;
    owned[page]=0;return 0;
}
zos_dev_t open(const char *path,uint8_t flags) {
    char name[1200];check(!handle&&flags==O_RDONLY);
    if(++opens==fail_open)return -ERR_FAILURE;
    check(!strncmp(path,"H:/",3));snprintf(name,sizeof(name),"%s/%s",directory,path+3);
    handle=fopen(name,"rb");return handle?2:-ERR_NO_SUCH_ENTRY;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *bytes) {
    check(fd==2&&handle);++reads;
    if(reads==fail_read)return ERR_FAILURE;
    if(reads==zero_read){*bytes=0;return 0;}
    if(reads==oversize_read){++*bytes;return 0;}
    if(short_limit&&*bytes>short_limit)*bytes=short_limit;
    *bytes=(uint16_t)fread(out,1,*bytes,handle);return ferror(handle)?ERR_FAILURE:0;
}
zos_err_t close(zos_dev_t fd) {
    check(fd==2&&handle);check(!fclose(handle));handle=0;
    return ++closes==fail_close?ERR_FAILURE:0;
}
void chain_page_io(uint8_t page,uint16_t at,uint16_t bytes) {
    check(page<64&&owned[page]&&at<=16384&&bytes<=128&&bytes<=16384-at);
    if(chain_store)memcpy(pages[page]+at,chain_buffer,bytes);
    else memcpy(chain_buffer,pages[page]+at,bytes);
}
uint16_t chain_user_word(uint16_t at) {check(at>=caller.heap_floor&&at<caller.heap_frontier-1);return user[at]|((uint16_t)user[at+1]<<8);}
void chain_user_store(uint16_t at,uint16_t value) {check(load.ready);user[at]=(uint8_t)value;user[at+1]=(uint8_t)(value>>8);}
void chain_user_copy(uint16_t at,uint16_t bytes) {check(load.ready&&at>=load.origin&&(uint32_t)at+bytes<=(uint32_t)load.origin+load.bytes);memcpy(user+at,chain_buffer,bytes);}
static void put(uint16_t at,uint16_t value) {user[at]=(uint8_t)value;user[at+1]=(uint8_t)(value>>8);}
static void setup(uint8_t heap) {
    check(!handle&&!count());++scenarios;chain_init(&load);
    opens=reads=closes=allocs=drops=0;
    fail_open=fail_read=fail_close=fail_alloc=zero_read=oversize_read=0;fail_drop=-1;short_limit=0;
    memset(user,0xa5,sizeof(user));
    caller.origin=0x9000;caller.bytes=0x20;caller.globals=23;caller.end=0xfbff;
    caller.heap_floor=caller.heap_frontier=0x9020;caller.stack_pointer=0xf800;
    if(heap) {
        caller.heap_frontier=0xe020;
        put(0x9020,0x8000u|(0xe000-0x9020-2));put(0xe000,30);
    }
    memcpy(before,user,sizeof(user));
}
static void unchanged(void) {check(!memcmp(before,user,sizeof(user)));}
static void rejected(const char *path) {
    check(!chain_prepare(&load,path,&caller));check(load.error&&!load.ready&&!handle&&!count());unchanged();
}
static void accepted(const char *path) {
    check(chain_prepare(&load,path,&caller));check(load.ready&&!load.error&&!handle);unchanged();
}
static void installed(const char *path) {
    char name[1200];uint8_t expected[30000];size_t bytes;FILE *f;
    snprintf(name,sizeof(name),"%s/%s",directory,path+3);f=fopen(name,"rb");check(f!=0);bytes=fread(expected,1,sizeof(expected),f);check(!fclose(f));
    check(bytes==96u+load.bytes);chain_install(&load);check(!load.ready);
    check(!memcmp(user+load.origin,expected+96,load.bytes));
    memcpy(before+load.origin,expected+96,load.bytes);
    if(load.trim_at) {before[load.trim_at]=(uint8_t)load.trim_value;before[load.trim_at+1]=(uint8_t)(load.trim_value>>8);}
    unchanged();check(chain_discard(&load)&&!count());
}
int main(int argc,char **argv) {
    unsigned i,total_reads,total_allocs;uint16_t floor;
    check(argc==2);snprintf(directory,sizeof(directory),"%s",argv[1]);
    setup(0);accepted("H:/small.CHN");check(load.floor==0x9020&&load.frontier==0x9020&&!load.trim_at&&count()==1);installed("H:/small.CHN");
    setup(0);accepted("H:/grow.CHN");check(load.floor==0xd800&&load.frontier==0xd800&&count()==2);installed("H:/grow.CHN");
    setup(1);accepted("H:/grow.CHN");check(load.floor==0xd800&&load.frontier==0xe020&&load.trim_at==0xd800&&load.trim_value==(0x8000u|0x7fe));installed("H:/grow.CHN");
    setup(1);accepted("H:/odd.CHN");check(load.floor==0xd802&&load.trim_at==0xd802&&load.trim_value==(0x8000u|0x7fc));installed("H:/odd.CHN");
    for(floor=0xd7fe;floor<=0xd802;floor+=2) {
        setup(0);caller.heap_frontier=floor+2;put(0x9020,0x8000u|(floor-0x9020-2));put(floor,0);memcpy(before,user,sizeof(user));
        if(floor<0xd800)rejected("H:/grow.CHN");
        else {accepted("H:/grow.CHN");check(load.trim_at==(floor==0xd802?0xd800:0));installed("H:/grow.CHN");}
    }
    setup(1);accepted("H:/short.CHN");check(load.floor==0x9020&&!load.trim_at&&load.frontier==0xe020);installed("H:/short.CHN");
    /* Every free-block boundary and both sides of every live header matter. */
    for(floor=0x9020;floor<0xd900;floor+=128) {
        setup(0);caller.heap_frontier=floor+4;put(0x9020,0x8000u|(floor-0x9020));put(floor+2,0);
        memcpy(before,user,sizeof(user));
        if(floor+2<0xd800)rejected("H:/grow.CHN");
        else {accepted("H:/grow.CHN");installed("H:/grow.CHN");}
    }
    setup(0);caller.heap_floor=caller.heap_frontier=0xe800;accepted("H:/short.CHN");check(load.floor==0xe800&&load.frontier==0xe800);installed("H:/short.CHN");
    setup(1);put(0xe000,31);memcpy(before,user,sizeof(user));rejected("H:/grow.CHN");
    setup(1);put(0xe000,0x7ffe);memcpy(before,user,sizeof(user));rejected("H:/grow.CHN");
    setup(1);put(0x9020,0x7fff);memcpy(before,user,sizeof(user));rejected("H:/grow.CHN");
    setup(1);++caller.heap_frontier;rejected("H:/grow.CHN");
    setup(1);--caller.heap_floor;rejected("H:/grow.CHN");
    setup(1);caller.stack_pointer=0xe040;rejected("H:/grow.CHN");
    setup(0);caller.bytes=0xffff;rejected("H:/small.CHN");
    setup(0);caller.globals=0x1001;rejected("H:/small.CHN");
    setup(0);rejected("H:/missing.CHN");check(load.error==1);
    {const char *bad[]={"abi","origin","end","globals","truncated","trailing","hash","reserved","overlay","empty"};
     char path[48];for(i=0;i<sizeof(bad)/sizeof(*bad);++i){setup(1);snprintf(path,sizeof(path),"H:/%s.CHN",bad[i]);rejected(path);}}
    setup(1);accepted("H:/grow.CHN");total_reads=reads;total_allocs=allocs;check(chain_discard(&load));
    for(i=1;i<=total_reads;++i) {
        setup(1);fail_read=i;rejected("H:/grow.CHN");check(load.error==0x99&&load.detail==ERR_FAILURE);
        setup(1);oversize_read=i;rejected("H:/grow.CHN");
        if(i<total_reads){setup(1);zero_read=i;rejected("H:/grow.CHN");}
    }
    for(i=1;i<=total_allocs;++i){setup(1);fail_alloc=i;rejected("H:/grow.CHN");check(load.error==0x1ff);}
    setup(1);fail_open=1;rejected("H:/grow.CHN");
    setup(1);fail_close=1;rejected("H:/grow.CHN");
    setup(1);short_limit=1;accepted("H:/grow.CHN");installed("H:/grow.CHN");
    setup(1);accepted("H:/grow.CHN");fail_drop=32;check(!chain_discard(&load)&&count()==1&&load.pages[0]==32&&load.pages[1]==255);unchanged();fail_drop=-1;accepted("H:/small.CHN");installed("H:/small.CHN");
    setup(1);fail_drop=32;check(!chain_prepare(&load,"H:/hash.CHN",&caller));check(load.error==0x20&&count()==1&&!handle);unchanged();fail_drop=-1;check(chain_discard(&load)&&!count());
    printf("PASS: %u chain staging/heap/identity/commit assertions across %u scenarios\n",checks,scenarios);return 0;
}
