/* Independent disk and page oracle for production unit stream ownership. */
#include "units/internal.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <assert.h>
#include "storage/transport.h"
#include <zos_errors.h>
Compiler cc;uint8_t platform_error;
static Diagnostic diagnostic;
static uint8_t disk[65536],window[PAGE_SIZE],arena[4096];
static const uint16_t sparse_starts[]={16370,32,4090,9},sparse_sizes[]={40,100,75,70};
static uint8_t sparse,selected;static CompilerFragmentLayout layout;
static unsigned written;
static uint32_t position;static uint16_t disk_length;
static uint8_t seek_error,read_error,short_read,wrong_seek,close_error,max_read,open_error;
static uint16_t records;
static int cancel_at,polls;static unsigned checks,initialized,closes[128];
static void check(uint8_t value){++checks;assert(value);}
uint8_t page_role(uint8_t page){return page==32?PAGE_COMPILER:0;}
void cc_put(uint16_t address,uint8_t value){assert(address>=0x8000&&address<0xc000);window[address-0x8000]=value;++written;}
uint8_t cc_fragment_layout_get(const CompilerFragmentLayout *l,uint16_t identity,uint16_t fragment,uint16_t *address){assert(l==&layout&&identity==1&&fragment<4);*address=0x8000+(sparse?512*fragment:0);return sparse||fragment==selected?CF_OK:CF_TARGET;}
uint16_t cc_unit_module_word(uint16_t module,uint16_t at){
    assert(module==256);if(at==48+6)return 4;
    if(at>=64&&at<96){uint16_t fragment=(at-64)/8,field=(at-64)%8;
        if(!field)return sparse?sparse_starts[fragment]:fragment*PAGE_SIZE;
        if(field==2)return sparse?sparse_sizes[fragment]:fragment==3?65470-3*PAGE_SIZE:PAGE_SIZE;
    }assert(0);return 0;
}
uint8_t cc_unit_byte(uint16_t at){assert(at<sizeof(arena));return arena[at];}
void cc_unit_set(uint16_t at,uint8_t value){assert(at<sizeof(arena));arena[at]=value;}
uint16_t cc_unit_word(uint16_t at){assert(at+1<sizeof(arena));return arena[at]|((uint16_t)arena[at+1]<<8);}
static void put(uint16_t at,uint16_t value){arena[at]=(uint8_t)value;arena[at+1]=(uint8_t)(value>>8);}
uint16_t cc_unit_record(uint16_t kind,uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint16_t e){
    uint16_t at=records;records+=12;put(UH_USED,records);put(at,kind);put(at+2,a);put(at+4,b);put(at+6,c);put(at+8,d);put(at+10,e);return at;
}
int8_t output_open_path(const char *path,uint8_t flags){assert(!strcmp(path,"file")&&flags==O_RDONLY);platform_error=open_error;return open_error?-1:3;}
uint8_t output_finish(int8_t fd,uint8_t ok){close(fd);return ok;}
void cc_fail(uint8_t code){if(!diagnostic.code)diagnostic.code=code;}
uint8_t cc_poll(void){if(++polls==cancel_at){cc_fail(COMPILE_INTERRUPTED);return 0;}return 1;}
zos_err_t close(zos_dev_t fd){assert(fd>=0);++closes[(uint8_t)fd];return fd==3?close_error:0;}
zos_err_t seek(zos_dev_t fd,int32_t *offset,zos_whence_t whence){
    assert(fd==3&&whence==SEEK_SET&&*offset>=0);position=(uint32_t)*offset;
    if(wrong_seek)++*offset;return seek_error;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size){
    uint16_t n=*size;assert(fd==3&&position<=disk_length);
    if(n>disk_length-position)n=(uint16_t)(disk_length-position);
    if(max_read&&n>max_read)n=max_read;
    if(short_read&&n)--n;memcpy(out,disk+position,n);position+=n;*size=n;return read_error;
}
static void reset(void){
    unsigned i;uint32_t hash;uint16_t begin,end,at;
    close_error=0;assert(cc_unit_files_close());
    memset(&diagnostic,0,sizeof(diagnostic));cc.error=&diagnostic;
    memset(arena,0,sizeof(arena));memset(window,0xa5,sizeof(window));
    disk_length=65470;position=0;polls=0;cancel_at=-1;initialized=0;written=0;selected=sparse=0;
    seek_error=read_error=short_read=wrong_seek=platform_error=max_read=open_error=0;records=384;
    put(256+4,disk_length);put(256+8,1);put(256+UNIT_RECORD_BYTES+10,48);put(UNIT_FILE_AT(256)+2,cc_unit_path_store("file"));
    for(at=0;at<disk_length;++at)disk[at]=(uint8_t)(at*17u+(at>>14));
    for(i=0;i<4;++i){
        begin=(uint16_t)i*PAGE_SIZE;end=disk_length-begin;if(end>PAGE_SIZE)end=PAGE_SIZE;
        hash=2166136261UL;for(at=0;at<end;++at)hash=(hash^disk[begin+at])*16777619UL;
        put(UNIT_HASH_AT(256,2*i),(uint16_t)hash);put(UNIT_HASH_AT(256,2*i+1),(uint16_t)(hash>>16));
    }
}
int main(void){
    uint8_t i,slot;uint16_t at,n;
    for(i=0;i<4;++i){
        reset();selected=i;check(cc_unit_emit_page(256,i,&layout));check(!diagnostic.code&&written>0);
        n=disk_length-(uint16_t)i*PAGE_SIZE;if(n>PAGE_SIZE)n=PAGE_SIZE;
        for(at=0;at<n;++at)check(window[at]==disk[(uint16_t)i*PAGE_SIZE+at]);
    }
    reset();sparse=1;
    check(cc_unit_emit_page(256,0,&layout));check(cc_unit_emit_page(256,1,&layout));
    for(i=0;i<4;++i){
        for(at=0;at<sparse_sizes[i];++at)check(window[512*i+at]==disk[sparse_starts[i]+at]);
        check(window[512*i+sparse_sizes[i]]==0xa5);
    }
    reset();selected=2;disk[40000]^=1;check(!cc_unit_emit_page(256,2,&layout)&&diagnostic.code==UNIT_STALE);
    reset();selected=3;--disk_length;check(!cc_unit_emit_page(256,3,&layout)&&diagnostic.code==UNIT_STALE);
    reset();seek_error=11;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==COMPILE_PLATFORM_FAILURE&&platform_error==11);
    reset();read_error=11;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==COMPILE_PLATFORM_FAILURE&&platform_error==11);
    reset();wrong_seek=1;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==UNIT_STALE);
    reset();short_read=1;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==UNIT_STALE);
    reset();max_read=17;check(cc_unit_emit_page(256,0,&layout)&&!diagnostic.code&&written);
    reset();open_error=ERR_NO_SUCH_ENTRY;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==UNIT_STALE);
    reset();open_error=ERR_INVALID_PARAMETER;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==COMPILE_PLATFORM_FAILURE);
    reset();close_error=11;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==COMPILE_PLATFORM_FAILURE&&cc_unit_file(0)<0);
    reset();cancel_at=3;check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==COMPILE_INTERRUPTED);
    reset();check(!cc_unit_emit_page(256,4,&layout)&&diagnostic.code==UNIT_FORMAT);
    reset();check(!cc_unit_emit_page(256,0,0)&&diagnostic.code==UNIT_FORMAT);
    reset();put(UNIT_FILE_AT(256)+2,255);check(!cc_unit_emit_page(256,0,&layout)&&diagnostic.code==UNIT_FORMAT);
    reset();check(cc_unit_file_bind(3)==0);slot=cc_unit_file_bind(4);close_error=11;
    check(!cc_unit_files_close()&&platform_error==11&&cc_unit_file(0)<0&&slot==255);
    close_error=0;check(cc_unit_files_close()&&cc_unit_file(0)<0);
    for(i=0;i<CC_FRAGMENT_OBJECTS;++i){
        check(cc_unit_file_bind(i)==0&&cc_unit_file(0)==i);
        check(cc_unit_file_bind(32)==255);check(cc_unit_files_close());
    }
    check(cc_unit_files_close());check(cc_unit_files_close());
    reset();for(i=0;i<32;++i){check(cc_unit_path_open(256)==3);check(cc_unit_files_close());check(cc_unit_file(0)<0);}
    printf("%u unit reader assertions passed\n",checks);return 0;
}
