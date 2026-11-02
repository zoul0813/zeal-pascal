#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>

extern uint8_t console_scratch[256];
/* One synchronous publication owns keyboard polling. Recovery and cleanup
 * are noninterruptible; the reason remains available after the job ends. */
static uint8_t interruptible,interrupted;
void output_job_begin(uint8_t enabled) SOURCE_BLOCK_CALL {
    interruptible=enabled; interrupted=0;
}
void output_job_end(void) SOURCE_BLOCK_CALL { interruptible=0; }
uint8_t output_interrupt_reason(void) SOURCE_BLOCK_CALL { return interrupted; }
uint8_t output_poll(void) SOURCE_BLOCK_CALL {
    if(interrupted&&interruptible) return 0;
    if(interruptible) interrupted=key_interrupt();
    return !interruptible||!interrupted;
}

int8_t output_open_path(const char *path,uint8_t flags) SOURCE_BLOCK_CALL {
    int8_t fd;
    strcpy((char*)console_scratch,path);
    fd=open((char*)console_scratch,flags);
    if(fd<0) platform_error=(uint8_t)-fd;
    return fd;
}
uint8_t output_finish(int8_t fd,uint8_t ok) SOURCE_BLOCK_CALL {
    uint8_t prior=platform_error,e=close(fd);
    if(!ok) platform_error=prior?prior:e?e:(interruptible&&interrupted?0:ERR_FAILURE);
    else if(e) { platform_error=e; ok=0; }
    return ok;
}
static uint8_t output_write_all(int8_t fd,uint16_t n) {
    uint16_t at=0,size;
    while(at<n) {
        size=n-at; platform_error=write(fd,console_scratch+at,&size);
        if(platform_error) return 0;
        if(!size||size>n-at) { platform_error=ERR_FAILURE; return 0; }
        at+=size;
    }
    return 1;
}
uint8_t output_write_buffer(int8_t fd,const uint8_t *buffer,uint16_t size) SOURCE_BLOCK_CALL {
    if(size>128) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    if(!output_poll()) return 0;
    memcpy(console_scratch,buffer,size);
    return output_write_all(fd,size);
}
uint8_t output_read_buffer(int8_t fd,uint8_t *buffer,uint16_t size) SOURCE_BLOCK_CALL {
    uint16_t at=0,n;
    if(size>128) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    while(at<size) {
        if(!output_poll()) return 0;
        n=size-at; platform_error=read(fd,console_scratch,&n);
        if(platform_error) return 0;
        if(!n||n>size-at) { platform_error=ERR_FAILURE; return 0; }
        memcpy(buffer+at,console_scratch,n); at+=n;
    }
    return 1;
}
static uint8_t stream_byte(const uint8_t *pages,uint16_t p) {
    return far_get(pages[p/PAGE_SIZE],p%PAGE_SIZE);
}
uint8_t output_store_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL {
    uint16_t p=0,n,i;
    int8_t fd=output_open_path(path,O_WRONLY|O_CREAT|O_TRUNC);
    uint8_t ok=1;
    if(fd<0) return 0;
    while(p<total&&ok) {
        if(!output_poll()) { ok=0; break; }
        n=total-p; if(n>128) n=128;
        for(i=0;i<n;++i) console_scratch[i]=stream_byte(pages,first+p+i);
        ok=output_write_all(fd,n); p+=n;
    }
    return output_finish(fd,ok);
}
uint8_t output_store_source_slice(const Source *s,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL {
    uint16_t p=0,n,i;uint8_t ok=1;
    int8_t fd=output_open_path(path,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    while(p<total&&ok) {
        n=total-p;if(n>128)n=128;
        for(i=0;i<n;++i)console_scratch[i]=buffer_get(s->page,first+p+i);
        ok=output_poll()&&output_write_all(fd,n);p+=n;
    }
    return output_finish(fd,ok);
}
/* Save the canonical source stream directly from suspended Work. Keep its
 * trailing-space rule without materializing a second16KiB source page. */
uint8_t output_store_source(const Source *s,const char *path) SOURCE_BLOCK_CALL {
    uint16_t a=0,z,trim,i,n=0;
    uint8_t ok=1;
    int8_t fd=output_open_path(path,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    while(a<s->length&&ok) {
        if(!output_poll()) {ok=0;break;}
        z=a;while(z<s->length&&buffer_get(s->page,z)!='\n')++z;
        trim=z;while(trim>a&&buffer_get(s->page,trim-1)==' ')--trim;
        for(i=a;i<trim&&ok;++i) {
            console_scratch[n++]=buffer_get(s->page,i);
            if(n==128) {ok=output_poll()&&output_write_all(fd,n);n=0;}
        }
        if(z<s->length&&ok) {
            console_scratch[n++]='\n';
            if(n==128) {ok=output_poll()&&output_write_all(fd,n);n=0;}
        }
        a=z+1;
    }
    if(ok&&n)ok=output_poll()&&output_write_all(fd,n);
    return output_finish(fd,ok);
}
uint8_t output_copy_file(const char *from,const char *to) SOURCE_BLOCK_CALL {
    int8_t input=output_open_path(from,O_RDONLY),out;
    uint16_t n;
    uint8_t ok=1;
    if(input<0) return 0;
    out=output_open_path(to,O_WRONLY|O_CREAT|O_TRUNC);
    if(out<0) { output_finish(input,0); return 0; }
    for(;;) {
        if(!output_poll()) { ok=0; break; }
        n=128; platform_error=read(input,console_scratch,&n);
        if(platform_error||n>128) { ok=0; break; }
        if(!n) break;
        if(!output_write_all(out,n)) { ok=0; break; }
    }
    ok=output_finish(input,ok); return output_finish(out,ok);
}
