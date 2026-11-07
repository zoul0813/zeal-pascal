/* Allocation/stream oracle; full Pascal metadata validation has its own tests. */
#include "compiler_units.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>
#include <assert.h>
#include <stdio.h>
Compiler cc;uint8_t platform_error;volatile uint32_t page_versions[64];
static Diagnostic diagnostic;
static uint8_t disk[65536],metadata[65536],arena[4096];
static uint16_t length,position,stored,records;static uint8_t pages,limit;
static int polls,cancel_at;static unsigned checks,validation_calls;
static uint32_t stream_hash;
static uint16_t get(const uint8_t *b,uint16_t at){return b[at]|((uint16_t)b[at+1]<<8);}
static void put(uint8_t *b,uint16_t at,uint16_t value){b[at]=(uint8_t)value;b[at+1]=(uint8_t)(value>>8);}
static uint32_t hash(const uint8_t *b,uint16_t n,uint8_t skip){uint32_t h=2166136261UL;uint16_t i;for(i=0;i<n;++i)if(!skip||i<28||i>=32)h=(h^b[i])*16777619UL;return h;}
static void check(uint8_t value){++checks;assert(value);}
uint8_t buffer_get(uint8_t page,uint16_t at){assert(page==32&&at<stored);return metadata[at];}
void buffer_set(uint8_t page,uint16_t at,uint8_t value){assert(page==32&&at<(uint32_t)pages*PAGE_SIZE);metadata[at]=value;if(at>=stored)stored=at+1;++page_versions[32];}
uint8_t buffer_reserve(uint8_t page,uint16_t n){uint8_t need=(uint8_t)(((uint32_t)n+PAGE_SIZE-1)/PAGE_SIZE);assert(page==32);if(need>limit){platform_error=22;return 0;}if(need>pages)pages=need;return 1;}
uint8_t cc_unit_byte(uint16_t at){assert(at<sizeof(arena));return arena[at];}
void cc_unit_set(uint16_t at,uint8_t value){assert(at<sizeof(arena));arena[at]=value;}
uint16_t cc_unit_word(uint16_t at){assert(at+1<sizeof(arena));return get(arena,at);}
void cc_unit_put(uint16_t at,uint16_t value){assert(at+1<sizeof(arena));put(arena,at,value);}
uint16_t cc_unit_owned_page(void){cc_unit_put(256,UR_MODULE);cc_unit_put(258,32);cc_unit_put(268,UR_SEAL);pages=1;records=280;return 256;}
uint16_t cc_unit_record(uint16_t kind,uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint16_t e){uint16_t at=records;records+=12;cc_unit_put(UH_USED,records);cc_unit_put(at,kind);cc_unit_put(at+2,a);cc_unit_put(at+4,b);cc_unit_put(at+6,c);cc_unit_put(at+8,d);cc_unit_put(at+10,e);return at;}
uint8_t cc_unit_validate_object(uint16_t module,const char *name,uint32_t actual){
    uint32_t expected=get(metadata,28)|((uint32_t)get(metadata,30)<<16);
    assert(module==256&&!strcmp(name,"fixture"));++validation_calls;stream_hash=actual;
    return expected==actual;
}
void cc_fail(uint8_t code){if(!diagnostic.code)diagnostic.code=code;}
uint8_t cc_poll(void){if(++polls==cancel_at){cc_fail(COMPILE_INTERRUPTED);return 0;}return 1;}
zos_err_t close(zos_dev_t fd){assert(fd==3);return 0;}
int8_t output_open_path(const char *path,uint8_t flags){assert(!strcmp(path,"file")&&flags==O_RDONLY);return 3;}
uint8_t output_finish(int8_t fd,uint8_t ok){close(fd);return ok;}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size){uint16_t n=*size;assert(fd==3);if(n>length-position)n=length-position;memcpy(out,disk+position,n);position+=n;*size=n;return 0;}
static uint16_t fixture(uint16_t blob,uint16_t fragments){
    uint16_t i,prefix=blob+16+8*fragments;uint32_t h;
    check(cc_unit_files_close());memset(&diagnostic,0,sizeof(diagnostic));cc.error=&diagnostic;
    memset(arena,0,sizeof(arena));stored=position=0;polls=0;cancel_at=-1;validation_calls=0;
    pages=0;limit=4;length=65470;
    for(i=0;i<length;++i)disk[i]=(uint8_t)(i*13u+7u);
    memcpy(disk,"ZPU1",4);put(disk,6,length);put(disk,26,blob);
    memcpy(disk+blob,"ZPF1",4);put(disk,blob+6,fragments);put(disk,blob+8,0);put(disk,blob+10,length-prefix);
    h=hash(disk,length,1);put(disk,28,(uint16_t)h);put(disk,30,(uint16_t)(h>>16));return prefix;
}
int main(void){
    uint16_t prefix,i;uint32_t actual,expected;
    const uint16_t blobs[]={48,16360,16380,36004};const uint16_t fragments[]={1,1,1,18};
    for(i=0;i<4;++i){
        prefix=fixture(blobs[i],fragments[i]);limit=(uint8_t)(((uint32_t)prefix+64+PAGE_SIZE-1)/PAGE_SIZE);
        check(cc_units_load_object("fixture","file",0)==256&&!diagnostic.code);
        check(pages==limit&&stored==prefix&&cc_unit_prefix(256)==prefix&&validation_calls==1);
        check(stream_hash==hash(disk,length,1)&&!memcmp(metadata,disk,prefix));
        {uint8_t index;for(index=0;index<4;++index){
            uint16_t base=(uint16_t)index*PAGE_SIZE,n=length-base;if(n>PAGE_SIZE)n=PAGE_SIZE;
            expected=hash(disk+base,n,0);actual=cc_unit_word(UNIT_HASH_AT(256,2u*index))|((uint32_t)cc_unit_word(UNIT_HASH_AT(256,2u*index+1))<<16);
            check(actual==expected);
        }}
    }
    prefix=fixture(16380,1);limit=1;check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==99&&platform_error==22);
    fixture(48,1);disk[50000]^=1;check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==UNIT_FORMAT&&validation_calls==1);
    fixture(48,1);--length;check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==UNIT_FORMAT);
    fixture(48,1);put(disk,26,65535);check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==UNIT_FORMAT);
    fixture(48,1);put(disk,48+8,65535);check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==UNIT_FORMAT);
    fixture(48,1);cancel_at=3;check(!cc_units_load_object("fixture","file",0)&&diagnostic.code==COMPILE_INTERRUPTED);
    check(cc_unit_files_close());printf("%u unit loader assertions passed\n",checks);return 0;
}
