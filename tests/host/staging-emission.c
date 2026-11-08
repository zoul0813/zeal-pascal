#include "compiler.h"
#include "compiler_disk.h"
#include <assert.h>
#include <stdio.h>
Compiler cc;
static Build image;
static CompilerSources sources;
static Diagnostic diagnostic;
static uint8_t stage[32768],relocations[8];
uint8_t compiler_disk_placed;
uint8_t cc_poll(void){return 1;}
void far_set(uint8_t page,uint16_t at,uint8_t value){assert(page==32&&at<PAGE_SIZE);stage[at]=value;}
void cc_stage_put(uint16_t at,uint8_t value){assert(at<PAGE_SIZE);stage[PAGE_SIZE+at]=value;}
void cc_reloc_put(uint16_t at,uint8_t value){assert(at<sizeof(relocations));relocations[at]=value;}
uint8_t cc_disk_active(const Build *b){(void)b;return 0;}
void cc_disk_put(uint16_t at,uint8_t value){(void)at;(void)value;assert(0);}
void cc_disk_final_put(uint16_t at,uint8_t value){(void)at;(void)value;assert(0);}
uint8_t cc_image_page(uint8_t page){(void)page;assert(0);return 0;}
int main(void){
    cc.image=&image;cc.sources=&sources;cc.error=&diagnostic;cc.primary_stage=32;
    cc.emitted=0xfffe;cc_emit(0xa5);
    assert(!diagnostic.code&&cc.emitted==0xffff&&stage[0x7ffe]==0xa5);
    cc_emit(0x5a);
    assert(diagnostic.code==99&&cc.emitted==0xffff&&stage[0x7fff]==0);
    diagnostic.code=0;
    cc_relocate_value(0xfffd,1,0);cc_relocate_value(0xfffe,1,1);
    assert(!diagnostic.code&&cc.relocations==2);
    assert(relocations[0]==0xfd&&relocations[1]==0x7f);
    assert(relocations[4]==0xfe&&relocations[5]==0xff);
    cc_relocate_value(0xffff,1,1);
    assert(diagnostic.code==99&&cc.relocations==2);
    diagnostic.code=0;sources.unit_mode=1;cc.emitted=0xfffd;
    cc_emit(0x42);
    assert(!diagnostic.code&&cc.emitted==0xfffe&&stage[0x7ffd]==0x42);
    cc_emit(0x24);
    assert(diagnostic.code==99&&cc.emitted==0xfffe&&stage[0x7ffe]==0xa5);
    diagnostic.code=0;cc_relocate_value(0xfffe,1,1);
    assert(diagnostic.code==99&&cc.relocations==2);
    puts("9 emission and relocation boundary assertions passed");return 0;
}
