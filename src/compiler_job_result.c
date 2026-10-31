#include "compiler_job.h"
#include "image.h"
#include "saved_map.h"

static void take(uint8_t page,uint16_t at,uint8_t *out,uint16_t n) {while(n--)*out++=far_get(page,at++);}
static uint16_t far_word(uint8_t page,uint16_t at) {return far_get(page,at)|((uint16_t)far_get(page,at+1)<<8);}
static uint32_t far_long(uint8_t page,uint16_t at) {return (uint32_t)far_word(page,at)|((uint32_t)far_word(page,at+2)<<16);}
uint8_t job_map_source(const Build *b,uint16_t at,Source *s,uint8_t hash[32]) SOURCE_BLOCK_CALL {
    char canonical[128];uint16_t n=far_word(b->map_page,at+2);
    if(!n||n>127||(uint32_t)at+40+n>b->map_length||far_long(b->map_page,at+4)>SOURCE_LIMIT)return 0;
    memset(s,0,sizeof(*s));s->page=255;s->length=(uint16_t)far_long(b->map_page,at+4);
    take(b->map_page,at+8,hash,32);take(b->map_page,at+40,(uint8_t*)s->path,n);s->path[n]=0;
    return strlen(s->path)==n&&sources_canonical(s->path,canonical)&&!strcmp(s->path,canonical);
}
static uint8_t validate_map(const Build *b,const NativeImage *image,const uint8_t *job,uint16_t count) {
    uint8_t h[96],digest[32],hash[32];uint16_t offsets[JOB_SOURCE_COUNT],at=96,i,j,n,file,previous=0,first,last;
    uint32_t table,entries;Source s,prior;JobSource input;Sha256 manifest;
    take(b->map_page,0,h,96);table=job_long(h+84);entries=job_long(h+16);
    if(memcmp(h,"ZMAP",4)||h[4]!=1||h[5]||job_word(h+6)!=96||job_word(h+8)!=1||
       job_word(h+10)!=image->origin||job_word(h+12)!=image->bytes||!count||count>JOB_SOURCE_COUNT||job_word(h+14)!=count||
       memcmp(h+20,image->id,32)||(!image->chain&&memcmp(h+52,image->manifest,32))||
       !job_zero(h+92,4)||table>b->map_length-96u||entries>image->bytes||job_long(h+88)!=entries*16||
       96UL+table+entries*16!=b->map_length)return 0;
    memset(offsets,0,sizeof(offsets));sha_init(&manifest);
    for(i=0;i<table;++i)sha_byte(&manifest,far_get(b->map_page,96+i));
    sha_end(&manifest,digest);if(memcmp(digest,h+52,32))return 0;
    for(i=0;i<count;++i) {
        if((uint32_t)at+40>96+table)return 0;
        file=far_word(b->map_page,at);n=far_word(b->map_page,at+2);
        if(file>=count||offsets[file]||!job_map_source(b,at,&s,hash)||(uint32_t)at+40+n>96+table)return 0;
        if(!file) {
            if(!job_find_source(job,job_word(job+14),&input)||strcmp(s.path,input.source.path))return 0;
        }
        if(job_find_path(job,s.path,&input)&&input.resident&&
           (s.length!=input.source.length||memcmp(hash,input.hash,32)))return 0;
        for(j=0;j<count;++j)if(offsets[j]) {
            if(!job_map_source(b,offsets[j],&prior,digest)||!strcmp(prior.path,s.path))return 0;
        }
        offsets[file]=at;at+=40+n;
    }
    if(at!=96+table)return 0;
    for(i=0;i<count;++i)if(!offsets[i])return 0;
    for(i=0;i<entries;++i,at+=16) {
        first=far_word(b->map_page,at);last=far_word(b->map_page,at+2);file=far_word(b->map_page,at+4);
        if(first<previous||first>=last||last>image->bytes||file>=count||far_word(b->map_page,at+6)||
           far_long(b->map_page,at+8)>far_long(b->map_page,at+12)||
           far_long(b->map_page,at+12)>far_long(b->map_page,offsets[file]+4))return 0;
        previous=last;
    }
    return at==b->map_length;
}
uint8_t job_validate_success(const uint8_t *job,const uint8_t *c,Build *b,uint16_t *files) SOURCE_BLOCK_CALL {
    uint8_t result[128],h[160],digest[32],record[48],i,j,kind=job[5]==2;
    uint16_t at=0,n;JobStream image,map,bundle,part;NativeImage identity;JobSource root;char paths[2][128],path[128];
    take(c[76],job_word(c+77),result,128);
    if(page_role(c[76])!=PAGE_WORKER||memcmp(result,"ZRES",4)||result[4]!=1||result[5]!=kind||job_word(result+6)!=128||
       memcmp(result+8,job+28,32)||!job_zero(result+98,30)||!result[72]||result[72]>3||
       !job_zero(result+73+result[72],3-result[72])||job_word(result+96)!=2)return 0;
    image.length=job_word(result+76);image.count=result[72];memcpy(image.pages,result+73,image.count);
    if(image.length<96||image.length>48128u||image.count!=(image.length+PAGE_SIZE-1)/PAGE_SIZE)return 0;
    for(i=0;i<image.count;++i) {
        if(!job_page(compiler_owned,image.pages[i])||page_role(image.pages[i])!=PAGE_IMAGE)return 0;
        for(j=0;j<i;++j)if(image.pages[i]==image.pages[j])return 0;
    }
    if(!job_read_stream(result+78,compiler_owned,&map)||map.count!=1||map.length<96||map.length>PAGE_SIZE||
       job_long(result+81)!=map.length||page_role(map.pages[0])!=PAGE_MAP||
       !job_read_stream(result+93,compiler_owned,&bundle)||bundle.count!=1||page_role(bundle.pages[0])!=PAGE_WORKER)return 0;
    take(image.pages[0],0,h,kind?96:160);
    if(!kind&&job_word(h+18))return 0;
    if(native_header(h,kind,&identity)||identity.length!=image.length||memcmp(identity.id,result+40,32)||
       identity.bytes!=job_word(result+87)||job_word(h+(kind?16:26))!=job_word(result+89)||
       job_word(h+(kind?18:30))!=job_word(job+20)||(job_word(job+18)&&identity.origin!=job_word(job+18))||
       job_word(result+91)!=job_word(job+20)+1u-((identity.origin+identity.bytes+1u)&~1u))return 0;
    if(!job_hash_stream(&image,kind?20:36,32,digest)||memcmp(digest,identity.id,32))return 0;
    if(!job_hash_stream(&map,20,32,digest)||memcmp(digest,identity.map,32))return 0;
    if(!job_find_source(job,job_word(job+14),&root)||!saved_paths_kind(root.source.path,paths,kind))return 0;
    for(i=0;i<2;++i) {
        if(job[5]&&job_find_path(job,paths[i],&root))return 0;
        if(bundle.length-at<48)return 0;
        take(bundle.pages[0],at,record,48);at+=48;n=job_word(record+2);
        if(record[0]!=(i?2:kind)||record[1]||!n||n>127||job_word(record+4)||job_word(record+6)||record[47]||
           n>bundle.length-at||!job_read_stream(record+44,compiler_owned,&part))return 0;
        take(bundle.pages[0],at,(uint8_t*)path,n);path[n]=0;at+=n;
        if(strlen(path)!=n||strcmp(path,paths[i])||job_long(record+8)!=part.length||
           part.length!=(i?map.length:image.length)||part.count!=(i?map.count:image.count))return 0;
        for(j=0;j<part.count;++j)if(part.pages[j]!=(i?map.pages[j]:image.pages[j]))return 0;
        if(!job_hash_stream(&part,0,0,digest)||memcmp(digest,record+12,32))return 0;
    }
    if(at!=bundle.length)return 0;
    memset(b,0,sizeof(*b));memset(b->pages,255,3);b->sources.page=255;
    memcpy(b->pages,image.pages,image.count);b->map_page=map.pages[0];b->length=(uint16_t)image.length;
    b->map_length=(uint16_t)map.length;b->code_origin=identity.origin;b->code_bytes=identity.bytes;
    b->globals_bytes=job_word(result+89);memcpy(b->id,identity.id,32);*files=job_word(result+85);
    return validate_map(b,&identity,job,*files);
}
