#include "runtime/process/overlay.h"
#include "runtime/process/chain.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>

static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint32_t wide(const uint8_t *p) {return word(p)|((uint32_t)word(p+2)<<16);}
static uint8_t zeros(const uint8_t *p,uint8_t n) {while(n--)if(*p++)return 0;return 1;}
static uint8_t group_valid(const OverlayGroup *g) {
    return g->group<100&&g->members&&g->bytes&&g->origin>=0x8000u&&
        (uint32_t)g->origin+g->bytes<=0xf400UL&&
        ((g->parent_group==65535u&&g->parent_member==65535u)||
         (g->parent_group<100&&g->parent_group!=g->group&&g->parent_member!=65535u));
}
uint8_t overlay_group_decode(const uint8_t *r,OverlayGroup *g) {
    memset(g,0,sizeof(*g));
    g->group=word(r);g->parent_group=word(r+2);g->parent_member=word(r+4);
    g->origin=word(r+6);g->bytes=word(r+8);g->members=word(r+10);
    return group_valid(g)&&zeros(r+12,36)?0:ERR_ENTRY_CORRUPTED;
}
static uint8_t fail(OverlayLoad *l,uint16_t error,uint8_t detail) {
    if(!l->error) {l->error=error;l->detail=detail;}
    return 0;
}
static uint8_t corrupt(OverlayLoad *l) {return fail(l,0x20,ERR_ENTRY_CORRUPTED);}
static uint8_t io_error(OverlayLoad *l,uint8_t error) {
    return fail(l,error==ERR_NO_SUCH_ENTRY?1:error==ERR_CANNOT_REGISTER_MORE?0xf3:
        error==ERR_READ_ONLY||error==ERR_BAD_MODE?2:error==ERR_NOT_A_FILE||
        error==ERR_NOT_SUPPORTED||error==ERR_NOT_IMPLEMENTED?0x20:0x99,error);
}
void overlay_init(OverlayLoad *l) {
    memset(l,0,sizeof(*l));memset(l->pages,255,sizeof(l->pages));
    memset(l->metadata,255,sizeof(l->metadata));l->fd=-1;
}
static uint8_t release(OverlayLoad *l,uint8_t *pages,uint8_t count) {
    uint8_t i,error,ok=1;
    for(i=0;i<count;++i)if(pages[i]!=255) {
        error=pfree(pages[i]);
        if(error) {fail(l,0x1ff,error);ok=0;}else pages[i]=255;
    }
    return ok;
}
static uint8_t shut(OverlayLoad *l) {
    uint8_t error;
    if(l->fd<0)return 1;
    error=close(l->fd);l->fd=-1; /* VFS revokes even a failed close. */
    return error?io_error(l,error):1;
}
uint8_t overlay_discard(OverlayLoad *l) {
    uint8_t ok;
    l->ready=0;ok=shut(l);
    if(!release(l,l->pages,2))ok=0;
    if(!release(l,l->metadata,32))ok=0;
    return ok;
}
/* All syscalls and page copies use the existing retained 128-byte transport.
 * No pointer into an MMU window survives a call or allocation. */
static uint8_t take(OverlayLoad *l,uint16_t bytes) {
    uint16_t n,done=0;uint8_t error;
    while(done<bytes) {
        n=bytes-done;error=read(l->fd,chain_buffer+done,&n);
        if(error)return io_error(l,error);
        if(!n||n>bytes-done)return corrupt(l);
        done+=n;
    }
    return 1;
}
static uint8_t acquire(OverlayLoad *l,uint8_t *page) {
    uint8_t error;
    if(*page!=255)return 1;
    error=palloc(page);
    if(error) {*page=255;return fail(l,0x1ff,error);}
    return 1;
}
/* A compact, temporary index of IDs and half-open code/map ranges lets us
 * check arbitrary index ordering without rereading mutable disk data. Entries
 * consume18 bytes, not96; there is no fixed compiler/member-count assumption. */
static uint8_t remember(OverlayLoad *l,uint16_t index,const uint8_t *r) {
    uint8_t i;uint32_t at=(uint32_t)index*18u;uint16_t n;
    for(i=0;i<18;++i,++at) {
        if(at>=32UL*16384u)return fail(l,0x1ff,ERR_NO_MORE_MEMORY);
        if(!acquire(l,l->metadata+(uint8_t)(at/16384u)))return 0;
        n=(uint16_t)(at%16384u);chain_buffer[0]=r[i];chain_store=1;
        chain_page_io(l->metadata[(uint8_t)(at/16384u)],n,1);
    }
    return 1;
}
static void recalled(OverlayLoad *l,uint16_t index,uint8_t *r) {
    uint8_t i;uint32_t at=(uint32_t)index*18u;
    for(i=0;i<18;++i,++at) {
        chain_store=0;chain_page_io(l->metadata[(uint8_t)(at/16384u)],(uint16_t)(at%16384u),1);
        r[i]=chain_buffer[0];
    }
}
static uint8_t extent(uint32_t start,uint32_t bytes,uint32_t base,uint32_t size) {
    return start>=base&&start<=size&&bytes&&bytes<=size-start;
}
static uint8_t overlap(uint32_t a,uint32_t z,uint32_t b,uint32_t y) {return a<y&&b<z;}
static void put_wide(uint8_t *p,uint32_t value) {
    uint8_t i;for(i=0;i<4;++i) {p[i]=(uint8_t)value;value>>=8;}
}
static uint8_t index_entry(OverlayLoad *l,const OverlayGroup *g,uint16_t index,
                          const uint8_t *r,uint32_t base,uint32_t size) {
    uint8_t record[18],previous[18];uint16_t i;
    uint32_t a=wide(r+4),z,b=wide(r+12),y;
    if(word(r)>=g->members||word(r+2)||!word(r+8)||word(r+8)>g->bytes||
       word(r+10)>=word(r+8)||wide(r+16)<96||!zeros(r+20,76)||
       !extent(a,word(r+8),base,size)||!extent(b,wide(r+16),base,size))return corrupt(l);
    z=a+word(r+8);y=b+wide(r+16);
    if(overlap(a,z,b,y))return corrupt(l);
    record[0]=r[0];record[1]=r[1];
    put_wide(record+2,a);put_wide(record+6,z);put_wide(record+10,b);put_wide(record+14,y);
    for(i=0;i<index;++i) {
        recalled(l,i,previous);
        if(word(previous)==word(r)||overlap(a,z,wide(previous+2),wide(previous+6))||
           overlap(a,z,wide(previous+10),wide(previous+14))||
           overlap(b,y,wide(previous+2),wide(previous+6))||
           overlap(b,y,wide(previous+10),wide(previous+14)))return corrupt(l);
    }
    return remember(l,index,record);
}
static uint8_t map_header(OverlayLoad *l,const uint8_t *h,uint32_t bytes) {
    uint32_t table=wide(h+84),entries=wide(h+16),entry_bytes=wide(h+88);
    if(memcmp(h,"ZMAP",4)||h[4]!=2||h[5]||word(h+6)!=96||word(h+8)!=1||
       word(h+10)!=l->origin||word(h+12)!=l->bytes||!word(h+14)||
       !zeros(h+20,64)||!zeros(h+92,4)||
       table<(uint32_t)word(h+14)*9u||entries>l->bytes||entry_bytes!=entries*16u||
       table>bytes-96u||entry_bytes!=bytes-96u-table)return corrupt(l);
    return 1;
}
static uint8_t prepare(OverlayLoad *l,const OverlayGroup *g,uint16_t member) {
    zos_stat_t info;
    uint8_t h[96],selected[96],map[96],error,found=0;
    uint16_t i,n,j,out;uint32_t at,size,base,code_at=0,map_at=0,map_bytes=0;
    error=dstat(l->fd,&info);if(error)return io_error(l,error);
    size=info.s_size;base=96UL+(uint32_t)g->members*96u;
    if(size<base)return corrupt(l);
    if(!take(l,96))return 0;
    memcpy(h,chain_buffer,96);
    if(memcmp(h,"ZOVR",4)||h[4]!=2||h[5]||word(h+6)!=96||word(h+8)!=1||
       word(h+10)!=g->group||word(h+12)!=g->origin||word(h+14)!=g->bytes||
       word(h+16)!=g->members||word(h+18)!=96||!zeros(h+20,64)||word(h+84)!=g->parent_group||
       word(h+86)!=g->parent_member||!zeros(h+88,8))return corrupt(l);
    for(i=0;i<g->members;++i) {
        if(!take(l,96))return 0;
        memcpy(h,chain_buffer,96);
        if(!index_entry(l,g,i,h,base,size))return 0;
        if(word(h)==member) {memcpy(selected,h,96);found=1;}
    }
    if(!found)return corrupt(l);
    code_at=wide(selected+4);map_at=wide(selected+12);map_bytes=wide(selected+16);
    l->origin=g->origin;l->bytes=word(selected+8);l->entry=l->origin+word(selected+10);l->member=member;
    /* Stream once after the index: all decisions, selected code and map bytes
     * come from one bounded pass through the opened file. */
    for(at=base;at<size;at+=n) {
        n=size-at>128u?128u:(uint16_t)(size-at);
        if(at<code_at&&code_at-at<n)n=(uint16_t)(code_at-at);
        if(at<map_at&&map_at-at<n)n=(uint16_t)(map_at-at);
        if(at>=code_at&&at-code_at<l->bytes) {
            out=(uint16_t)(at-code_at);
            if(n>l->bytes-out)n=l->bytes-out;
            if(n>16384u-out%16384u)n=16384u-out%16384u;
            if(!acquire(l,l->pages+out/16384u))return 0;
        }
        if(at>=map_at&&at-map_at<map_bytes&&map_bytes-(at-map_at)<n)n=(uint16_t)(map_bytes-(at-map_at));
        if(!take(l,n))return 0;
        if(at>=code_at&&at-code_at<l->bytes) {
            out=(uint16_t)(at-code_at);
            chain_store=1;chain_page_io(l->pages[out/16384u],out%16384u,n);
        }
        if(at>=map_at&&at-map_at<map_bytes) {
            if(at-map_at<96) {
                j=(uint16_t)(at-map_at);out=n;if(out>96-j)out=96-j;
                memcpy(map+j,chain_buffer,out);
            }
        }
    }
    n=1;error=read(l->fd,chain_buffer,&n);
    if(error)return io_error(l,error);
    if(n)return corrupt(l);
    return map_header(l,map,map_bytes);
}
uint8_t overlay_prepare(OverlayLoad *l,const char *path,const OverlayGroup *g,
                        uint16_t member) {
    uint8_t ok;
    /* Retry failed page cleanup before reinitializing an ownership ledger. */
    if(!overlay_discard(l))return 0;
    overlay_init(l);
    if(!group_valid(g)||member>=g->members)return corrupt(l);
    l->fd=open(path,O_RDONLY);
    if(l->fd<0) {io_error(l,(uint8_t)-l->fd);l->fd=-1;return 0;}
    ok=prepare(l,g,member);
    if(!shut(l))ok=0;
    if(!release(l,l->metadata,32))ok=0;
    if(!ok) {overlay_discard(l);return 0;}
    l->ready=1;return 1;
}
