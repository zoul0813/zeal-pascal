#include "chain.h"
#include "native_image.h"
#include "sha256.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>

uint8_t chain_buffer[128],chain_store;
static uint8_t header[96];
static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint8_t fail(ChainLoad *load,uint16_t error,uint8_t detail) {
    if(!load->error) {load->error=error;load->detail=detail;}
    return 0;
}
static uint8_t corrupt(ChainLoad *load) {return fail(load,0x20,ERR_ENTRY_CORRUPTED);}
static uint8_t io_error(ChainLoad *load,uint8_t error) {
    return fail(load,error==ERR_NO_SUCH_ENTRY?1:error==ERR_CANNOT_REGISTER_MORE?0xf3:
        error==ERR_READ_ONLY||error==ERR_BAD_MODE?2:error==ERR_NOT_A_FILE||
        error==ERR_NOT_SUPPORTED||error==ERR_NOT_IMPLEMENTED?0x20:0x99,error);
}
void chain_init(ChainLoad *load) {
    memset(load,0,sizeof(*load));load->pages[0]=load->pages[1]=255;load->fd=-1;
}
uint8_t chain_discard(ChainLoad *load) {
    uint8_t i,error,ok=1;
    load->ready=0;
    if(load->fd>=0) {
        error=close(load->fd);load->fd=-1; /* VFS revokes even a failed close. */
        if(error) {io_error(load,error);ok=0;}
    }
    for(i=0;i<2;++i)if(load->pages[i]!=255) {
        error=pfree(load->pages[i]);
        if(error) {fail(load,0x1ff,error);ok=0;}else load->pages[i]=255;
    }
    return ok;
}
/* Buffer belongs to the retained loader, including across page changes. */
static uint8_t take(ChainLoad *load,uint16_t bytes) {
    uint16_t n,done=0;uint8_t error;
    while(done<bytes) {
        n=bytes-done;error=read(load->fd,chain_buffer+done,&n);
        if(error)return io_error(load,error);
        if(!n||n>bytes-done)return corrupt(load);
        done+=n;
    }
    return 1;
}
static uint8_t caller_valid(const ChainCaller *c) {
    uint32_t extent=(uint32_t)c->origin+c->bytes;
    uint16_t aligned;
    if(!c->bytes||c->origin<0x8000u||c->globals>c->origin-0x8000u)return 0;
    if(extent>0xf400UL||c->end<0xcfffu||c->end>0xfbffu)return 0;
    /* Narrow only after the widened extent check. Keep alignment and stack
     * comparisons in their proven16-bit domain; this also avoids SDCC4.4's
     * erroneous spill of the high byte in a compound widened expression. */
    aligned=((uint16_t)extent+1u)&0xfffeu;
    if((c->heap_floor&1)||(c->heap_frontier&1)||c->heap_floor<aligned)return 0;
    if(c->heap_frontier<c->heap_floor)return 0;
    if(c->stack_pointer<0xc840u||c->stack_pointer>c->end+1u)return 0;
    return c->heap_frontier<=c->stack_pointer-64u;
}
static uint8_t heap_plan(ChainLoad *load,const ChainCaller *c) {
    uint16_t at=c->heap_floor,next,header,span;
    load->floor=(load->origin+load->bytes+1u)&0xfffeu;
    if(load->floor<c->heap_floor)load->floor=c->heap_floor;
    load->frontier=load->floor>c->heap_frontier?load->floor:c->heap_frontier;
    load->trim_at=0;
    /* Scan all blocks, including those after a candidate overlap/free block.
     * The inline list is also the allocator's free list; no detached pointer
     * can survive this adjustment and later allocate replacement code. */
    while(at<c->heap_frontier) {
        if(c->heap_frontier-at<2)return corrupt(load);
        header=chain_user_word(at);span=header&0x7fffu;
        if((span&1)||span>c->heap_frontier-at-2)return corrupt(load);
        next=at+2+span;
        if(!(header&0x8000u)&&at<load->floor)return corrupt(load);
        if((header&0x8000u)&&at<load->floor&&next>load->floor) {
            load->trim_at=load->floor;load->trim_value=0x8000u|(next-load->floor-2);
        }
        at=next;
    }
    return 1;
}
uint8_t chain_prepare(ChainLoad *load,const char *path,const ChainCaller *caller) {
    NativeImage id;Sha256 hash;
    uint16_t at,n,i;uint8_t error,digest[32];
    if(!chain_discard(load))return 0;
    chain_init(load);
    if(!caller_valid(caller))return corrupt(load);
    load->fd=open(path,O_RDONLY);
    if(load->fd<0) {error=(uint8_t)-load->fd;load->fd=-1;return io_error(load,error);}
    if(!take(load,96))goto failed;
    memcpy(header,chain_buffer,96);
    error=native_decode(header,1,&id);
    if(error) {fail(load,0x20,error);goto failed;}
    load->origin=id.origin;load->bytes=id.bytes;load->globals=word(header+16);load->end=word(header+18);
    if(load->origin!=caller->origin||load->end!=caller->end) {corrupt(load);goto failed;}
    memcpy(load->id,id.id,32);memcpy(load->map,id.map,32);
    sha_init(&hash);
    for(i=0;i<96;++i)sha_byte(&hash,i>=20&&i<52?0:header[i]);
    for(at=0;at<load->bytes;at+=n) {
        n=load->bytes-at;if(n>128)n=128;
        if(!take(load,n))goto failed;
        for(i=0;i<n;++i)sha_byte(&hash,chain_buffer[i]);
        if(load->pages[at/16384u]==255) {
            error=palloc(&load->pages[at/16384u]);
            if(error) {load->pages[at/16384u]=255;fail(load,0x1ff,error);goto failed;}
        }
        chain_store=1;chain_page_io(load->pages[at/16384u],at%16384u,n);
    }
    n=1;error=read(load->fd,chain_buffer,&n);
    if(error) {io_error(load,error);goto failed;}
    if(n) {corrupt(load);goto failed;}
    sha_end(&hash,digest);
    if(memcmp(digest,load->id,32)) {corrupt(load);goto failed;}
    error=close(load->fd);load->fd=-1;
    if(error) {io_error(load,error);goto failed;}
    if(!heap_plan(load,caller))goto failed;
    load->ready=1;return 1;
failed:
    chain_discard(load);return 0;
}
void chain_install(ChainLoad *load) {
    uint16_t at,n;
    if(!load->ready)return;
    for(at=0;at<load->bytes;at+=n) {
        n=load->bytes-at;if(n>128)n=128;
        chain_store=0;chain_page_io(load->pages[at/16384u],at%16384u,n);
        chain_user_copy(load->origin+at,n);
    }
    if(load->trim_at)chain_user_store(load->trim_at,load->trim_value);
    load->ready=0;
}
#ifndef ZP_CHAIN_HOST
uint16_t chain_user_word(uint16_t at) {return ((uint8_t*)at)[0]|((uint16_t)((uint8_t*)at)[1]<<8);}
void chain_user_store(uint16_t at,uint16_t value) {((uint8_t*)at)[0]=(uint8_t)value;((uint8_t*)at)[1]=(uint8_t)(value>>8);}
void chain_user_copy(uint16_t at,uint16_t bytes) {memcpy((void*)at,chain_buffer,bytes);}
#endif
