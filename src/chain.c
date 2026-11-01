#include "chain.h"
#include "native_image.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>
#ifndef ZP_CHAIN_ONLY
#include "execute_handoff.h"
#endif

uint8_t chain_buffer[128],chain_store;
#ifdef ZP_CHAIN_ONLY
static uint8_t header[96];
#else
static uint8_t header[160];
#endif
static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint8_t fail(ImageLoad *load,uint16_t error,uint8_t detail) {
    if(!load->error) {load->error=error;load->detail=detail;}
    return 0;
}
static uint8_t corrupt(ImageLoad *load) {return fail(load,0x20,ERR_ENTRY_CORRUPTED);}
static uint8_t io_error(ImageLoad *load,uint8_t error) {
    return fail(load,error==ERR_NO_SUCH_ENTRY?1:error==ERR_CANNOT_REGISTER_MORE?0xf3:
        error==ERR_READ_ONLY||error==ERR_BAD_MODE?2:error==ERR_NOT_A_FILE||
        error==ERR_NOT_SUPPORTED||error==ERR_NOT_IMPLEMENTED?0x20:0x99,error);
}
void chain_init(ImageLoad *load) {
    memset(load,0,sizeof(*load));load->pages[0]=load->pages[1]=255;load->fd=-1;
#ifndef ZP_CHAIN_ONLY
    load->pages[2]=255;load->kind=IMAGE_CHAIN;
#endif
}
uint8_t chain_discard(ImageLoad *load) {
    uint8_t i,error,ok=1;
    load->ready=0;
    if(load->fd>=0) {
        error=close(load->fd);load->fd=-1; /* VFS revokes even a failed close. */
        if(error) {io_error(load,error);ok=0;}
    }
    for(i=0;i<IMAGE_LOAD_PAGES;++i)if(load->pages[i]!=255) {
        error=pfree(load->pages[i]);
        if(error) {fail(load,0x1ff,error);ok=0;}else load->pages[i]=255;
    }
    return ok;
}
/* Buffer belongs to the retained loader, including across page changes. */
static uint8_t take(ImageLoad *load,uint16_t bytes) {
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
static uint8_t heap_plan(ImageLoad *load,const ChainCaller *c) {
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
/* Both image formats check the declared extent, then retain only their
 * executable payload. Keep chunking, page boundaries, EOF and close
 * handling in one owner instead of maintaining two subtly different readers. */
static uint8_t stage(ImageLoad *load,uint8_t head_bytes,uint8_t payload) {
    uint16_t at,n,out,total;
    uint8_t error;
#ifdef ZP_CHAIN_ONLY
    total=96+load->bytes;
#else
    total=load->length;
#endif
    for(at=0;at<total;at+=n) {
        n=total-at;if(n>128)n=128;
        if(at<head_bytes) {
            if(n>head_bytes-at)n=head_bytes-at;
            memcpy(chain_buffer,header+at,n);
        } else {
            out=at-payload;
            if(n>16384u-out%16384u)n=16384u-out%16384u;
            if(!take(load,n))return 0;
        }
        if(at>=payload) {
            out=at-payload;
            if(load->pages[out/16384u]==255) {
                error=palloc(&load->pages[out/16384u]);
                if(error) {load->pages[out/16384u]=255;return fail(load,0x1ff,error);}
            }
            chain_store=1;chain_page_io(load->pages[out/16384u],out%16384u,n);
        }
    }
    n=1;error=read(load->fd,chain_buffer,&n);
    if(error)return io_error(load,error);
    if(n)return corrupt(load);
    error=close(load->fd);load->fd=-1;
    return error?io_error(load,error):1;
}
uint8_t chain_prepare(ImageLoad *load,const char *path,const ChainCaller *caller) {
    NativeImage id;
    uint8_t error;
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
#ifndef ZP_CHAIN_ONLY
    load->length=id.length;
#endif
    if(load->bytes<CHAIN_ACTIVATION_BYTES||load->origin!=caller->origin||load->end!=caller->end) {corrupt(load);goto failed;}
    if(!stage(load,96,96))goto failed;
    chain_store=0;chain_page_io(load->pages[0],CHAIN_ENTRY_BYTES,CHAIN_PROFILE_BYTES);
    if(memcmp(chain_buffer,caller->profile,CHAIN_PROFILE_BYTES)) {corrupt(load);goto failed;}
    if(!heap_plan(load,caller))goto failed;
    load->ready=1;return 1;
failed:
    chain_discard(load);return 0;
}
void chain_install(ImageLoad *load) {
    uint16_t at,n;
    if(!load->ready)return;
#ifndef ZP_CHAIN_ONLY
    if(load->kind!=IMAGE_CHAIN)return;
#endif
    for(at=0;at<load->bytes;at+=n) {
        n=load->bytes-at;if(n>128)n=128;
        chain_store=0;chain_page_io(load->pages[at/16384u],at%16384u,n);
        chain_user_copy(load->origin+at,n);
    }
    if(load->trim_at)chain_user_store(load->trim_at,load->trim_value);
    load->ready=0;
}
#ifndef ZP_CHAIN_ONLY
static uint8_t handoff_capable(ImageLoad *load) {
    uint16_t at,i,n,done=0;
    if(load->entry<0x8000u||execute_entry_length>load->length||
       load->entry-0x4000u>load->length-execute_entry_length)return 0;
    at=load->entry-0x4000u;
    while(done<execute_entry_length) {
        n=execute_entry_length-done;if(n>128)n=128;
        if(n>16384u-at%16384u)n=16384u-at%16384u;
        chain_store=0;chain_page_io(load->pages[at/16384u],at%16384u,n);
        for(i=0;i<n;++i)if(chain_buffer[i]!=execute_entry_start[done+i])return 0;
        done+=n;at+=n;
    }
    return 1;
}
uint8_t execute_prepare(ImageLoad *load,const char *path) {
    NativeImage id;
    uint16_t at,n;
    uint8_t error;
    if(!chain_discard(load))return 0;
    chain_init(load);load->kind=IMAGE_EXECUTABLE;
    load->fd=open(path,O_RDONLY);
    if(load->fd<0){error=(uint8_t)-load->fd;load->fd=-1;return io_error(load,error);}
    for(at=0;at<160;at+=n) {
        n=160-at;if(n>128)n=128;
        if(!take(load,n))goto failed;
        memcpy(header+at,chain_buffer,n);
    }
    error=native_decode(header,0,&id);
    if(error){fail(load,0x20,error);goto failed;}
    load->length=id.length;load->entry=word(header+12);load->origin=id.origin;load->bytes=id.bytes;
    load->globals=word(header+26);load->end=word(header+30);
    if(!stage(load,160,0))goto failed;
    load->handoff=handoff_capable(load);load->ready=1;return 1;
failed:
    chain_discard(load);return 0;
}
/* This component and its buffer/ledger live in the retained prefix. Neither
 * capture nor restore may alias it or the live stack. Validate the whole plan
 * before the first write; borrowed originals never enter the owned ledger. */
static uint8_t rollback_plan(ImageLoad *load,const uint8_t *originals,uint8_t count) {
    uint8_t i,j,code=execute_code_page(),stack=execute_stack_page();
    for(i=0;i<3;++i) {
        if(originals[i]<32||originals[i]>=64||originals[i]==code||originals[i]==stack)
            return corrupt(load);
        for(j=0;j<i;++j)if(originals[i]==originals[j])return corrupt(load);
    }
    for(i=0;i<count;++i) {
        if(load->pages[i]<32||load->pages[i]>=64||load->pages[i]==code||load->pages[i]==stack)
            return corrupt(load);
        for(j=0;j<i;++j)if(load->pages[i]==load->pages[j])return corrupt(load);
        for(j=0;j<3;++j)if(load->pages[i]==originals[j])return corrupt(load);
    }
    return 1;
}
static void rollback_copy(uint8_t from,uint8_t to) {
    uint16_t at;
    for(at=0;at<16384u;at+=sizeof(chain_buffer)) {
        chain_store=0;chain_page_io(from,at,sizeof(chain_buffer));
        chain_store=1;chain_page_io(to,at,sizeof(chain_buffer));
    }
}
uint8_t execute_snapshot(ImageLoad *load,const uint8_t originals[3]) {
    uint8_t i,error,count;
    if(!load->ready||load->kind!=IMAGE_EXECUTABLE||load->fd>=0||
       load->length<=16384u||load->length>46080u||
       (load->length>32768u&&load->pages[2]==255))return corrupt(load);
    count=load->pages[2]==255?2:3;
    if(!rollback_plan(load,originals,count))return 0;
    if(count==2) {
        error=palloc(&load->pages[2]);
        if(error) {load->pages[2]=255;return fail(load,0x1ff,error);}
        if(!rollback_plan(load,originals,3))return 0;
    }
    memcpy(load->originals,originals,3);
    for(i=0;i<3;++i)rollback_copy(load->originals[i],load->pages[i]);
    load->kind=IMAGE_ROLLBACK;return 1;
}
uint8_t execute_restore(ImageLoad *load) {
    uint8_t i;
    if(!load->ready||load->kind!=IMAGE_ROLLBACK||load->fd>=0)return corrupt(load);
    if(!rollback_plan(load,load->originals,3))return 0;
    for(i=0;i<3;++i)rollback_copy(load->pages[i],load->originals[i]);
    load->ready=0;return 1;
}
static ImageLoad *override_load;
static uint8_t override_originals[3];
static char override_path[128];
static uint8_t override_owners[EXECUTE_HANDOFF_MAX_PAGES-4],override_owner_count;
uint8_t execute_override(ImageLoad *load,const char *path,const uint8_t originals[3],
                         const uint8_t *owners,uint8_t owner_count) {
    uint16_t i;uint8_t j,page;
    if(!load->ready||load->kind!=IMAGE_EXECUTABLE||load->fd>=0||!load->handoff)return corrupt(load);
    /* Copy caller-owned arguments before the original stack is frozen. The
     * retained module must reserve its alias-stack area outside code/data. */
    for(i=0;i<sizeof(override_path);++i) {override_path[i]=path[i];if(!path[i])break;}
    if(!i||i==sizeof(override_path))return corrupt(load);
    if(owner_count>sizeof(override_owners)||(owner_count&&!owners))return corrupt(load);
    for(i=0;i<owner_count;++i) {
        page=owners[i];
        if(page<32||page>=64||page==execute_code_page())return corrupt(load);
        for(j=0;j<3;++j)if(page==originals[j]||page==load->pages[j])return corrupt(load);
        for(j=0;j<i;++j)if(page==owners[j])return corrupt(load);
        override_owners[i]=page;
    }
    override_owner_count=owner_count;
    override_load=load;memcpy(override_originals,originals,3);
    execute_switch();return 0;
}
/* Runs with both F1 and F3 mapped to the retained module, SP belowFC00. The
 * syscall dispatcher preserves these entry mappings on failure even while
 * the loader overwrites the OS's original allocation pages. */
void execute_attempt(void) {
    uint8_t error,record[EXECUTE_HANDOFF_BYTES],i;
    if(!execute_snapshot(override_load,override_originals))return;
    memset(record,0,sizeof(record));memcpy(record,"ZEX1",4);
    record[EXECUTE_HANDOFF_COUNT]=4+override_owner_count;
    memcpy(record+EXECUTE_HANDOFF_ORIGINALS,override_originals,3);
    record[EXECUTE_HANDOFF_LENGTH]=(uint8_t)override_load->length;
    record[EXECUTE_HANDOFF_LENGTH+1]=(uint8_t)(override_load->length>>8);
    record[EXECUTE_HANDOFF_PAGES]=execute_code_page();
    memcpy(record+EXECUTE_HANDOFF_PAGES+1,override_load->pages,3);
    memcpy(record+EXECUTE_HANDOFF_PAGES+4,override_owners,override_owner_count);
    for(i=0;i<sizeof(record)-1;++i)record[sizeof(record)-1]^=record[i];
    memcpy(chain_buffer,record,sizeof(record));chain_store=1;
    chain_page_io(override_originals[2],EXECUTE_HANDOFF_ADDRESS-0xc000u,sizeof(record));
    error=execute_os_override(override_path);
    io_error(override_load,error?error:ERR_FAILURE); /* override cannot return success */
    if(!execute_restore(override_load))execute_abandon(); /* never resume damaged RAM */
    memset(chain_buffer,0,sizeof(record));chain_store=1;
    chain_page_io(override_originals[2],EXECUTE_HANDOFF_ADDRESS-0xc000u,sizeof(record));
}
#endif
#ifndef ZP_CHAIN_HOST
uint16_t chain_user_word(uint16_t at) {return ((uint8_t*)at)[0]|((uint16_t)((uint8_t*)at)[1]<<8);}
void chain_user_store(uint16_t at,uint16_t value) {((uint8_t*)at)[0]=(uint8_t)value;((uint8_t*)at)[1]=(uint8_t)(value>>8);}
void chain_user_copy(uint16_t at,uint16_t bytes) {memcpy((void*)at,chain_buffer,bytes);}
#endif
