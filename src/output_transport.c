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
uint8_t output_verify_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL {
    uint16_t p=0,n,i,requested;
    int8_t fd=output_open_path(path,O_RDONLY);
    uint8_t ok=1;
    if(fd<0) return 0;
    while(p<total&&ok) {
        if(!output_poll()) { ok=0; break; }
        requested=total-p; if(requested>128) requested=128; n=requested;
        platform_error=read(fd,console_scratch,&n);
        if(platform_error) { ok=0; break; }
        if(!n||n>requested) { ok=0; break; }
        for(i=0;i<n;++i) if(console_scratch[i]!=stream_byte(pages,first+p+i)) ok=0;
        p+=n;
    }
    if(ok) { n=1; platform_error=read(fd,console_scratch,&n); if(platform_error||n) ok=0; }
    if(!ok&&!platform_error&&!(interruptible&&interrupted)) platform_error=ERR_FAILURE;
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
uint8_t output_same_files(const char *a,const char *b) SOURCE_BLOCK_CALL {
    int8_t first=output_open_path(a,O_RDONLY),second;
    uint16_t n[2]={0,0},at[2]={0,0},count,i;
    uint8_t j,ok=1;
    if(first<0) return 0;
    second=output_open_path(b,O_RDONLY);
    if(second<0) { output_finish(first,0); return 0; }
    for(;;) {
        if(!output_poll()) { ok=0; break; }
        for(j=0;j<2;++j) if(at[j]==n[j]) {
            n[j]=128; at[j]=0;
            platform_error=read(j?second:first,console_scratch+j*128,&n[j]);
            if(platform_error||n[j]>128) { ok=0; break; }
        }
        if(!ok||!n[0]||!n[1]) { if(n[0]!=n[1]) ok=0; break; }
        count=n[0]-at[0]; if(count>n[1]-at[1]) count=n[1]-at[1];
        for(i=0;i<count;++i) if(console_scratch[at[0]+i]!=console_scratch[128+at[1]+i]) ok=0;
        if(!ok) break;
        at[0]+=count; at[1]+=count;
    }
    if(!ok&&!platform_error&&!(interruptible&&interrupted)) platform_error=ERR_FAILURE;
    ok=output_finish(first,ok); return output_finish(second,ok);
}
