/* Actual loader against a deterministic SDK; caller memory is independent of
 * the staged pages and must remain byte-identical until authenticated commit. */
#include "chain.h"
#include "execute_handoff.h"
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
static ImageLoad load;
static ChainCaller caller;
static uint8_t code_page=63,stack_page=62;
static uint8_t rollback_expected[3][16384],staged_expected[3][16384];
static uint8_t override_error;
static unsigned override_calls,switch_calls;
uint8_t execute_code_page(void) {return code_page;}
uint8_t execute_stack_page(void) {return stack_page;}
#define check(x) do {++checks;if(!(x)){fprintf(stderr,"chain check at line %u (scenario %u, error %u/%u)\n",__LINE__,scenarios,load.error,load.detail);abort();}}while(0)
void execute_switch(void) {
    uint8_t saved=stack_page;++switch_calls;stack_page=code_page;
    execute_attempt();check(stack_page==code_page);stack_page=saved;
}
uint8_t execute_os_override(const char *path) {
    unsigned i;uint8_t sum=0,*record=pages[load.originals[2]]+0x3c00;
    check(!strcmp(path,"H:/checked.COM")&&load.ready&&load.kind==IMAGE_ROLLBACK);
    check(stack_page==code_page);++override_calls;
    check(!memcmp(record,"ZEX1",4)&&record[4]==4&&record[42]==code_page);
    check(!memcmp(record+5,load.originals,3)&&!memcmp(record+10,load.id,32)&&!memcmp(record+43,load.pages,3));
    for(i=0;i<EXECUTE_HANDOFF_BYTES;++i)sum^=record[i];check(!sum);
    for(i=0;i<3;++i)memset(pages[load.originals[i]],0x6a,16384);
    return override_error;
}
void execute_abandon(void) {check(0);}
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
    uint8_t i;
    check(!handle&&!count());++scenarios;chain_init(&load);
    opens=reads=closes=allocs=drops=0;
    fail_open=fail_read=fail_close=fail_alloc=zero_read=oversize_read=0;fail_drop=-1;short_limit=0;
    memset(user,0xa5,sizeof(user));
    caller.origin=0x9000;caller.bytes=0x20;caller.globals=23;caller.end=0xfbff;
    caller.heap_floor=caller.heap_frontier=0x9020;caller.stack_pointer=0xf800;
    for(i=0;i<CHAIN_PROFILE_BYTES;++i)caller.profile[i]=(uint8_t)((CHAIN_ENTRY_BYTES+i)*7+3);
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
static void executable_accepted(const char *path) {
    char name[1200];uint8_t expected[46081];size_t bytes;unsigned at;FILE *f;
    check(execute_prepare(&load,path));
    check(load.ready&&!load.error&&load.kind==IMAGE_EXECUTABLE&&!handle);unchanged();
    snprintf(name,sizeof(name),"%s/%s",directory,path+3);f=fopen(name,"rb");check(f!=0);
    bytes=fread(expected,1,sizeof(expected),f);check(feof(f)&&!ferror(f)&&!fclose(f));
    check(bytes==load.length&&count()==(bytes+16383)/16384);
    for(at=0;at<bytes;++at)check(pages[load.pages[at/16384]][at%16384]==expected[at]);
    chain_install(&load);check(load.ready);unchanged(); /* wrong installer cannot commit a COM */
}
static void executable_rejected(const char *path) {
    check(!execute_prepare(&load,path)&&load.error&&!load.ready&&!handle&&!count());unchanged();
}
static void executable_cases(void) {
    const char *valid[]={"tiny","small","boundary","three","max"};
    const char *bad[]={"abi","entry","length","origin","globals","end","stack","reserved","overlay","hash","truncated","trailing"};
    char path[64];unsigned i,total_reads,total_allocs,j;
    for(i=0;i<sizeof(valid)/sizeof(*valid);++i) {
        setup(0);snprintf(path,sizeof(path),"H:/x-%s.COM",valid[i]);executable_accepted(path);
        check(chain_discard(&load)&&!count());
    }
    for(i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        setup(0);snprintf(path,sizeof(path),"H:/x-%s.COM",bad[i]);executable_rejected(path);
    }
    setup(0);executable_rejected("H:/small.CHN");
    setup(0);executable_rejected("H:/missing.COM");check(load.error==1);
    setup(0);executable_accepted("H:/x-max.COM");total_reads=reads;total_allocs=allocs;
    check(total_allocs==3&&chain_discard(&load));
    for(i=1;i<=total_reads;++i) {
        setup(0);fail_read=i;executable_rejected("H:/x-max.COM");check(load.error==0x99);
        setup(0);oversize_read=i;executable_rejected("H:/x-max.COM");
        if(i<total_reads){setup(0);zero_read=i;executable_rejected("H:/x-max.COM");}
    }
    for(i=1;i<=total_allocs;++i) {
        setup(0);fail_alloc=i;executable_rejected("H:/x-max.COM");check(load.error==0x1ff);
    }
    setup(0);fail_open=1;executable_rejected("H:/x-max.COM");
    setup(0);fail_close=1;executable_rejected("H:/x-max.COM");
    setup(0);short_limit=1;executable_accepted("H:/x-max.COM");check(chain_discard(&load));
    for(j=32;j<35;++j) {
        setup(0);fail_drop=(int)j;check(!execute_prepare(&load,"H:/x-hash.COM"));
        check(load.error==0x20&&count()==1&&!load.ready&&!handle&&load.pages[j-32]==j);unchanged();
        check(!chain_prepare(&load,"H:/small.CHN",&caller)&&!load.ready&&count()==1);
        fail_drop=-1;check(chain_discard(&load)&&!count());
    }
    setup(0);accepted("H:/small.CHN");executable_accepted("H:/x-max.COM");
    check(chain_prepare(&load,"H:/small.CHN",&caller)&&load.kind==IMAGE_CHAIN&&count()==1);
    installed("H:/small.CHN");
}
static void borrowed(uint8_t originals[3]) {
    unsigned i,at;
    for(i=0;i<3;++i) {
        originals[i]=(uint8_t)(40+i);check(!owned[40+i]);owned[40+i]=1;
        for(at=0;at<16384;++at)pages[40+i][at]=(uint8_t)(i*71+at*13+(at>>8));
        memcpy(rollback_expected[i],pages[40+i],16384);
    }
}
static void borrowed_unchanged(void) {
    unsigned i;for(i=0;i<3;++i)check(!memcmp(pages[40+i],rollback_expected[i],16384));
}
static void release_borrowed(void) {
    unsigned i;for(i=40;i<43;++i)check(!pfree((uint8_t)i));check(!count());
}
static void rollback_cases(void) {
    uint8_t originals[3],saved[3],identity[32],kind,ready;unsigned i,j,at,allocated;
    /* Two-page target grows by exactly one page, three-page target reuses all
     * three; every byte of caller code/data/stack is captured and restored. */
    for(i=0;i<2;++i) {
        setup(0);check(execute_prepare(&load,i?"H:/x-max.COM":"H:/x-small.COM"));
        allocated=allocs;memcpy(identity,load.id,32);borrowed(originals);
        check(execute_snapshot(&load,originals)&&load.ready&&load.kind==IMAGE_ROLLBACK);
        check(allocs==allocated+!i&&count()==6&&!memcmp(identity,load.id,32));
        for(j=0;j<3;++j) {
            for(at=0;at<16384;++at)check(pages[load.pages[j]][at]==rollback_expected[j][at]);
            memset(pages[originals[j]],0x6a,16384);
        }
        check(!execute_snapshot(&load,originals)&&load.ready&&load.kind==IMAGE_ROLLBACK);
        load.error=0x99;load.detail=ERR_FAILURE;stack_page=originals[2];
        check(!execute_restore(&load)&&load.ready&&load.error==0x99&&load.detail==ERR_FAILURE);
        for(j=0;j<3;++j)for(at=0;at<16384;++at)check(pages[originals[j]][at]==0x6a);
        stack_page=62;check(execute_restore(&load)&&!load.ready);
        check(load.error==0x99&&load.detail==ERR_FAILURE);borrowed_unchanged();
        check(!execute_restore(&load));borrowed_unchanged();
        check(chain_discard(&load)&&count()==3);borrowed_unchanged();release_borrowed();
    }
    /* Reject every alias/invalid mapping before changing any page. An invalid
     * request must preserve both an authenticated image and a ready backup. */
    for(i=0;i<2;++i)for(j=0;j<18;++j) {
        setup(0);check(execute_prepare(&load,"H:/x-max.COM"));borrowed(originals);
        if(i)check(execute_snapshot(&load,originals));
        memcpy(saved,load.pages,3);kind=load.kind;ready=load.ready;
        for(at=0;at<3;++at)memcpy(staged_expected[at],pages[saved[at]],16384);
        switch(j) {
        case 0: originals[0]=31;break;
        case 1: originals[1]=64;break;
        case 2: originals[2]=255;break;
        case 3: originals[2]=originals[0];break;
        case 4: originals[1]=saved[0];break;
        case 5: code_page=originals[0];break;
        case 6: stack_page=originals[1];break;
        case 7: load.pages[0]=31;break;
        case 8: load.pages[1]=64;break;
        case 9: load.pages[2]=255;break;
        case 10: load.pages[1]=saved[0];break;
        case 11: load.pages[2]=originals[2];break;
        case 12: code_page=saved[1];break;
        case 13: stack_page=saved[2];break;
        case 14: load.kind=IMAGE_CHAIN;break;
        case 15: load.ready=0;break;
        case 16: load.fd=2;break;
        case 17: load.pages[0]=255;break;
        }
        if(i)memcpy(load.originals,originals,3);
        /* A missing third slot is a valid small-image allocation request;
         * this max image has no legitimate missing page. */
        check(!(i?execute_restore(&load):execute_snapshot(&load,originals)));
        check(load.error==0x20&&load.ready==(j==15?0:ready));borrowed_unchanged();
        for(at=0;at<3;++at)check(!memcmp(staged_expected[at],pages[saved[at]],16384));
        memcpy(load.pages,saved,3);load.kind=kind;load.fd=-1;code_page=63;stack_page=62;
        check(chain_discard(&load)&&count()==3);release_borrowed();
    }
    setup(0);check(execute_prepare(&load,"H:/x-small.COM"));borrowed(originals);
    for(i=0;i<2;++i)memcpy(staged_expected[i],pages[load.pages[i]],16384);
    fail_alloc=allocs+1;check(!execute_snapshot(&load,originals));
    check(load.error==0x1ff&&load.detail==ERR_FAILURE&&load.ready&&load.kind==IMAGE_EXECUTABLE&&load.pages[2]==255);
    for(i=0;i<2;++i)check(!memcmp(staged_expected[i],pages[load.pages[i]],16384));
    borrowed_unchanged();
    fail_alloc=0;check(execute_snapshot(&load,originals));
    check(execute_restore(&load)&&load.error==0x1ff&&load.detail==ERR_FAILURE);
    check(chain_discard(&load)&&count()==3);release_borrowed();
    for(i=0;i<3;++i) {
        setup(0);check(execute_prepare(&load,"H:/x-small.COM"));borrowed(originals);
        check(execute_snapshot(&load,originals));fail_drop=load.pages[i];
        check(!chain_discard(&load)&&count()==4&&load.pages[i]==fail_drop&&!load.ready);
        check(!execute_restore(&load));borrowed_unchanged();fail_drop=-1;
        check(chain_discard(&load)&&count()==3);borrowed_unchanged();release_borrowed();
    }
}
static void override_cases(void) {
    uint8_t originals[3];unsigned i;
    static const uint8_t errors[]={ERR_NO_SUCH_ENTRY,ERR_ENTRY_CORRUPTED,ERR_FAILURE,0};
    static const uint16_t expected[]={1,0x99,0x99,0x99};
    for(i=0;i<sizeof(errors);++i) {
        setup(0);check(execute_prepare(&load,"H:/x-ready.COM")&&load.handoff);borrowed(originals);
        memset(pages[originals[2]]+0x3c00,0,EXECUTE_HANDOFF_BYTES);
        memset(rollback_expected[2]+0x3c00,0,EXECUTE_HANDOFF_BYTES);
        stack_page=originals[2];override_calls=switch_calls=0;override_error=errors[i];
        check(!execute_override(&load,"H:/checked.COM",originals));
        check(override_calls==1&&switch_calls==1&&stack_page==originals[2]);
        check(load.error==expected[i]&&load.detail==(errors[i]?errors[i]:ERR_FAILURE));
        check(!load.ready&&load.kind==IMAGE_ROLLBACK&&count()==6);borrowed_unchanged();
        stack_page=62;check(chain_discard(&load)&&count()==3);release_borrowed();
    }
    setup(0);check(execute_prepare(&load,"H:/x-readys.COM"));borrowed(originals);
    override_calls=switch_calls=0;stack_page=originals[2];fail_alloc=allocs+1;
    check(!execute_override(&load,"H:/checked.COM",originals));
    check(!override_calls&&switch_calls==1&&stack_page==originals[2]&&load.ready&&load.kind==IMAGE_EXECUTABLE);
    check(load.error==0x1ff);borrowed_unchanged();
    stack_page=62;check(chain_discard(&load)&&count()==3);release_borrowed();
    setup(0);check(execute_prepare(&load,"H:/x-readys.COM"));borrowed(originals);
    override_calls=switch_calls=0;
    check(!execute_override(&load,"",originals)&&!switch_calls&&!override_calls&&load.ready);
    {
        char path[129];memset(path,'x',128);path[128]=0;
        check(!execute_override(&load,path,originals)&&!switch_calls&&!override_calls&&load.ready);
    }
    check(chain_discard(&load)&&count()==3);release_borrowed();
    setup(0);check(execute_prepare(&load,"H:/x-small.COM")&&!load.handoff);
    override_calls=switch_calls=0;
    check(!execute_override(&load,"H:/checked.COM",originals)&&!switch_calls&&!override_calls&&load.ready);
    check(load.error==0x20&&chain_discard(&load)&&!count());
}
static void handoff_cases(void) {
    unsigned i;char path[48];uint8_t originals[3]={40,41,42};
    for(i=0;i<execute_entry_length;++i) {
        setup(0);snprintf(path,sizeof(path),"H:/x-h%u.COM",i);
        check(execute_prepare(&load,path)&&load.ready&&!load.error&&!load.handoff);
        override_calls=switch_calls=0;
        check(!execute_override(&load,"H:/checked.COM",originals)&&!override_calls&&!switch_calls);
        check(load.error==0x20&&chain_discard(&load)&&!count());
    }
    for(i=0xbfff;i<=0xc001;++i) {
        setup(0);snprintf(path,sizeof(path),"H:/x-h%x.COM",i);
        check(execute_prepare(&load,path)&&load.ready&&load.handoff&&load.entry==i);
        check(chain_discard(&load)&&!count());
    }
}
int main(int argc,char **argv) {
    unsigned i,total_reads,total_allocs;uint16_t floor;
    check(argc==2);snprintf(directory,sizeof(directory),"%s",argv[1]);
    setup(0);accepted("H:/small.CHN");check(load.floor==0x9080&&load.frontier==0x9080&&!load.trim_at&&count()==1);installed("H:/small.CHN");
    setup(0);accepted("H:/grow.CHN");check(load.floor==0xd800&&load.frontier==0xd800&&count()==2);installed("H:/grow.CHN");
    setup(1);accepted("H:/grow.CHN");check(load.floor==0xd800&&load.frontier==0xe020&&load.trim_at==0xd800&&load.trim_value==(0x8000u|0x7fe));installed("H:/grow.CHN");
    setup(1);accepted("H:/odd.CHN");check(load.floor==0xd802&&load.trim_at==0xd802&&load.trim_value==(0x8000u|0x7fc));installed("H:/odd.CHN");
    for(floor=0xd7fe;floor<=0xd802;floor+=2) {
        setup(0);caller.heap_frontier=floor+2;put(0x9020,0x8000u|(floor-0x9020-2));put(floor,0);memcpy(before,user,sizeof(user));
        if(floor<0xd800)rejected("H:/grow.CHN");
        else {accepted("H:/grow.CHN");check(load.trim_at==(floor==0xd802?0xd800:0));installed("H:/grow.CHN");}
    }
    setup(1);accepted("H:/short.CHN");check(load.floor==0x9080&&load.trim_at==0x9080&&load.frontier==0xe020);installed("H:/short.CHN");
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
    {const char *bad[]={"abi","origin","end","globals","truncated","trailing","hash","reserved","overlay","empty","legacy","entry-short","unprofiled"};
     char path[48];for(i=0;i<sizeof(bad)/sizeof(*bad);++i){setup(1);snprintf(path,sizeof(path),"H:/%s.CHN",bad[i]);rejected(path);}}
    for(i=0;i<CHAIN_PROFILE_BYTES;++i) {
        char path[48];setup(1);snprintf(path,sizeof(path),"H:/profile-%u.CHN",i);rejected(path);
        check(load.error==0x20&&allocs==1&&closes==1);
        setup(1);caller.profile[i]^=1;rejected("H:/grow.CHN");check(load.error==0x20&&allocs==2&&closes==1);
    }
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
    setup(1);fail_drop=32;check(!chain_prepare(&load,"H:/profile-0.CHN",&caller));check(load.error==0x20&&load.detail==ERR_ENTRY_CORRUPTED&&count()==1&&!handle);unchanged();fail_drop=-1;check(chain_discard(&load)&&!count());
    executable_cases();
    rollback_cases();
    override_cases();
    handoff_cases();
    printf("PASS: %u image staging/heap/identity/commit assertions across %u scenarios\n",checks,scenarios);return 0;
}
