/* Execute the real saved-map reader against a deterministic read-only SDK.
 * Format fixtures and their hashes are authored independently in Python. */
#include "saved_map.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static uint8_t memory[64][PAGE_SIZE],roles[64];
static uint32_t lifetimes[64],sequence;
volatile uint32_t page_versions[64];
uint8_t console_scratch[256],platform_error;
static FILE *handles[16];
static char directory[1024];
static unsigned assertions,opens,reads,closes,seeks,allocations;
static unsigned fail_open,fail_read,fail_close,fail_seek,fail_alloc;
static int fail_drop=-1;
static uint8_t short_read,zero_read,oversize,tamper_seek,chain;
static void check_at(int ok,unsigned line) {
    ++assertions;
    if(!ok) {fprintf(stderr,"saved-map check %u at line %u, error %u\n",assertions,line,platform_error);abort();}
}
#define check(x) check_at((x),__LINE__)
uint8_t page_new(uint8_t role) {
    uint8_t i;if(++allocations==fail_alloc)return 255;
    for(i=32;i<64;++i)if(!roles[i]) {
        roles[i]=role;lifetimes[i]=++sequence;memset(memory[i],0xa5,PAGE_SIZE);return i;
    }
    return 255;
}
uint8_t page_drop(uint8_t p) {
    assert(p>=32&&p<64&&roles[p]);
    if(p==fail_drop){platform_error=ERR_FAILURE;return 0;}
    roles[p]=0;return 1;
}
uint8_t page_role(uint8_t p) {return p<64?roles[p]:0;}
uint32_t page_lifetime(uint8_t p) {return p<64&&roles[p]?lifetimes[p]:0;}
uint8_t far_get(uint8_t p,uint16_t at) {assert(p<64&&roles[p]&&at<PAGE_SIZE);return memory[p][at];}
void far_set(uint8_t p,uint16_t at,uint8_t b) {assert(p<64&&roles[p]&&at<PAGE_SIZE);memory[p][at]=b;++page_versions[p];}
void page_initialized(uint8_t p,uint16_t n) {assert(p<64&&roles[p]&&n<=PAGE_SIZE);}
static unsigned owned(void) {unsigned n=0,i;for(i=32;i<64;++i)n+=roles[i]!=0;return n;}
static unsigned opened(void) {unsigned n=0,i;for(i=2;i<16;++i)n+=handles[i]!=NULL;return n;}
zos_dev_t open(const char *path,uint8_t flags) {
    char full[1200];unsigned i;
    assert(flags==O_RDONLY);
    if(++opens==fail_open)return -ERR_FAILURE;
    if(strncmp(path,"H:/",3))return -ERR_INVALID_PATH;
    snprintf(full,sizeof(full),"%s/%s",directory,path+3);
    for(i=2;i<16;++i)if(!handles[i]) {
        handles[i]=fopen(full,"rb");return handles[i]?(int8_t)i:-ERR_NO_SUCH_ENTRY;
    }
    return -ERR_NO_MORE_ENTRIES;
}
zos_err_t close(zos_dev_t fd) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd]);
    check(fclose(handles[(uint8_t)fd])==0);handles[(uint8_t)fd]=NULL;
    return ++closes==fail_close?ERR_FAILURE:0;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size) {
    size_t n;
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd]);
    if(++reads==fail_read)return ERR_FAILURE;
    if(zero_read){*size=0;return 0;}
    if(oversize){++*size;return 0;}
    n=*size;if(short_read&&n>3)n=3;
    n=fread(out,1,n,handles[(uint8_t)fd]);if(ferror(handles[(uint8_t)fd]))return ERR_FAILURE;
    if(tamper_seek==2&&n>=40){((uint8_t*)out)[8]^=1;tamper_seek=0;}
    *size=(uint16_t)n;return 0;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd]&&whence==SEEK_SET);
    if(++seeks==fail_seek)return ERR_FAILURE;
    if(tamper_seek==1)tamper_seek=2;
    if(fseek(handles[(uint8_t)fd],*at,SEEK_SET))return ERR_FAILURE;
    *at=(int32_t)ftell(handles[(uint8_t)fd]);return 0;
}
uint8_t read_block(Source *source,const char *path,Diagnostic *d) {
    SourceImport in;int8_t fd;uint16_t n,i;uint8_t ok=1,error;
    if(!source_new(source,path)){platform_error=ERR_NO_MORE_MEMORY;return 0;}
    fd=open(path,O_RDONLY);if(fd<0){source_close(source);platform_error=(uint8_t)-fd;return 0;}
    if(!import_begin(&in,path,d)){close(fd);source_close(source);platform_error=ERR_NO_MORE_MEMORY;return 0;}
    while(!in.eof) {
        n=128;platform_error=read(fd,console_scratch,&n);
        if(platform_error||n>128){ok=0;break;}
        if(!n)break;
        for(i=0;i<n;++i)if(!import_byte(&in,console_scratch[i],d)){ok=0;break;}
        if(!ok)break;
    }
    error=close(fd);if(ok&&error){platform_error=error;ok=0;}
    if(!ok){error=platform_error;page_drop(in.staged.page);page_drop(source->page);platform_error=error;return 0;}
    return import_finish(&in,source,d);
}
static void reset(void) {
    assert(!opened());opens=reads=closes=seeks=allocations=0;
    fail_open=fail_read=fail_close=fail_seek=fail_alloc=0;
    short_read=zero_read=oversize=tamper_seek=0;
}
static uint8_t lookup(const char *root,const Sources *buffers,uint16_t pc,SavedLocation *location) {
    return saved_location_kind(root,buffers,pc,location,chain);
}
static void locate(uint8_t expected,uint16_t pc) {
    SavedLocation result,before;unsigned pages=owned();uint8_t ok;
    memset(&result,0xa5,sizeof(result));before=result;
    ok=lookup("H:/main.pas",NULL,pc,&result);check(ok==expected);
    check(!opened());
    if(owned()!=pages)fprintf(stderr,"pages before %u after %u; failures open/read/close/alloc %u/%u/%u/%u\n",pages,owned(),fail_open,fail_read,fail_close,fail_alloc);
    check(owned()==pages);
    if(!ok)check(!memcmp(&result,&before,sizeof(result)));
    else {check(!strcmp(result.path,"H:/parts/fail.inc"));check(result.position==8);}
}
int main(int argc,char **argv) {
    unsigned i,open_count,read_count,close_count,alloc_count;
    char paths[2][128];Source root;Sources buffers;SavedLocation result;Diagnostic d;
    check(argc==3);chain=(uint8_t)(atoi(argv[2])>>2);snprintf(directory,sizeof(directory),"%s",argv[1]);
    if((atoi(argv[2])&3)==1){reset();locate(0,5);printf("PASS: %u saved-map rejection checks\n",assertions);return 0;}
    if((atoi(argv[2])&3)==2){reset();locate(1,5);reset();fail_alloc=2;locate(0,5);printf("PASS: %u wide/reordered saved-map checks\n",assertions);return 0;}
    reset();locate(1,5);
    open_count=opens;read_count=reads;close_count=closes;alloc_count=allocations;
    reset();short_read=1;locate(1,5);
    reset();locate(0,0xffff);
    reset();locate(0,31); /* Valid code extent, unmapped PC. */
    reset();zero_read=1;locate(0,5);
    reset();oversize=1;locate(0,5);
    reset();tamper_seek=1;locate(0,5);
    reset();fail_seek=1;locate(0,5);check(platform_error==ERR_FAILURE);
    for(i=1;i<=open_count;++i){reset();fail_open=i;locate(0,5);check(platform_error==ERR_FAILURE);}
    for(i=1;i<=close_count;++i){reset();fail_close=i;locate(0,5);check(platform_error==ERR_FAILURE);}
    for(i=1;i<=read_count;++i){reset();fail_read=i;locate(0,5);check(platform_error==ERR_FAILURE);}
    for(i=1;i<=alloc_count;++i){reset();fail_alloc=i;locate(0,5);}
    reset();check(saved_paths("H:/main.pas",paths));check(!strcmp(paths[0],"H:/main.COM")&&!strcmp(paths[1],"H:/main.ZMP"));
    check(saved_paths_kind("H:/main.pas",paths,1)&&!strcmp(paths[0],"H:/main.CHN"));
    check(saved_paths("H:/main.COM",paths));check(!strcmp(paths[0],"H:/main.COM"));
    check(saved_paths("H:/main.ZMP",paths));check(!strcmp(paths[1],"H:/main.ZMP"));
    check(!saved_paths("H:/123456789012345",paths));check(!saved_paths("H:/parts/../main.pas",paths));
    reset();check(sources_new(&buffers));check(read_block(&root,"H:/main.pas",&d));root.dirty=1;
    check(sources_take(&buffers,&root,&root)>=0);
    check(lookup("H:/main.pas",&buffers,5,&result));check(!strcmp(result.path,"H:/parts/fail.inc"));
    ++buffers.version;check(!lookup("H:/main.pas",&buffers,5,&result));--buffers.version;
    check(source_replace(&root,0,0,(const uint8_t*)"{dirty}\n",8));check(sources_take(&buffers,&root,&root)>=0);
    memset(&result,0xa5,sizeof(result));check(!lookup("H:/main.pas",&buffers,5,&result));check(result.path[0]==(char)0xa5);
    check(root.dirty&&root.cursor==8);check(sources_close(&buffers));check(!opened()&&!owned());
    reset();fail_drop=32;
    check(!lookup("H:/main.pas",NULL,5,&result));check(owned()==1);
    fail_drop=-1;reset();check(lookup("H:/main.pas",NULL,5,&result));
    check(!opened()); /* Pending owner is retired before retry. */
    check(!owned());
    reset();fail_drop=34;check(!lookup("H:/main.pas",NULL,5,&result));check(owned()==2);
    fail_drop=-1;reset();check(lookup("H:/main.pas",NULL,5,&result));check(!owned()&&!opened());
    reset();fail_drop=33;check(!lookup("H:/main.pas",NULL,5,&result));check(owned()==2);
    fail_drop=-1;reset();check(lookup("H:/main.pas",NULL,5,&result));check(!owned()&&!opened());
    printf("PASS: %u saved-map format, identity, source, short-I/O and ownership checks\n",assertions);
    return 0;
}
