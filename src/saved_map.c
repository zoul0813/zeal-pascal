#include "saved_map.h"
#include "image.h"
#include <zos_vfs.h>
#include <zos_errors.h>

extern uint8_t console_scratch[256];

typedef struct {
    int8_t fd;
    uint32_t position;
} Reader;

/* Only this retained handle consumes C data. The first temporary page keeps
 * its own ownership ledger, including a source page whose release failed.
 * Retry that cleanup before another job; never forget failed pfree ownership. */
static uint8_t pending=255;
enum { META=64,RECORD=136,PATH=4,OFFSET=132,SOURCE_PAGE=33 };

static uint16_t word(const uint8_t *p) { return p[0]|((uint16_t)p[1]<<8); }
static uint32_t longword(const uint8_t *p) {
    return (uint32_t)word(p)|((uint32_t)word(p+2)<<16);
}
static uint8_t fail(uint8_t error) { platform_error=error;return 0; }
static uint8_t corrupt(void) { return fail(ERR_ENTRY_CORRUPTED); }
static uint8_t zeros(const uint8_t *p,uint8_t n) {
    while(n--)if(*p++)return 0;
    return 1;
}

uint8_t saved_paths_kind(const char *root,char files[2][128],uint8_t chain) SOURCE_BLOCK_CALL {
    char canonical[128];uint16_t n,base=0,dot=0,i;
    if(!sources_canonical(root,canonical)||strcmp(root,canonical))return 0;
    n=(uint16_t)strlen(canonical);
    for(i=0;i<n;++i) {
        if(canonical[i]=='/') {base=i+1;dot=0;}
        else if(canonical[i]=='.')dot=i;
    }
    if(!dot)dot=n;
    if(dot+4>127||dot-base+4>15)return 0;
    for(i=0;i<2;++i) {
        memcpy(files[i],canonical,dot);strcpy(files[i]+dot,i?".ZMP":chain?".CHN":".COM");
    }
    return 1;
}

static uint8_t open_reader(Reader *r,const char *path) {
    strcpy((char*)console_scratch,path);r->fd=open((char*)console_scratch,O_RDONLY);
    if(r->fd<0)return fail((uint8_t)-r->fd);
    r->position=0;return 1;
}
static uint8_t close_reader(Reader *r,uint8_t ok) {
    uint8_t previous=platform_error,error;
    if(r->fd<0)return ok;
    error=close(r->fd);r->fd=-1;
    if(!ok)platform_error=previous;
    else if(error)return fail(error);
    return ok;
}
/* Every syscall buffer belongs to the retained prefix and fits one MMU page.
 * No buffered window pointer survives source loading or a code-bank switch. */
static uint8_t take(Reader *r,uint8_t *out,uint16_t count) {
    uint16_t n,i;
    while(count) {
        n=count>128?128:count;platform_error=read(r->fd,console_scratch,&n);
        if(platform_error)return 0;
        if(!n||n>count||n>128)return corrupt();
        for(i=0;i<n;++i) {
            uint8_t value=console_scratch[i];
            if(out)*out++=value;
            ++r->position;
        }
        count-=n;
    }
    return 1;
}
static uint8_t at_end(Reader *r) {
    uint16_t n=1;platform_error=read(r->fd,console_scratch,&n);
    if(platform_error)return 0;
    return n?corrupt():1;
}
static uint8_t image(Reader *r,NativeImage *id,uint8_t chain) {
    uint8_t h[160],error;uint16_t n;
    if(!take(r,h,chain?96:160))return 0;
    error=native_header(h,chain,id);if(error)return fail(error);
    while(r->position<id->length) {
        n=(uint16_t)(id->length-r->position);if(n>128)n=128;
        if(!take(r,0,n))return 0;
    }
    if(!at_end(r))return 0;
    return 1;
}

static uint8_t cleanup(void) {
    uint8_t count,page;
    if(pending==255)return 1;
    page=far_get(pending,SOURCE_PAGE);
    if(page!=255) {
        if(!page_drop(page))return 0;
        far_set(pending,SOURCE_PAGE,255);
    }
    count=far_get(pending,0);
    while(count>1) {
        page=far_get(pending,count);
        if(!page_drop(page))return 0;
        far_set(pending,0,--count);
    }
    if(!page_drop(pending))return 0;
    pending=255;return 1;
}
static uint8_t metadata(uint16_t files) {
    uint32_t bytes=META+(uint32_t)files*RECORD;
    uint8_t count=(uint8_t)((bytes+PAGE_SIZE-1)/PAGE_SIZE),i,page;
    uint16_t j;
    if(bytes>32UL*PAGE_SIZE)return fail(ERR_NO_MORE_MEMORY);
    for(i=0;i<count;++i) {
        page=page_new(PAGE_MAP);if(page==255)return fail(ERR_NO_MORE_MEMORY);
        for(j=0;j<PAGE_SIZE;++j)far_set(page,j,0);
        page_initialized(page,PAGE_SIZE);
        if(!i) {pending=page;far_set(pending,SOURCE_PAGE,255);}
        far_set(pending,i+1,page);far_set(pending,0,i+1);
    }
    return 1;
}
static uint8_t meta_byte(uint16_t file,uint8_t at) {
    uint32_t offset=META+(uint32_t)file*RECORD+at;
    return far_get(far_get(pending,(uint16_t)(offset/PAGE_SIZE)+1),(uint16_t)(offset%PAGE_SIZE));
}
static void meta_put(uint16_t file,uint8_t at,uint8_t value) {
    uint32_t offset=META+(uint32_t)file*RECORD+at;
    far_set(far_get(pending,(uint16_t)(offset/PAGE_SIZE)+1),(uint16_t)(offset%PAGE_SIZE),value);
}
static uint32_t meta_word(uint16_t file,uint8_t at) {
    uint8_t i;uint32_t value=0;
    for(i=0;i<4;++i)value|=(uint32_t)meta_byte(file,at+i)<<(i*8);
    return value;
}
static uint8_t meta_path_equal(uint16_t file,const char *path) {
    uint8_t i,c;
    for(i=0;i<128;++i) {c=meta_byte(file,PATH+i);if(c!=(uint8_t)path[i])return 0;if(!c)return 1;}
    return 0;
}
static uint8_t source_check(const Source *work,const char *path,uint32_t bytes) {
    Source source;Diagnostic d;uint8_t owned=0,ok;int16_t found;
    if(bytes>SOURCE_LIMIT)return fail(ERR_NO_MORE_MEMORY);
    found=work&&!strcmp(work->path,path)?0:-1;
    if(found>=0) {
        source=*work;
        if(page_role(source.page)!=PAGE_SOURCE||!page_lifetime(source.page)||source.length>SOURCE_LIMIT)return corrupt();
    } else {
        source.page=255;
        if(!read_block(&source,path,&d)) {
            if(page_role(source.page)==PAGE_SOURCE)far_set(pending,SOURCE_PAGE,source.page);
            return platform_error?0:corrupt();
        }
        owned=1;far_set(pending,SOURCE_PAGE,source.page);
    }
    /* Editor buffers may retain trailing spaces. Canonical length is supplied
     * by the same snapshot policy as compilation. */
    if(owned)ok=source.length==bytes;
    else {
        uint16_t a=0,z,trim,total=0;
        while(a<source.length) {
            z=a;while(z<source.length&&far_get(source.page,z)!='\n')++z;
            trim=z;while(trim>a&&far_get(source.page,trim-1)==' ')--trim;
            total+=trim-a;if(z<source.length)++total;a=z+1;
        }
        ok=total==bytes;
    }
    if(owned) {
        if(!page_drop(source.page))return 0;
        far_set(pending,SOURCE_PAGE,255);
    }
    return ok?1:corrupt();
}

static uint8_t mapping(Reader *r,const NativeImage *id,const char *root,const Source *work,
                       uint16_t pc,SavedLocation *candidate) {
    uint8_t h[96],record[8],entry[16];
    char path[128],canonical[128];
    uint16_t files,i,j,file,n,previous=0,first,last,match=0,selected=0;
    uint32_t table,entries,entry_bytes,offset,start,end,position=0;
    if(!take(r,h,sizeof(h)))return 0;
    files=word(h+14);entries=longword(h+16);table=longword(h+84);entry_bytes=longword(h+88);
    if(memcmp(h,"ZMAP",4)||h[4]!=2||h[5]||word(h+6)!=96||word(h+8)!=1||
       word(h+10)!=id->origin||word(h+12)!=id->bytes||
       !zeros(h+20,64)||!zeros(h+92,4)||
       !files||table>32UL*PAGE_SIZE||(uint32_t)files*9>table||entries>id->bytes||
       entry_bytes!=entries*16||96UL+table+entry_bytes>32UL*PAGE_SIZE)return corrupt();
    if(!metadata(files))return 0;
    offset=96;
    while(r->position<96+table) {
        offset=r->position;
        if(96+table-offset<8)return corrupt();
        if(!take(r,record,8))return 0;
        file=word(record);n=word(record+2);
        if(file>=files||meta_word(file,OFFSET)||!n||n>127||n>96+table-r->position)return corrupt();
        if(!take(r,(uint8_t*)path,n))return 0;
        path[n]=0;
        if(strlen(path)!=n||!sources_canonical(path,canonical)||strcmp(path,canonical)||
           (!file&&strcmp(path,root)))return corrupt();
        for(j=0;j<files;++j)if(meta_word(j,OFFSET)&&meta_path_equal(j,path))return corrupt();
        for(i=0;i<4;++i) {
            meta_put(file,(uint8_t)i,record[4+i]);meta_put(file,OFFSET+i,(uint8_t)(offset>>(8*i)));
        }
        for(i=0;i<=n;++i)meta_put(file,PATH+i,(uint8_t)path[i]);
        if(!source_check(work,path,longword(record+4)))return 0;
    }
    for(i=0;i<files;++i)if(!meta_word(i,OFFSET))return corrupt();
    for(offset=0;offset<entries;++offset) {
        if(!take(r,entry,16))return 0;
        first=word(entry);last=word(entry+2);file=word(entry+4);
        start=longword(entry+8);end=longword(entry+12);
        if(first<previous||first>=last||last>id->bytes||file>=files||word(entry+6)||
           start>end||end>meta_word(file,0))return corrupt();
        previous=last;
        if(pc>=first&&pc<last) {++match;selected=file;position=start;}
    }
    if(!at_end(r))return 0;
    if(match!=1)return fail(ERR_INVALID_OFFSET);
    /* Reread the selected record, checking its path and length against the
     * temporary index before returning a bounded source position. */
    offset=meta_word(selected,OFFSET);
    {int32_t seek_to=(int32_t)offset;platform_error=seek(r->fd,&seek_to,SEEK_SET);if(platform_error)return 0;}
    r->position=offset;
    if(!take(r,record,8))return 0;
    n=word(record+2);
    if(word(record)!=selected||!n||n>127||longword(record+4)!=meta_word(selected,0))return corrupt();
    if(!take(r,(uint8_t*)path,n))return 0;
    path[n]=0;
    if(strlen(path)!=n||!meta_path_equal(selected,path))return corrupt();
    strcpy(candidate->path,path);candidate->position=(uint16_t)position;
    return 1;
}

uint8_t saved_location_kind(const char *root,const Source *work,uint16_t pc,
                       SavedLocation *location,uint8_t chain) SOURCE_BLOCK_CALL {
    Reader r;NativeImage id;SavedLocation candidate;char paths[2][128];
    uint8_t ok=0,error;
    platform_error=0;r.fd=-1;
    if(!cleanup())return 0;
    if(work&&(!memchr(work->path,0,sizeof(work->path))||
       page_role(work->page)!=PAGE_SOURCE||!page_lifetime(work->page)||work->length>SOURCE_LIMIT))return corrupt();
    if(!saved_paths_kind(root,paths,chain))return fail(ERR_INVALID_PATH);
    if(!strcmp(root,paths[0])||!strcmp(root,paths[1]))return fail(ERR_INVALID_PATH);
    if(!open_reader(&r,paths[0]))return 0;
    ok=image(&r,&id,chain);ok=close_reader(&r,ok);
    if(!ok)return 0;
    if(pc>=id.bytes)return fail(ERR_INVALID_OFFSET);
    if(!open_reader(&r,paths[1]))return 0;
    ok=mapping(&r,&id,root,work,pc,&candidate);ok=close_reader(&r,ok);
    error=platform_error;
    if(!cleanup())return 0;
    platform_error=error;
    if(ok)*location=candidate;
    return ok;
}

uint8_t saved_paths(const char *root,char files[2][128]) SOURCE_BLOCK_CALL {
    return saved_paths_kind(root,files,0);
}
uint8_t saved_location(const char *root,const Source *work,uint16_t pc,
                       SavedLocation *location) SOURCE_BLOCK_CALL {
    return saved_location_kind(root,work,pc,location,0);
}
