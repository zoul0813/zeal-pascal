/* Deterministic SDK semantics, including unchanged transfer counts on error.
 * The same component cases also run against the real native SDK/emulator. */
#include "file_runtime.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <string.h>
#include <assert.h>

uint8_t zf_memory_mode,model_pages,model_handles,model_close_error,model_read_error,model_write_error;
uint8_t model_zero_write,model_short,model_readonly;
uint8_t model_curdir_error;
static char current_directory[128]="A:/";
static uint8_t memory[32][16384],owned[32];
static struct { char path[128]; uint8_t data[8192]; uint32_t size; } files[64];
static struct { uint8_t used,file,flags; uint32_t position,cached_size; } handles[16];
void zf_record_io(uint8_t page,uint16_t offset) {
    assert(page>=32&&page<64&&owned[page-32]&&offset<=16384-ZF_RECORD_BYTES);
    if(zf_store) memcpy(memory[page-32]+offset,zf_record,ZF_RECORD_BYTES);
    else memcpy(zf_record,memory[page-32]+offset,ZF_RECORD_BYTES);
}
void zf_page_clear(uint8_t page) { assert(page>=32&&page<64&&owned[page-32]); memset(memory[page-32],0,16384); }
uint8_t zf_service(uint8_t operation,uint16_t argument) { (void)operation;(void)argument;assert(0);return 1; }
zos_err_t curdir(char *out) {
    if(model_curdir_error) return (zos_err_t)model_curdir_error;
    strcpy(out,current_directory); return 0;
}
zos_err_t chdir(const char *path) {
    if(strcmp(path,"A:/")&&strcmp(path,"H:/")) return ERR_NO_SUCH_ENTRY;
    strcpy(current_directory,path); return 0;
}
zos_err_t palloc(uint8_t *page) {
    uint8_t i;
    for(i=0;i<32;++i) if(!owned[i]) {
        owned[i]=1; ++model_pages; *page=i+32; memset(memory[i],0xa5,16384); return 0;
    }
    return ERR_NO_MORE_MEMORY;
}
zos_err_t pfree(uint8_t page) {
    if(page<32||page>=64||!owned[page-32]) return ERR_INVALID_PARAMETER;
    owned[page-32]=0; --model_pages; return 0;
}
zos_dev_t open(const char *name,uint8_t flags) {
    uint8_t i,n;
    for(i=0;i<64;++i) if(!strcmp(files[i].path,name)) break;
    if(i==64) {
        if(!(flags&O_CREAT)) return -ERR_NO_SUCH_ENTRY;
        for(i=0;i<64;++i) if(!files[i].path[0]) break;
        if(i==64) return -ERR_CANNOT_REGISTER_MORE;
        strcpy(files[i].path,name); files[i].size=0;
    }
    if(model_readonly&&(flags&(O_WRONLY|O_RDWR))) return -ERR_READ_ONLY;
    for(n=2;n<16;++n) if(!handles[n].used) break;
    if(n==16) return -ERR_CANNOT_REGISTER_MORE;
    handles[n].cached_size=files[i].size;
    if(flags&O_TRUNC) files[i].size=0;
    handles[n].used=1; handles[n].file=i; handles[n].flags=flags; handles[n].position=0;
    ++model_handles; return (int8_t)n;
}
zos_err_t close(zos_dev_t fd) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd].used);
    handles[(uint8_t)fd].used=0; --model_handles; return (zos_err_t)model_close_error;
}
zos_err_t dstat(zos_dev_t fd,zos_stat_t *out) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd].used);
    if(handles[(uint8_t)fd].flags&(O_RDWR|O_WRONLY)) return ERR_INVALID_FILESYSTEM;
    memset(out,0,sizeof(*out)); out->s_flags=1; out->s_size=files[handles[(uint8_t)fd].file].size; return 0;
}
zos_err_t stat(const char *name,zos_stat_t *out) {
    int8_t fd=open(name,O_RDONLY);
    zos_err_t error;
    if(fd<0) return (zos_err_t)-fd;
    error=dstat(fd,out); close(fd); return error;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    uint8_t n=(uint8_t)fd;
    uint32_t offset=(uint32_t)*at;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(whence==SEEK_CUR) offset+=handles[n].position;
    if(whence==SEEK_END) offset+=handles[n].cached_size;
    if(offset>handles[n].cached_size) offset=handles[n].cached_size;
    handles[n].position=offset; *at=(int32_t)offset; return 0;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size) {
    uint8_t n=(uint8_t)fd,file=handles[n].file;
    uint32_t left=files[file].size-handles[n].position;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(model_read_error) return (zos_err_t)model_read_error;
    if(handles[n].flags&O_WRONLY) return ERR_BAD_MODE;
    if(*size>left) *size=(uint16_t)left;
    if(model_short&&*size>1) *size=1;
    memcpy(out,files[file].data+handles[n].position,*size); handles[n].position+=*size; return 0;
}
zos_err_t write(zos_dev_t fd,const void *in,uint16_t *size) {
    uint8_t n=(uint8_t)fd,file=handles[n].file;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(model_write_error) return (zos_err_t)model_write_error;
    if(!(handles[n].flags&(O_WRONLY|O_RDWR))) return ERR_BAD_MODE;
    if(model_zero_write) *size=0;
    if(model_short&&*size>1) *size=1;
    assert(handles[n].position+*size<=sizeof(files[file].data));
    memcpy(files[file].data+handles[n].position,in,*size); handles[n].position+=*size;
    if(handles[n].position>files[file].size) files[file].size=handles[n].position;
    if(handles[n].position>handles[n].cached_size) handles[n].cached_size=handles[n].position;
    /* Match the pinned post-write metadata status. The SDK leaves *size
     * unchanged on a nonzero return, just as for a genuine driver failure. */
    {
        uint32_t limit=handles[n].cached_size,at=handles[n].position;
        if((limit>>24)!=(at>>24)) return (zos_err_t)(uint8_t)(limit>>24);
        if((limit>>16)!=(at>>16)) return (zos_err_t)(uint8_t)(limit>>16);
        if((limit>>8)!=(at>>8)) return (zos_err_t)(uint8_t)(limit>>8);
    }
    return 0;
}
zos_err_t rm(const char *name) {
    uint8_t i;
    for(i=0;i<64;++i) if(!strcmp(files[i].path,name)) { files[i].path[0]=0; return 0; }
    return ERR_NO_SUCH_ENTRY;
}
