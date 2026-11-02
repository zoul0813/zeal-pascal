/* Deterministic SDK semantics, including unchanged transfer counts on error.
 * The same component cases also run against the real native SDK/emulator. */
#include "file_runtime.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_serial.h>
#include <string.h>
#include <assert.h>

uint8_t zf_memory_mode,model_pages,model_handles,model_close_error,model_read_error,model_write_error;
uint8_t model_zero_write,model_short,model_readonly;
uint8_t model_curdir_error;
uint8_t model_rm_error,model_corrupt_write;
uint8_t model_free_error;
uint8_t model_oversize_read,model_oversize_write;
uint16_t model_open_calls,model_close_calls,model_read_calls,model_write_calls;
uint16_t model_fail_open_at,model_fail_close_at,model_fail_read_at,model_fail_write_at;
uint16_t model_zero_read_at;
uint8_t model_serial_present=1,model_serial_attr=1;
uint8_t model_serial_input[256],model_serial_output[512];
uint16_t model_serial_size,model_serial_position,model_serial_written;
uint16_t model_serial_ioctl_calls,model_serial_fail_ioctl_at,model_serial_seek_calls;
static char current_directory[128]="A:/";
static uint8_t memory[32][16384],owned[32];
static struct { char path[128]; uint8_t data[73728]; uint32_t size; uint8_t directory; } files[64];
static struct { uint8_t used,file,flags; uint32_t position,cached_size; } handles[16];
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
    if(model_free_error) return (zos_err_t)model_free_error;
    owned[page-32]=0; --model_pages; return 0;
}
zos_dev_t open(const char *name,uint8_t flags) {
    uint8_t i,n;
    if(++model_open_calls==model_fail_open_at) return -ERR_FAILURE;
    if(!strcmp(name,"#SER0")) {
        if(!model_serial_present) return -ERR_NO_SUCH_ENTRY;
        i=64;
    } else {
    if(strchr(name+3,'/')) return -ERR_NO_SUCH_ENTRY;
    for(i=0;i<64;++i) if(!strcmp(files[i].path,name)) break;
    if(i==64) {
        if(!(flags&O_CREAT)) return -ERR_NO_SUCH_ENTRY;
        for(i=0;i<64;++i) if(!files[i].path[0]) break;
        if(i==64) return -ERR_CANNOT_REGISTER_MORE;
        strcpy(files[i].path,name); files[i].size=0; files[i].directory=0;
    }
    if(files[i].directory) return -ERR_NOT_A_FILE;
    if(model_readonly&&(flags&(O_WRONLY|O_RDWR))) return -ERR_READ_ONLY;
    }
    for(n=2;n<16;++n) if(!handles[n].used) break;
    if(n==16) return -ERR_CANNOT_REGISTER_MORE;
    handles[n].cached_size=i<64?files[i].size:0;
    if(i<64&&(flags&O_TRUNC)) files[i].size=0;
    handles[n].used=1; handles[n].file=i; handles[n].flags=flags; handles[n].position=0;
    ++model_handles; return (int8_t)n;
}
zos_err_t close(zos_dev_t fd) {
    uint8_t fault=++model_close_calls==model_fail_close_at;
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd].used);
    handles[(uint8_t)fd].used=0; --model_handles;
    return fault?ERR_FAILURE:(zos_err_t)model_close_error;
}
zos_err_t ioctl(zos_dev_t fd,uint8_t command,void *argument) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd].used&&handles[(uint8_t)fd].file==64);
    if(++model_serial_ioctl_calls==model_serial_fail_ioctl_at) return ERR_FAILURE;
    if(command==SERIAL_CMD_GET_ATTR) *(uint8_t *)argument=model_serial_attr;
    else if(command==SERIAL_CMD_SET_ATTR) model_serial_attr=(uint8_t)(uintptr_t)argument;
    else return ERR_NOT_SUPPORTED;
    return 0;
}
zos_err_t dstat(zos_dev_t fd,zos_stat_t *out) {
    assert(fd>=2&&fd<16&&handles[(uint8_t)fd].used);
    if(handles[(uint8_t)fd].file==64) return ERR_NOT_SUPPORTED;
    if(handles[(uint8_t)fd].flags&(O_RDWR|O_WRONLY)) return ERR_INVALID_FILESYSTEM;
    memset(out,0,sizeof(*out)); out->s_flags=1; out->s_size=files[handles[(uint8_t)fd].file].size; return 0;
}
zos_err_t stat(const char *name,zos_stat_t *out) {
    int8_t fd;
    zos_err_t error; uint8_t i;
    if(!strcmp(name,"A:/")||!strcmp(name,"H:/")) { memset(out,0,sizeof(*out)); return 0; }
    for(i=0;i<64;++i) if(files[i].directory&&!strcmp(files[i].path,name)) {
        memset(out,0,sizeof(*out)); return 0;
    }
    fd=open(name,O_RDONLY);
    if(fd<0) return (zos_err_t)-fd;
    error=dstat(fd,out); close(fd); return error;
}
zos_err_t mkdir(const char *name) {
    uint8_t i;
    for(i=0;i<64;++i) if(!strcmp(files[i].path,name)) return ERR_ALREADY_EXIST;
    for(i=0;i<64;++i) if(!files[i].path[0]) {
        strcpy(files[i].path,name); files[i].size=0; files[i].directory=1; return 0;
    }
    return ERR_CANNOT_REGISTER_MORE;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    uint8_t n=(uint8_t)fd;
    uint32_t offset=(uint32_t)*at;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(handles[n].file==64) { ++model_serial_seek_calls; return ERR_NOT_SUPPORTED; }
    if(whence==SEEK_CUR) offset+=handles[n].position;
    if(whence==SEEK_END) offset+=handles[n].cached_size;
    if(offset>handles[n].cached_size) offset=handles[n].cached_size;
    handles[n].position=offset; *at=(int32_t)offset; return 0;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size) {
    uint8_t n=(uint8_t)fd,file=handles[n].file;
    uint32_t left;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(++model_read_calls==model_fail_read_at) return ERR_FAILURE;
    if(model_read_calls==model_zero_read_at) { *size=0; return 0; }
    if(model_oversize_read) { ++*size; return 0; }
    if(model_read_error) return (zos_err_t)model_read_error;
    if(handles[n].flags&O_WRONLY) return ERR_BAD_MODE;
    if(file==64) {
        left=model_serial_size-model_serial_position;
        if(*size>left) *size=(uint16_t)left;
        memcpy(out,model_serial_input+model_serial_position,*size);
        model_serial_position+=*size; return 0;
    }
    left=files[file].size-handles[n].position;
    if(*size>left) *size=(uint16_t)left;
    if(model_short&&*size>1) *size=1;
    memcpy(out,files[file].data+handles[n].position,*size); handles[n].position+=*size; return 0;
}
zos_err_t write(zos_dev_t fd,const void *in,uint16_t *size) {
    uint8_t n=(uint8_t)fd,file=handles[n].file;
    assert(fd>=2&&fd<16&&handles[n].used);
    if(++model_write_calls==model_fail_write_at) return ERR_FAILURE;
    if(model_oversize_write) { ++*size; return 0; }
    if(model_write_error) return (zos_err_t)model_write_error;
    if(!(handles[n].flags&(O_WRONLY|O_RDWR))) return ERR_BAD_MODE;
    if(model_zero_write) *size=0;
    if(model_short&&*size>1) *size=1;
    if(file==64) {
        assert(model_serial_attr&SERIAL_ATTR_MODE_RAW);
        assert(model_serial_written+*size<=sizeof(model_serial_output));
        memcpy(model_serial_output+model_serial_written,in,*size);
        model_serial_written+=*size; return 0;
    }
    assert(handles[n].position+*size<=sizeof(files[file].data));
    memcpy(files[file].data+handles[n].position,in,*size);
    if(model_corrupt_write==1&&*size) files[file].data[handles[n].position]^=1;
    handles[n].position+=*size;
    if(handles[n].position>files[file].size) files[file].size=handles[n].position;
    if(model_corrupt_write==2) {
        files[file].data[handles[n].position]=0xa5; files[file].size=handles[n].position+1;
    }
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
    if(model_rm_error) return (zos_err_t)model_rm_error;
    for(i=0;i<64;++i) if(!strcmp(files[i].path,name)) { files[i].path[0]=0; return 0; }
    return ERR_NO_SUCH_ENTRY;
}
