/* Exercise the real group close with a frozen catalog, including 16-bit wrap. */
#include "compiler/groups/groups.h"
#include <zos_errors.h>
#include <assert.h>
#include <stdio.h>
Compiler cc;
uint8_t platform_error;
volatile uint32_t page_versions[64];
static uint8_t data[3][PAGE_SIZE];
static Build image;
uint8_t page_role(uint8_t page){return page==33?PAGE_IMAGE:PAGE_COMPILER;}
uint32_t page_lifetime(uint8_t page){assert(page>=32&&page<=34);return 1;}
uint8_t far_get(uint8_t page,uint16_t at){assert(page>=32&&page<=34&&at<PAGE_SIZE);return data[page-32][at];}
void far_set(uint8_t page,uint16_t at,uint8_t value){assert(page>=32&&page<=34&&at<PAGE_SIZE);data[page-32][at]=value;++page_versions[page];}
uint8_t output_poll(void){return 1;}
static void put(uint16_t at,uint16_t value){data[0][at]=(uint8_t)value;data[0][at+1]=(uint8_t)(value>>8);}
int main(void){
    static const uint16_t starts[]={0xfff0,0xfff0,0x9000,0xbff0};
    static const uint16_t sizes[]={15,16,0x8000,32};
    unsigned n,p;
    for(n=0;n<4;++n){
        CompilerGroups groups;
        memset(&groups,0,sizeof(groups));memset(data,0xaa,sizeof(data));
        memset(data[0],0,PAGE_SIZE);memset(&cc,0,sizeof(cc));
        groups.page=32;groups.groups=1;groups.lifetime=groups.version=1;
        page_versions[32]=1;platform_error=0;
        cc.image=&image;image.pages[2]=33;cc.stage_page=34;cc.emitted=starts[n];
        put(48,starts[n]);put(50,sizes[n]);put(60,1);put(62,1);
        if(n==1||n==2){
            assert(!cc_group_close(&groups,0));
            assert(cc.emitted==starts[n]&&groups.store.failed&&platform_error==ERR_NO_MORE_MEMORY);
            for(p=0;p<PAGE_SIZE;++p)assert(data[1][p]==0xaa&&data[2][p]==0xaa);
        }else{
            assert(cc_group_close(&groups,0));
            assert(cc.emitted==(uint32_t)starts[n]+sizes[n]&&!platform_error&&!groups.store.failed);
            for(p=0x8000;p<0x10000;++p){
                uint8_t want=p>=starts[n]&&p<(uint32_t)starts[n]+sizes[n]?0:0xaa;
                assert(data[p<0xc000?1:2][p&0x3fff]==want);
            }
        }
    }
    puts("4 real group-close boundary cases passed (including wrap and canaries)");return 0;
}
