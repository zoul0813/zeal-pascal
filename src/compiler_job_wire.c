#include "compiler_job_wire.h"

uint16_t job_word(const uint8_t *p) SOURCE_BLOCK_CALL {return p[0]|((uint16_t)p[1]<<8);}
uint32_t job_long(const uint8_t *p) SOURCE_BLOCK_CALL {return (uint32_t)job_word(p)|((uint32_t)job_word(p+2)<<16);}
void job_put16(uint8_t *p,uint16_t v) SOURCE_BLOCK_CALL {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
void job_put32(uint8_t *p,uint32_t v) SOURCE_BLOCK_CALL {job_put16(p,(uint16_t)v);job_put16(p+2,(uint16_t)(v>>16));}
uint8_t job_zero(const uint8_t *p,uint16_t n) SOURCE_BLOCK_CALL {while(n--)if(*p++)return 0;return 1;}
uint8_t job_page(const uint8_t allowed[8],uint8_t p) SOURCE_BLOCK_CALL {
    return p<64&&(allowed[p>>3]&(1u<<(p&7)))&&page_lifetime(p);
}
uint8_t job_handle(const uint8_t *h,uint16_t n,const uint8_t allowed[8]) SOURCE_BLOCK_CALL {
    uint16_t at=job_word(h+1);
    return job_page(allowed,h[0])&&at<PAGE_SIZE&&n<=PAGE_SIZE-at;
}
static void take(uint8_t page,uint16_t at,uint8_t *out,uint16_t n) {
    while(n--)*out++=far_get(page,at++);
}
static uint8_t canonical(const char *path,uint16_t n) {
    char normalized[128];
    return n&&n<=127&&strlen(path)==n&&sources_canonical(path,normalized)&&!strcmp(path,normalized);
}
uint8_t job_read_source(uint8_t page,uint16_t *at,uint16_t end,const uint8_t allowed[8],JobSource *r) SOURCE_BLOCK_CALL {
    uint8_t h[44],count;uint16_t n,position=*at;uint32_t bytes;
    if(end>PAGE_SIZE||position>end||end-position<44)return 0;
    take(page,position,h,44);n=job_word(h+40);count=h[3];bytes=job_long(h+4);
    if(job_word(h)==65535u||h[2]>1||!job_zero(h+42,2)||!n||n>127||
       (uint32_t)44+n+count>end-position)return 0;
    memset(r,0,sizeof(*r));r->id=job_word(h);r->resident=h[2];r->source.page=255;
    memcpy(r->hash,h+8,32);take(page,position+44,(uint8_t*)r->source.path,n);r->source.path[n]=0;
    if(!canonical(r->source.path,n))return 0;
    if(!r->resident) {
        if(count||bytes!=0xffffffffUL||!job_zero(r->hash,32))return 0;
    } else {
        if(bytes>SOURCE_LIMIT||count!=(bytes?1:0))return 0;
        r->source.length=(uint16_t)bytes;
        if(count) {
            r->source.page=far_get(page,position+44+n);
            if(!job_page(allowed,r->source.page)||r->source.page==page||
               (page_role(r->source.page)!=PAGE_SOURCE&&page_role(r->source.page)!=PAGE_SNAPSHOT))return 0;
        }
    }
    *at=position+44+n+count;return 1;
}
uint8_t job_write_source(uint8_t page,uint16_t *at,const JobSource *r) SOURCE_BLOCK_CALL {
    uint8_t h[44],i,count=r->resident&&r->source.length?1:0;
    uint16_t n=(uint16_t)strlen(r->source.path),position=*at;
    if(!canonical(r->source.path,n)||r->id==65535u||r->resident>1||
       r->source.length>SOURCE_LIMIT||position>PAGE_SIZE||44u+n+count>PAGE_SIZE-position)return 0;
    memset(h,0,sizeof(h));job_put16(h,r->id);h[2]=r->resident;h[3]=count;
    job_put32(h+4,r->resident?r->source.length:0xffffffffUL);job_put16(h+40,n);
    if(r->resident)memcpy(h+8,r->hash,32);
    for(i=0;i<44;++i)far_set(page,position++,h[i]);
    for(i=0;i<n;++i)far_set(page,position++,(uint8_t)r->source.path[i]);
    if(count)far_set(page,position++,r->source.page);
    *at=position;return 1;
}
/* Capture and authentication share canonical-byte validation, but callers
 * still perform their own independent scan at each trust boundary. The digest
 * is usable only when the complete source is canonical. */
uint8_t job_source_digest(const Source *source,uint8_t digest[32]) SOURCE_BLOCK_CALL {
    Sha256 hash;uint8_t c,previous=0;uint16_t i,line=0;
    sha_init(&hash);
    for(i=0;i<source->length;++i) {
        c=far_get(source->page,i);
        if(c=='\r'||c==26)return 0;
        if(c=='\n') {if(previous==' ')return 0;line=0;}
        else if(++line>127)return 0;
        sha_byte(&hash,c);previous=c;
    }
    if(previous==' ')return 0;
    sha_end(&hash,digest);return 1;
}
uint8_t job_source_hash(const JobSource *r) SOURCE_BLOCK_CALL {
    uint8_t digest[32];
    if(!r->resident)return 1;
    return job_source_digest(&r->source,digest)&&!memcmp(digest,r->hash,32);
}
void job_identity(const uint8_t job[128],uint8_t digest[32]) SOURCE_BLOCK_CALL {
    Sha256 hash;uint16_t i,base=job_word(job+9),bytes=job_word(job+12);
    sha_init(&hash);
    for(i=0;i<128;++i)sha_byte(&hash,i>=28&&i<60?0:job[i]);
    for(i=0;i<bytes;++i)sha_byte(&hash,far_get(job[8],base+i));
    sha_end(&hash,digest);
}
static uint8_t retained(const uint8_t *p) {
#ifdef __SDCC
    uint16_t at=(uint16_t)p;
    /* The banked IDE retains its own entire page3 stack. FC00 is reserved
     * only in a RAM child's separate mapping, not in this parent's page. */
    return (at>=0x4000u&&at<=0x7f80u)||(at>=0xc000u&&at<=0xff80u);
#else
    return p!=0;
#endif
}
uint8_t job_validate(const uint8_t context[128],const uint8_t job[128],const uint8_t allowed[8]) SOURCE_BLOCK_CALL {
    uint16_t at,base,end,count,i,j,prior,found=0;uint32_t total=0;
    JobSource r,other;uint8_t digest[32],handle[3];char path[128];
    if(!retained(context)||!retained(job)||memcmp(context,"ZRAM",4)||context[4]!=1||context[5]!=1||
       job_word(context+6)!=128||!job_zero(context+8,14)||job_word(context+22)!=(uint16_t)(uintptr_t)job||
       job_word(context+24)!=128||!job_zero(context+26,8)||job_word(context+34)!=65535u||
       !job_zero(context+36,92)||memcmp(job,"ZCMP",4)||job[4]!=1||job[5]>2||job_word(job+6)!=128||
       job[11]||!job_zero(job+67,61)||job_word(job+60)!=1||job_word(job+20)<0xcfffu||job_word(job+20)>0xfbffu||
       (job_word(job+18)&&(job_word(job+18)<0x8000u||job_word(job+18)>0xf400u)))return 0;
    count=job_word(job+22);base=job_word(job+9);
    if(!count||count>JOB_SOURCE_COUNT||!job_handle(job+8,job_word(job+12),allowed)||!job_word(job+12))return 0;
    end=base+job_word(job+12);at=base;
    for(i=0;i<count;++i) {
        prior=base;
        if(!job_read_source(job[8],&at,end,allowed,&r)||!job_source_hash(&r))return 0;
        if(r.id==job_word(job+14))found|=1;
        if(r.id==job_word(job+16))found|=2;
        if(r.resident)total+=r.source.length;
        for(j=0;j<i;++j) {
            if(!job_read_source(job[8],&prior,end,allowed,&other)||other.id==r.id||
               !strcmp(other.source.path,r.source.path)||
               (r.source.page!=255&&r.source.page==other.source.page))return 0;
        }
    }
    if(at!=end||found!=3||total!=job_long(job+24))return 0;
    if(!job[5]) {if(!job_zero(job+62,5))return 0;}
    else {
        uint16_t n=job_word(job+65);
        memcpy(handle,job+62,3);
        if(!n||n>127||!job_handle(handle,n,allowed))return 0;
        take(handle[0],job_word(handle+1),(uint8_t*)path,n);path[n]=0;
        if(!canonical(path,n))return 0;
    }
    job_identity(job,digest);return !memcmp(digest,job+28,32);
}
static uint8_t find(const uint8_t job[128],uint16_t id,const char *path,JobSource *r) {
    uint8_t allowed[8];uint16_t at=job_word(job+9),end=at+job_word(job+12),i;
    memset(allowed,255,8); /* The enclosing job has already been authenticated. */
    for(i=0;i<job_word(job+22);++i) {
        if(!job_read_source(job[8],&at,end,allowed,r))return 0;
        if(path?!strcmp(path,r->source.path):r->id==id)return 1;
    }
    return 0;
}
uint8_t job_find_source(const uint8_t job[128],uint16_t id,JobSource *r) SOURCE_BLOCK_CALL {return find(job,id,0,r);}
uint8_t job_find_path(const uint8_t job[128],const char *path,JobSource *r) SOURCE_BLOCK_CALL {return find(job,0,path,r);}
uint8_t job_read_stream(const uint8_t *h,const uint8_t allowed[8],JobStream *s) SOURCE_BLOCK_CALL {
    uint8_t header[8],i,j;uint16_t at=job_word(h+1);
    if(!job_handle(h,8,allowed))return 0;
    take(h[0],at,header,8);s->length=job_long(header);s->count=header[4];
    if(s->length>3UL*PAGE_SIZE||s->count!=(s->length?(s->length+PAGE_SIZE-1)/PAGE_SIZE:0)||
       header[5]||!job_zero(header+6,2)||!job_handle(h,8+s->count,allowed))return 0;
    memset(s->pages,255,3);
    for(i=0;i<s->count;++i) {
        s->pages[i]=far_get(h[0],at+8+i);
        if(!job_page(allowed,s->pages[i]))return 0;
        for(j=0;j<i;++j)if(s->pages[i]==s->pages[j])return 0;
    }
    return 1;
}
