#include "file_runtime.h"
#include "path.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>

/* No user token, copied slot, or returned page list owns a native handle.
 * The only authority is an exact match with this module's private record.
 * Each page contains102 records. Saturated generations are retired instead
 * of wrapping and making a stale token authoritative again. */
#define RECORDS_PER_PAGE 102u
#define REGISTRY_PAGES 32u
#define OPENED 1u
#define READABLE 2u
#define WRITABLE 4u
#define DIRTY 8u
#define LOOKAHEAD 16u
typedef struct {
    uint16_t object,owner,generation,family,width,state,fd,position,count;
    uint16_t next,physical_low,physical_high,standard,device,reserved[2];
    char path[128];
} Record;
typedef char record_size_is_fixed[sizeof(Record)==ZF_RECORD_BYTES?1:-1];
uint8_t zf_record[ZF_RECORD_BYTES],zf_store;
uint16_t zf_service_value;
static uint8_t pages[REGISTRY_PAGES],page_count;
static uint16_t slot;
static FileJob *job;
static Record *record;
static uint8_t bytes[128];
static struct { uint8_t flags; char path[128]; } request;
static char rename_path[128];
static uint8_t verify_bytes[128];
static zos_stat_t info;
static uint32_t native_position;

#ifdef ZP_FILE_HOST
extern uint8_t zf_user_memory[65536];
#define USER(at) (zf_user_memory+(at))
extern uint8_t zf_memory_mode;
#define MEMORY_MODE zf_memory_mode
#else
#define USER(at) ((uint8_t *)(at))
#define MEMORY_MODE (*((uint8_t *)0xfe78))
#endif
static uint16_t word(uint16_t at) {
    return (uint16_t)USER(at)[0]|((uint16_t)USER(at)[1]<<8);
}
static void put_word(uint16_t at,uint16_t value) {
    USER(at)[0]=(uint8_t)value; USER(at)[1]=(uint8_t)(value>>8);
}
static void fail(uint16_t error,uint8_t detail) {
    if(!job->error) { job->error=error; job->detail=detail; }
}
static void native_error(uint8_t detail,uint8_t writing) {
    fail(detail==ERR_NO_SUCH_ENTRY?1:detail==ERR_CANNOT_REGISTER_MORE?0xf3:
         detail==ERR_READ_ONLY||detail==ERR_BAD_MODE?(writing==2?0x20:writing?3:2):
         detail==ERR_NOT_A_FILE||detail==ERR_NOT_SUPPORTED||detail==ERR_NOT_IMPLEMENTED||
         detail==ERR_ALREADY_EXIST?0x20:writing?0xf0:0x99,detail);
}
static uint8_t alloc_page(uint8_t *page) {
    uint8_t error;
    if(MEMORY_MODE) {
        error=zf_service(0,0);
        if(!error) *page=(uint8_t)zf_service_value;
    } else error=palloc(page);
    if(error) fail(0x1ff,error);
    return !error;
}
static uint8_t drop_page(uint8_t page) {
    return MEMORY_MODE?zf_service(1,page):pfree(page);
}
static int8_t open_path(const char *path,uint8_t flags) {
    uint8_t error;
    if(!MEMORY_MODE) return open(path,flags);
    request.flags=flags; strcpy(request.path,path);
    error=zf_service(2,(uint16_t)(uintptr_t)&request);
    return error?-(int8_t)error:(int8_t)zf_service_value;
}
static int8_t open_file(uint8_t flags) { return open_path(record->path,flags); }
static uint8_t close_file(uint8_t fd) {
    return MEMORY_MODE?zf_service(3,fd):close((int8_t)fd);
}
static void load_record(uint16_t index) {
    uint16_t n=index-1;
    zf_store=0; zf_record_io(pages[n/RECORDS_PER_PAGE],(n%RECORDS_PER_PAGE)*ZF_RECORD_BYTES);
}
static void save_record(uint16_t index) {
    uint16_t n=index-1;
    zf_store=1; zf_record_io(pages[n/RECORDS_PER_PAGE],(n%RECORDS_PER_PAGE)*ZF_RECORD_BYTES);
}
static uint8_t within(uint16_t at,uint16_t size,uint16_t low,uint16_t high) {
    return at>=low&&at<=high&&size<=high-at;
}
/* Validate the whole chain before accepting an interior object. Saved IX
 * links increase toward the program root. The compiler's private[-8] word
 * is the real lower bound after allocating all local/formal storage. */
static uint8_t frame_owner(uint16_t at,uint16_t size,uint16_t *owner) {
    uint16_t frame=job->frame,next,low,found=0;
    while(frame) {
        if(frame<0xc808u||frame>0xfbfcu||word(frame-4)!=frame) return 0;
        low=word(frame-8);
        if(low<0xc800u||low>frame-8||low<job->stack_pointer) return 0;
        if(within(at,size,low,frame-8)) found=frame;
        next=word(frame);
        if(next&&next<=frame) return 0;
        frame=next;
    }
    if(!found) return 0;
    *owner=found; return 1;
}
/* Inspect every header before following it, including headers after a
 * candidate object. Metadata, freed payloads and cross-allocation spans
 * cannot acquire a new binding even if token bytes look initialized. */
static uint8_t heap_owner(uint16_t at,uint16_t size,uint16_t *owner) {
    uint16_t cursor=job->heap_floor,span,next,found=0,header;
    while(cursor<job->heap_frontier) {
        if(job->heap_frontier-cursor<2) return 0;
        header=word(cursor); span=header&0x7fff;
        if(span&1||span>job->heap_frontier-cursor-2) return 0;
        next=cursor+2+span;
        if(!(header&0x8000)&&within(at,size,cursor+2,next)) found=cursor+2;
        cursor=next;
    }
    if(cursor!=job->heap_frontier||!found) return 0;
    *owner=found; return 1;
}
static uint8_t object_owner(uint16_t *owner) {
    if(within(job->object,16,0x8000,job->static_end)) { *owner=0; return 1; }
    if(heap_owner(job->object,16,owner)) return 1;
    return frame_owner(job->object,16,owner);
}
static uint8_t readable(uint16_t at,uint16_t size) {
    uint16_t owner;
    return within(at,size,0x8000,job->static_end)||
        within(at,size,job->stack_pointer,0xfc00)||heap_owner(at,size,&owner);
}
static uint8_t descriptor(uint16_t *family,uint16_t *width) {
    if(!within(job->descriptor,3,job->code_origin,job->static_end)) return 0;
    *family=USER(job->descriptor)[0]; *width=word(job->descriptor+1);
    return *family<=2&&(*family!=0||!*width)&&(*family!=2||*width==128);
}
static void token(uint16_t owner,uint16_t family,uint16_t width) {
    uint16_t at=job->object;
    memset(USER(at),0,16);
    put_word(at,0x465a); put_word(at+2,slot); put_word(at+4,record->generation);
    put_word(at+6,owner); USER(at)[8]=(uint8_t)family; put_word(at+10,width);
    USER(at)[9]=record->standard?1:0;
}
static uint8_t bound(uint16_t owner,uint16_t family,uint16_t width) {
    uint16_t at=job->object,n=word(at+2);
    uint8_t i;
    if(word(at)!=0x465a||!n||n>(uint16_t)page_count*RECORDS_PER_PAGE||
       word(at+6)!=owner||USER(at)[8]!=family||USER(at)[9]>1||word(at+10)!=width) return 0;
    for(i=12;i<16;++i) if(USER(at)[i]) return 0;
    load_record(n);
    if(record->object!=at||record->owner!=owner||record->family!=family||
       record->width!=width||record->generation!=word(at+4)||USER(at)[9]!=(record->standard?1:0)) return 0;
    slot=n; return 1;
}
static uint8_t new_record(uint16_t owner,uint16_t family,uint16_t width) {
    uint16_t i,generation;
    uint8_t page;
    for(i=1;i<=(uint16_t)page_count*RECORDS_PER_PAGE;++i) {
        load_record(i);
        if(!record->object&&record->generation!=0xffff) break;
    }
    if(i>(uint16_t)page_count*RECORDS_PER_PAGE) {
        if(page_count==REGISTRY_PAGES) { fail(0x1ff,ERR_NO_MORE_MEMORY); return 0; }
        if(!alloc_page(&page)) return 0;
        pages[page_count++]=page; zf_page_clear(page); load_record(i);
    }
    generation=record->generation+1;
    memset(record,0,sizeof(*record)); record->object=job->object; record->owner=owner;
    record->generation=generation; record->family=family; record->width=width;
    slot=i; return 1;
}
/* Only immutable compiler-supplied addresses acquire preassigned identity.
 * A flag or token copied into any other object cannot grant that authority.
 * Check the complete bounded table before registering any of its objects. */
static uint8_t standards(void) {
    uint16_t table=job->standard_table,count,i,j,at,object,identity,saved=job->object;
    uint8_t identities=0;
    if(!table) return 1;
    if(!within(table,2,job->code_origin,job->static_end)) { fail(0x1ff,0); return 0; }
    count=word(table);
    if(count>ZF_STANDARD_KBD||!within(table+2,count*4,job->code_origin,job->static_end)) { fail(0x1ff,0); return 0; }
    for(i=0;i<count;++i) {
        at=table+2+4*i; object=word(at); identity=word(at+2);
        if(!identity||identity>ZF_STANDARD_KBD||(identities&(1u<<(identity-1)))||
           !within(object,16,0x8000,job->code_origin)) { fail(0x1ff,0); return 0; }
        identities|=1u<<(identity-1);
        for(j=0;j<i;++j) if(word(table+2+4*j)==object) { fail(0x1ff,0); return 0; }
    }
    for(i=0;i<count&&!job->error;++i) {
        at=table+2+4*i; job->object=word(at); identity=word(at+2);
        if(bound(0,0,0)) {
            if(record->standard!=identity) fail(0x1ff,0);
            continue;
        }
        for(j=1;j<=(uint16_t)page_count*RECORDS_PER_PAGE;++j) {
            load_record(j);
            if(record->object==job->object) { fail(0x1ff,0); break; }
        }
        if(job->error||!new_record(0,0,0)) break;
        record->standard=identity;
        record->device=identity==ZF_STANDARD_KBD?identity:0;
        record->state=identity==ZF_STANDARD_KBD?READABLE:READABLE|WRITABLE;
        token(0,0,0); save_record(slot);
    }
    job->object=saved; return !job->error;
}
/* The pinned kernel may return an old-size byte after a successful write
 * advances its cursor inside an existing (or just truncated) file. Distinguish
 * that exact post-write path through supported cursor/size queries. Driver
 * failures return before cursor advancement and must never become success.
 * No raw kernel table access or third-party patch is needed. */
static uint8_t bookkeeping_status(uint32_t size,uint32_t position) {
    if(size<=position) return 0;
    if((size>>24)!=(position>>24)) return (uint8_t)(size>>24);
    if((size>>16)!=(position>>16)) return (uint8_t)(size>>16);
    if((size>>8)!=(position>>8)) return (uint8_t)(size>>8);
    return 0;
}
static uint8_t write_bytes(const uint8_t *data,uint16_t *size) {
    uint16_t wanted=*size;
    uint8_t error=write((int8_t)record->fd,data,size);
    int32_t next=0,end=0,restored;
    uint32_t advanced;
    if(!error) {
        if(*size<=wanted) native_position+=*size;
        return 0;
    }
    if(!wanted||seek((int8_t)record->fd,&next,SEEK_CUR)) return error;
    if((uint32_t)next<=native_position) return error;
    advanced=(uint32_t)next-native_position;
    if(advanced>wanted) return error;
    if(seek((int8_t)record->fd,&end,SEEK_END)) return error;
    restored=next;
    if(seek((int8_t)record->fd,&restored,SEEK_SET)||restored!=next) return error;
    if(bookkeeping_status((uint32_t)end,(uint32_t)next)!=error) return error;
    *size=(uint16_t)advanced; native_position=(uint32_t)next;
    return 0;
}
/* The byte bridge owns fixed prefix staging. Neither an outer SDK pointer
 * nor a nested request ever points into page3 or crosses a virtual page. */
static uint16_t transfer(uint16_t address,uint16_t count,uint8_t writing) {
    uint16_t done=0,size,n;
    uint8_t error;
    while(done<count) {
        n=count-done; if(n>sizeof(bytes)) n=sizeof(bytes);
        if(writing) memcpy(bytes,USER(address+done),n);
        size=n;
        error=writing?write_bytes(bytes,&size):read((int8_t)record->fd,bytes,&size);
        /* The SDK leaves size unchanged on error; it is not a transferred
         * byte count. Never copy stale input or claim completion from it. */
        if(error) { native_error(error,writing); break; }
        if(size>n) { fail(writing?0xf0:0x99,ERR_INVALID_PARAMETER); break; }
        if(!writing&&size) memcpy(USER(address+done),bytes,size);
        if(!writing) native_position+=size;
        done+=size;
        if(!size) { if(writing) fail(0xf0,0); break; }
    }
    return done;
}
static uint8_t seek_file(uint32_t at) {
    uint32_t requested=at;
    uint8_t error=seek((int8_t)record->fd,(int32_t *)&at,SEEK_SET);
    if(error) native_error(error,0);
    else if(at!=requested) fail(0x99,0); /* native seek clamps at EOF */
    else native_position=at;
    return !job->error;
}
static uint8_t header(uint8_t writing) {
    uint16_t size,done=0;
    uint8_t error;
    if(!seek_file(0)) return 0;
    if(writing) {
        bytes[0]=(uint8_t)record->count; bytes[1]=(uint8_t)(record->count>>8);
        bytes[2]=(uint8_t)record->width; bytes[3]=(uint8_t)(record->width>>8);
    }
    while(done<4) {
        size=4-done;
        error=writing?write_bytes(bytes+done,&size):read((int8_t)record->fd,bytes+done,&size);
        if(error) { native_error(error,writing); return 0; }
        if(!size||size>4-done) { fail(writing?0xf0:0x99,0); return 0; }
        if(!writing) native_position+=size;
        done+=size;
    }
    if(!writing) {
        if(((uint16_t)bytes[2]|((uint16_t)bytes[3]<<8))!=record->width) fail(0x90,0);
        else record->count=(uint16_t)bytes[0]|((uint16_t)bytes[1]<<8);
    }
    return !job->error;
}
static uint8_t access(uint8_t writing) {
    if(!(record->state&(writing?WRITABLE:READABLE))) {
        fail(record->family==2&&!(record->state&OPENED)?4:writing?3:2,0); return 0;
    }
    return 1;
}
static void text_position(void) {
    native_position=(uint32_t)record->physical_low|((uint32_t)record->physical_high<<16);
}
static void text_remember(void) {
    record->physical_low=(uint16_t)native_position;
    record->physical_high=(uint16_t)(native_position>>16);
}
static void text_bytes(uint16_t count) {
    uint16_t done=0,size;
    uint8_t error;
    while(done<count&&!job->error) {
        size=count-done; error=write_bytes(bytes+done,&size);
        if(error) native_error(error,1);
        else if(!size||size>count-done) fail(0xf0,0);
        else done+=size;
    }
}
static uint8_t text_peek(void) {
    uint16_t size=1;
    uint8_t error;
    if(!(record->state&LOOKAHEAD)) {
        error=read((int8_t)record->fd,bytes,&size);
        if(error) { native_error(error,0); return 26; }
        if(size>1) { fail(0x99,ERR_INVALID_PARAMETER); return 26; }
        record->next=size?bytes[0]:26;
        record->state|=LOOKAHEAD;
    }
    return (uint8_t)record->next;
}
static uint8_t text_take(void) {
    uint8_t c=text_peek();
    if(c!=26) record->state&=(uint16_t)~LOOKAHEAD;
    return c;
}
static uint8_t text_space(uint8_t c,uint8_t lines) {
    return c==32||c==9||(lines&&(c==13||c==10));
}
static void text_predicate(uint16_t operation) {
    uint8_t c=text_peek();
    if(operation==ZF_SEEKEOF||operation==ZF_SEEKEOLN)
        while(!job->error&&text_space(c,operation==ZF_SEEKEOF)) {
            text_take(); c=text_peek();
        }
    job->result=c==26||((operation==ZF_EOLN||operation==ZF_SEEKEOLN)&&c==13);
}
static uint8_t text_input_target(void) {
    uint16_t out=job->data;
    uint8_t kind=(uint8_t)job->count,limit=(uint8_t)(job->count>>8),valid;
    if(kind!=ZF_TEXT_STRING&&limit) { fail(0x1ff,0); return 0; }
    if(kind==ZF_TEXT_STRING) valid=readable(out,(uint16_t)limit+1);
    else if(kind==ZF_TEXT_CHAR) valid=readable(out,1);
    else valid=(kind==ZF_TEXT_INTEGER||kind==ZF_TEXT_BYTE||kind==ZF_TEXT_REAL)&&
        readable(out,ZF_NUMBER_BYTES)&&
        readable(word(out+ZF_NUMBER_TARGET),kind==ZF_TEXT_BYTE?1:kind==ZF_TEXT_REAL?6:2);
    if(!valid) fail(0x1ff,0);
    return valid;
}
static uint8_t text_output_value(void) {
    if(!readable(job->data,1)||!readable(job->data,(uint16_t)USER(job->data)[0]+1)) {
        fail(0x1ff,0); return 0;
    }
    return 1;
}
static void text_read(void) {
    uint16_t out=job->data;
    uint8_t kind=(uint8_t)job->count,limit=(uint8_t)(job->count>>8),n=0,c;
    if(!text_input_target()) return;
    if(kind==ZF_TEXT_CHAR) {
        c=text_take(); if(!job->error) { USER(out)[0]=c; job->actual=1; }
        return;
    }
    if(kind!=ZF_TEXT_STRING) {
        out+=ZF_NUMBER_TOKEN; limit=30;
        c=text_peek();
        while(!job->error&&text_space(c,1)) { text_take(); c=text_peek(); }
    }
    while(n<limit&&!job->error) {
        c=text_peek();
        if(c==26||c==13||(kind!=ZF_TEXT_STRING&&text_space(c,1))) break;
        USER(out+1+n)[0]=text_take(); ++n;
    }
    if(!job->error) {
        USER(out)[0]=n; job->actual=kind==ZF_TEXT_STRING||n!=0;
    }
}
static void text_readln(void) {
    uint8_t c;
    do { c=text_take(); } while(!job->error&&c!=13&&c!=26);
    if(c==13&&!job->error&&text_peek()==10) text_take();
}
static void text_write(uint8_t newline) {
    uint16_t n;
    text_position();
    if(newline) { bytes[0]=13; bytes[1]=10; text_bytes(2); }
    else {
        if(!text_output_value()) return;
        n=USER(job->data)[0];
        transfer(job->data+1,n,1);
    }
    text_remember();
}
static void flush(void) {
    if(record->family==1&&(record->state&DIRTY)) {
        if(header(1)) record->state&=(uint16_t)~DIRTY;
    }
}
static void close_record(void) {
    uint16_t primary=job->error,detail=job->detail;
    uint8_t error;
    if(record->device||!(record->state&OPENED)) return;
    job->error=0; job->detail=0;
    if(!record->family&&(record->state&WRITABLE)) {
        text_position(); bytes[0]=26; text_bytes(1); text_remember();
    } else flush();
    error=close_file((uint8_t)record->fd);
    record->state=0; record->fd=0; /* VFS consumes even a failed close. */
    if(error) fail(error==ERR_NO_SUCH_ENTRY?0xff:error==ERR_CANNOT_REGISTER_MORE?0xf3:0xf0,error);
    if(primary) { job->error=primary; job->detail=detail; }
}
static void activate(uint8_t rewriting) {
    int8_t fd;
    uint8_t flags,error;
    if(record->family==1&&!record->width) { fail(0x90,0); return; }
    if(record->state&OPENED) { fail(0x20,0); return; }
    /* On the pinned OS, descriptor stat does not mask access flags from
     * its filesystem byte. Use supported path stat (its temporary open is
     * read-only) before acquiring our descriptor, including at fd capacity.
     * Classify an existing directory before any truncating open. */
    error=stat(record->path,&info);
    if(error&&(!rewriting||error!=ERR_NO_SUCH_ENTRY)) { native_error(error,rewriting); return; }
    if(!error&&!D_ISFILE(info.s_flags)) { fail(0x20,0); return; }
    if(rewriting) info.s_size=0;
    flags=rewriting?O_RDWR|O_CREAT|O_TRUNC:O_RDWR;
    if(!record->family) flags=rewriting?O_WRONLY|O_CREAT|O_TRUNC:O_RDONLY;
    record->state=0;
    fd=open_file(flags);
    if(fd<0&&!rewriting&&record->family&&-fd==ERR_READ_ONLY) {
        flags=O_RDONLY; fd=open_file(flags);
    }
    if(fd<0) { native_error((uint8_t)-fd,rewriting); return; }
    record->fd=(uint8_t)fd; record->position=0; record->count=0;
    record->next=record->physical_low=record->physical_high=0;
    record->state=OPENED|(flags&O_WRONLY?WRITABLE:flags&O_RDWR?READABLE|WRITABLE:READABLE);
    if(record->family==1) header(rewriting);
    else if(record->family==2) {
        if(info.s_size/128u>65535u) fail(0xf2,0);
        else record->count=(uint16_t)(info.s_size/128u);
    }
    if(job->error) close_record();
}
static void component(uint8_t writing) {
    uint16_t size=record->width;
    if(record->family!=1) { fail(0x20,0); return; }
    if(!access(writing)) return;
    if(!readable(job->data,size)) { fail(0x1ff,0); return; }
    if(!writing&&record->position>=record->count) { fail(0x99,0); return; }
    if(writing&&record->position==65535u) { fail(0xf2,0); return; }
    if(!seek_file(4ul+(uint32_t)record->position*size)) return;
    if(transfer(job->data,size,writing)!=size) { if(!job->error) fail(0x99,0); return; }
    ++record->position; job->actual=1;
    if(writing) {
        if(record->position>record->count) record->count=record->position;
        record->state|=DIRTY;
    }
}
static void blocks(uint8_t writing,uint8_t partial) {
    uint16_t i;
    uint32_t size=(uint32_t)job->count*128u;
    if(record->family!=2) { fail(0x20,0); return; }
    if(!access(writing)) return;
    if(size>65535u||!readable(job->data,(uint16_t)size)) { fail(0x1ff,0); return; }
    if((uint32_t)record->position+job->count>65535u) { fail(0xf2,0); return; }
    if(!seek_file((uint32_t)record->position*128u)) return;
    for(i=0;i<job->count;++i) {
        if(transfer(job->data+i*128u,128,writing)!=128) break;
        ++record->position; ++job->actual;
        if(writing&&record->position>record->count) record->count=record->position;
    }
    if(!partial&&job->actual!=job->count&&!job->error) fail(writing?0xf0:0x99,0);
}
/* Capture and resolve once, before replacing any association. The fixed prefix
 * owns every native syscall buffer, including the returned current directory.
 * Runtime data names never acquire the editor's implicit .PAS extension. */
static uint8_t assigned_path(void) {
    uint16_t n;
    uint8_t error;
    if(!readable(job->data,1)) return 0;
    n=USER(job->data)[0];
    if(!n||n>127||!readable(job->data+1,n)||memchr(USER(job->data+1),0,n)) return 0;
    memcpy(request.path,USER(job->data+1),n); request.path[n]=0;
    if(n==4&&(request.path[0]=='K'||request.path[0]=='k')&&
       (request.path[1]=='B'||request.path[1]=='b')&&
       (request.path[2]=='D'||request.path[2]=='d')&&request.path[3]==':') {
        strcpy(request.path,"KBD:"); return 1;
    }
    bytes[0]=0;
    if(n<2||request.path[1]!=':') {
        error=curdir((char*)bytes);
        if(error) { fail(0x20,error); return 0; }
    }
    return path_resolve(request.path,(char*)bytes,request.path);
}
/* Rename is deliberately a copy/verify/remove transaction on the supported
 * VFS. Every handle is parent-owned immediately in RAM mode, and every local
 * close is attempted even after failure. Neither the binding nor the old name
 * changes until both verification handles have closed successfully. A failed
 * transaction may leave the requested destination for inspection/recovery. */
static void rename_close(int8_t fd) {
    uint8_t error;
    if(fd<0) return;
    error=close_file((uint8_t)fd);
    if(error) native_error(error,2);
}
static void rename_read(int8_t fd,uint8_t *out,uint16_t count) {
    uint16_t done=0,n;
    uint8_t error;
    while(done<count&&!job->error) {
        n=count-done; error=read(fd,out+done,&n);
        if(error) native_error(error,0);
        else if(!n||n>count-done) fail(0x99,0);
        else done+=n;
    }
}
static void rename_end(int8_t fd) {
    uint16_t n=1;
    uint8_t error=read(fd,bytes,&n);
    if(error) native_error(error,0);
    else if(n) fail(0xf0,0);
}
static void rename_file(void) {
    uint32_t size,left;
    uint16_t n,done,written;
    uint8_t error;
    int8_t in=-1,out=-1;
    if(record->state&OPENED) { fail(0x20,0); return; }
    if(!assigned_path()) { fail(0x20,ERR_INVALID_PATH); return; }
    strcpy(rename_path,request.path);
    error=stat(rename_path,&info);
    if(!error) { fail(0x20,ERR_ALREADY_EXIST); return; }
    if(error!=ERR_NO_SUCH_ENTRY) { native_error(error,2); return; }
    error=stat(record->path,&info);
    if(error) { native_error(error,2); return; }
    if(!D_ISFILE(info.s_flags)) { fail(0x20,0); return; }
    size=info.s_size;
    in=open_file(O_RDONLY);
    if(in<0) { native_error((uint8_t)-in,2); return; }
    /* Existing names were rejected above. No truncation is used or promised
     * atomicity against an external writer racing on host-backed media. */
    out=open_path(rename_path,O_WRONLY|O_CREAT);
    if(out<0) native_error((uint8_t)-out,2);
    left=size;
    while(left&&!job->error) {
        n=left>sizeof(bytes)?sizeof(bytes):(uint16_t)left;
        rename_read(in,bytes,n); done=0;
        while(done<n&&!job->error) {
            written=n-done; error=write(out,bytes+done,&written);
            if(error) native_error(error,1);
            else if(!written||written>n-done) fail(0xf0,0);
            else done+=written;
        }
        left-=n;
    }
    if(!job->error) rename_end(in);
    rename_close(out); rename_close(in);
    if(job->error) return;
    in=open_file(O_RDONLY);
    if(in<0) { native_error((uint8_t)-in,2); return; }
    out=open_path(rename_path,O_RDONLY);
    if(out<0) native_error((uint8_t)-out,2);
    left=size;
    while(left&&!job->error) {
        n=left>sizeof(bytes)?sizeof(bytes):(uint16_t)left;
        rename_read(in,bytes,n); rename_read(out,verify_bytes,n);
        if(!job->error&&memcmp(bytes,verify_bytes,n)) fail(0xf0,0);
        left-=n;
    }
    if(!job->error) { rename_end(in); if(!job->error) rename_end(out); }
    rename_close(out); rename_close(in);
    if(job->error) return;
    error=rm(record->path);
    if(error) native_error(error,2);
    else strcpy(record->path,rename_path);
}
/* The file module validates ownership while mapped; the restored prefix owns
 * keyboard services. Only the authenticated binding's small lookahead state
 * crosses that handoff. Logical Close owns no native descriptor and is a no-op. */
static void keyboard_stream(void) {
    switch(job->operation) {
    case ZF_RESET: case ZF_REWRITE:
        record->state=READABLE; record->next=0; save_record(slot); return;
    case ZF_CLOSE: return;
    case ZF_TEXT_BEGIN: case ZF_TEXT_READLN: case ZF_EOF:
    case ZF_EOLN: case ZF_SEEKEOF: case ZF_SEEKEOLN:
        if(!access(0)) return;
        break;
    case ZF_TEXT_READ:
        if(!access(0)||!text_input_target()) return;
        break;
    case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN: fail(3,0); return;
    default: fail(0x20,0); return;
    }
    job->stream=ZF_STANDARD_KBD;
    job->lookahead=(uint8_t)record->next|((record->state&LOOKAHEAD)?0x100u:0);
}
void zf_dispatch(FileJob *invocation) ZFILE_CALL {
    uint16_t owner,family,width;
    uint8_t valid,error;
    job=invocation;
    if(job->operation!=ZF_STREAM_RETURN) {
        job->error=0; job->detail=0; job->result=0; job->actual=0; job->stream=0; job->lookahead=0;
    }
    record=(Record *)zf_record;
    if(job->static_end<0x8000u||job->static_end>job->heap_floor||
       job->heap_floor>job->heap_frontier||job->heap_frontier>job->stack_pointer||
       job->stack_pointer<0xc800u||job->stack_pointer>0xfc00u||
       job->code_origin<0x8000u||job->code_origin>job->static_end||
       (job->heap_floor&1)||!descriptor(&family,&width)||!object_owner(&owner)) {
        fail(0x1ff,0); return;
    }
    if(job->operation!=ZF_STREAM_RETURN&&!standards()) return;
    valid=bound(owner,family,width);
    if(job->operation==ZF_STREAM_RETURN) {
        if(!valid||record->device!=ZF_STANDARD_KBD||job->stream!=ZF_STANDARD_KBD||job->lookahead>0x1ffu)
            fail(0x1ff,0);
        else {
            record->next=(uint8_t)job->lookahead;
            record->state=(record->state&~LOOKAHEAD)|((job->lookahead&0x100u)?LOOKAHEAD:0);
            save_record(slot);
        }
        return;
    }
    if(valid&&record->standard) {
        if(job->operation==ZF_ASSIGN||job->operation==ZF_RESET||job->operation==ZF_REWRITE||job->operation==ZF_CLOSE)
            fail(0x22,0);
        else if(record->device) keyboard_stream();
        else if(job->operation==ZF_TEXT_READ) {
            if(text_input_target()) job->stream=record->standard;
        } else if(job->operation==ZF_TEXT_WRITE) {
            if(text_output_value()) job->stream=record->standard;
        } else if(job->operation==ZF_TEXT_BEGIN||job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_WRITELN||
                  job->operation==ZF_EOF||(job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN))
            job->stream=record->standard;
        else fail(0x20,0);
        return;
    }
    if(job->operation==ZF_ASSIGN) {
        if(!assigned_path()) { fail(0x20,ERR_INVALID_PATH); return; }
        error=!strcmp(request.path,"KBD:");
        if(error&&family) { fail(0x20,0); return; }
        if(valid&&(record->state&OPENED)) { fail(0x20,0); return; }
        if(!valid&&!new_record(owner,family,width)) return;
        record->device=error?ZF_STANDARD_KBD:0; record->state=error?READABLE:0; record->next=0;
        strcpy(record->path,request.path); token(owner,family,width); save_record(slot); return;
    }
    if(!valid) {
        fail(family==2?4:job->operation==ZF_READ||job->operation==ZF_TEXT_READ||
             job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_BEGIN||job->operation==ZF_EOF||
             (job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN)?2:3,0); return;
    }
    if(record->device) { keyboard_stream(); return; }
    switch(job->operation) {
    case ZF_TEXT_BEGIN:
        if(record->family) fail(0x20,0);
        break;
    case ZF_RESET: activate(0); break;
    case ZF_REWRITE: activate(1); break;
    case ZF_CLOSE: close_record(); break;
    case ZF_READ: component(0); break;
    case ZF_WRITE: component(1); break;
    case ZF_EOF:
        if(!record->family) { if(access(0)) text_predicate(ZF_EOF); break; }
        /* fall through */
    case ZF_POSITION: case ZF_SIZE:
        if(!record->family) { fail(0x20,0); break; }
        if(access(0)) job->result=job->operation==ZF_POSITION?record->position:
            job->operation==ZF_SIZE?record->count:record->position>=record->count;
        break;
    case ZF_SEEK:
        if(record->family==0) fail(0x20,0);
        else if(access(0)) {
            if(job->count>record->count) fail(0x91,0);
            else record->position=job->count;
        }
        break;
    case ZF_FLUSH:
        if(record->family!=1) fail(0x20,0);
        else if(record->state&OPENED) flush();
        else fail(3,0);
        break;
    case ZF_ERASE:
        if(record->state&OPENED) fail(0x20,0);
        else {
            error=stat(record->path,&info);
            if(error) native_error(error,2);
            else if(!D_ISFILE(info.s_flags)) fail(0x20,0);
            else { error=rm(record->path); if(error) native_error(error,2); }
        }
        break;
    case ZF_RENAME: rename_file(); break;
    case ZF_BLOCKREAD: case ZF_BLOCKREAD_PARTIAL: case ZF_BLOCKWRITE: case ZF_BLOCKWRITE_PARTIAL:
        blocks(job->operation>=ZF_BLOCKWRITE,job->operation&1); break;
    case ZF_TEXT_READ: case ZF_TEXT_READLN: case ZF_EOLN: case ZF_SEEKEOF: case ZF_SEEKEOLN:
        if(record->family) fail(0x20,0);
        else if(access(0)) {
            if(job->operation==ZF_TEXT_READ) text_read();
            else if(job->operation==ZF_TEXT_READLN) text_readln();
            else text_predicate(job->operation);
        }
        break;
    case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN:
        if(record->family) fail(0x20,0);
        else if(access(1)) text_write(job->operation==ZF_TEXT_WRITELN);
        break;
    default: fail(0x20,ERR_NOT_IMPLEMENTED); break;
    }
    save_record(slot);
}
void zf_cleanup(FileCleanup *ending) ZFILE_CALL {
    FileJob result;
    uint16_t i;
    uint8_t selected,error;
    memset(&result,0,sizeof(result)); job=&result; record=(Record *)zf_record;
    for(i=1;i<=(uint16_t)page_count*RECORDS_PER_PAGE;++i) {
        load_record(i);
        selected=ending->operation==ZF_END_PROGRAM||
            ((ending->operation==ZF_END_FRAME||ending->operation==ZF_END_OBJECT)&&record->owner==ending->owner)||
            (ending->operation==ZF_END_RANGE&&record->owner>=ending->owner&&record->owner<ending->end);
        if(record->object&&selected) {
            close_record(); record->object=0; save_record(i);
        }
    }
    if(ending->operation==ZF_END_PROGRAM) {
        while(page_count) {
            error=drop_page(pages[page_count-1]);
            if(error) { fail(0x1ff,error); break; }
            --page_count;
        }
    }
    ending->error=result.error; ending->detail=result.detail;
    job=0;
}
