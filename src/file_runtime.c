#include "file_runtime.h"
#include "path.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_serial.h>

/* TP3 stores its176-byte FIB with the file variable. The first12 bytes
 * retain flags/current-byte/count/width/position fields. CP/M FCB/sector
 * storage is native backend metadata and a128-byte Zeal path here. */
#define OPENED 1u
#define READABLE 2u
#define WRITABLE 4u
#define DIRTY 8u
#define LOOKAHEAD 16u
#define FILE_MARKER 0x465au
#include <stddef.h>
typedef struct {
    uint8_t flags,next;
    uint16_t sector,count,width,position,unused;
    uint16_t marker,family,state,fd,physical_low,physical_high,standard,device;
    uint16_t reserved[10];
    char path[128];
} Record;
typedef char file_block_size[sizeof(Record)==ZF_FILE_BYTES?1:-1];
typedef char file_count_offset[offsetof(Record,count)==4?1:-1];
typedef char file_path_offset[offsetof(Record,path)==48?1:-1];
uint16_t zf_service_value;
/* One disk handle lives only for this synchronous call. Pascal state, including
 * recursive snapshots, never retains native descriptor authority. */
#if defined(__SDCC) && defined(ZP_COMPACT_EXPERIMENT)
typedef char low_copy_layout[(sizeof(FileLowCopy)==6&&offsetof(FileLowCopy,buffer)==2&&
    offsetof(FileLowCopy,count)==4&&offsetof(FileLowCopy,writing)==5)?1:-1];
#endif
static int8_t native_fd;
static FileJob *job;
static Record *record;
static uint8_t bytes[128];
/* Open requests finish before Rename begins comparing its streams. Both
 * buffers belong to this synchronous transport phase; no callback or retained
 * SDK pointer outlives an open request. Keep the live rename destination apart. */
static union {
    struct { uint8_t flags; char path[128]; } request;
    uint8_t verify_bytes[128];
} transport;
static char rename_path[128];
static zos_stat_t info;
static uint32_t native_position;
static uint8_t serial_attribute;

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
static void fail(uint16_t error,uint8_t detail) {
    if(!job->error) { job->error=error; job->detail=detail; }
}
static void native_error(uint8_t detail,uint8_t writing) {
    fail(detail==ERR_NO_SUCH_ENTRY?1:detail==ERR_CANNOT_REGISTER_MORE?0xf3:
         detail==ERR_READ_ONLY||detail==ERR_BAD_MODE?(writing==2?0x20:writing?3:2):
         detail==ERR_NOT_A_FILE||detail==ERR_NOT_SUPPORTED||detail==ERR_NOT_IMPLEMENTED||
         detail==ERR_ALREADY_EXIST?0x20:writing?0xf0:0x99,detail);
}
static int8_t open_path(const char *path,uint8_t flags) {
    uint8_t error;
    int8_t fd;
    transport.request.flags=flags;
    if(path!=transport.request.path)strcpy(transport.request.path,path);
    if(!MEMORY_MODE) fd=open(transport.request.path,flags);
    else {
        error=zf_service(2,(uint16_t)(uintptr_t)&transport.request);
        fd=error?-(int8_t)error:(int8_t)zf_service_value;
    }
    return fd;
}
static int8_t open_file(uint8_t flags) { return open_path(record->path,flags); }
static uint8_t stat_file(void) {
    strcpy(transport.request.path,record->path);return stat(transport.request.path,&info);
}
static uint8_t remove_file(void) {
    strcpy(transport.request.path,record->path);return rm(transport.request.path);
}
static uint8_t close_file(uint8_t fd) {
    return MEMORY_MODE?zf_service(3,fd):close((int8_t)fd);
}
static uint8_t within(uint16_t at,uint16_t size,uint16_t low,uint16_t high) {
    return at>=low&&at<=high&&size<=high-at;
}
/* Inspect every header before following it, including headers after a
 * candidate object. Metadata, freed payloads and cross-allocation spans
 * must not be passed to native transfers. */
static uint8_t heap_span(uint16_t at,uint16_t size) {
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
    return 1;
}
static uint8_t readable(uint16_t at,uint16_t size) {
    return within(at,size,0x8000,job->static_end)||
        within(at,size,job->stack_pointer,0xfc00)||heap_span(at,size);
}
/* Only typed/untyped data transfers opt into the experimental low interval.
 * File blocks, descriptors, paths and Text metadata retain readable() checks. */
#ifdef ZP_COMPACT_EXPERIMENT
static uint8_t transfer_span(uint16_t at,uint16_t size) {
    return within(at,size,0x6000u,0x8000u)||readable(at,size);
}
static uint8_t copy_data(uint16_t address,uint8_t count,uint8_t writing) {
    if(address<0x8000u) {
        FileLowCopy request;
        request.address=address;request.buffer=bytes;
        request.count=count;request.writing=writing;
        if(zf_low_copy(&request)) {fail(0x1ff,0);return 0;}
        return 1;
    }
    if(writing)memcpy(bytes,USER(address),count);
    else memcpy(USER(address),bytes,count);
    return 1;
}
#else
#define transfer_span readable
#endif
static uint8_t descriptor(uint16_t *family,uint16_t *width) {
    if(!within(job->descriptor,3,job->code_origin,job->static_end)) return 0;
    *family=USER(job->descriptor)[0]; *width=word(job->descriptor+1);
    return *family<=2&&(*family!=0||!*width)&&(*family!=2||*width==128);
}
static void initialize(uint16_t family,uint16_t width) {
    memset(record,0,sizeof(*record));
    record->marker=FILE_MARKER;record->family=family;record->width=width;
    record->fd=0xffffu;
}
static uint8_t bound(uint16_t family,uint16_t width) {
    return record->marker==FILE_MARKER&&record->family==family&&record->width==width&&
        memchr(record->path,0,sizeof(record->path))!=0;
}
static void flags(void) {
    uint8_t type=record->device==ZF_STANDARD_KBD?2:record->device==ZF_STANDARD_LST?3:
        record->device==ZF_STANDARD_AUX?4:record->device==ZF_STANDARD_USR?5:record->device?1:0;
    record->flags=type|((record->state&READABLE)?0x80:0)|((record->state&WRITABLE)?0x40:0)|
        ((record->state&(record->family?DIRTY:LOOKAHEAD))?0x20:0)|
        (record->family==1?0x10:0);
}
static uint8_t device_access(uint16_t identity) {
    return identity==ZF_STANDARD_KBD?READABLE:identity==ZF_STANDARD_LST?WRITABLE:READABLE|WRITABLE;
}
/* Compiler-defined standard streams are the only preassigned file blocks.
 * Validate the bounded immutable table before initializing any of them. */
static uint8_t standards(void) {
    uint16_t table=job->standard_table,count,i,j,at,object,identity,saved=job->object;
    uint8_t identities=0;
    if(!table)return 1;
    if(!within(table,2,job->code_origin,job->static_end))goto invalid;
    count=word(table);
    if(count>ZF_STANDARD_USR||!within(table+2,count*4,job->code_origin,job->static_end))goto invalid;
    for(i=0;i<count;++i) {
        at=table+2+4*i;object=word(at);identity=word(at+2);
        if(!identity||identity>ZF_STANDARD_USR||(identities&(1u<<(identity-1)))||
           !within(object,ZF_FILE_BYTES,0x8000,job->code_origin))goto invalid;
        identities|=1u<<(identity-1);
        for(j=0;j<i;++j)if(word(table+2+4*j)==object)goto invalid;
    }
    for(i=0;i<count;++i) {
        at=table+2+4*i;object=word(at);record=(Record *)USER(object);identity=word(at+2);
        if(bound(0,0)&&record->standard==identity)continue;
        initialize(0,0);record->standard=identity;
        record->device=identity>=ZF_STANDARD_KBD?identity:0;
        record->state=device_access(identity);flags();
    }
    record=(Record *)USER(saved);return 1;
invalid:
    fail(0x1ff,0);return 0;
}
static uint8_t standard_identity(void) {
    uint16_t table=job->standard_table,count,i;
    if(!table)return 0;
    count=word(table);
    for(i=0;i<count;++i)if(word(table+2+4*i)==job->object)return (uint8_t)word(table+4+4*i);
    return 0;
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
    uint8_t error=write((int8_t)native_fd,data,size);
    int32_t next=0,end=0,restored;
    uint32_t advanced;
    /* Serial has no file cursor/size metadata. Never reinterpret a real device
     * error through the disk-only post-write bookkeeping workaround. */
    if(record->device) return error;
    if(!error) {
        if(*size<=wanted) native_position+=*size;
        return 0;
    }
    if(!wanted||seek((int8_t)native_fd,&next,SEEK_CUR)) return error;
    if((uint32_t)next<=native_position) return error;
    advanced=(uint32_t)next-native_position;
    if(advanced>wanted) return error;
    if(seek((int8_t)native_fd,&end,SEEK_END)) return error;
    restored=next;
    if(seek((int8_t)native_fd,&restored,SEEK_SET)||restored!=next) return error;
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
#ifdef ZP_COMPACT_EXPERIMENT
        if(writing&&!copy_data(address+done,(uint8_t)n,1))break;
#else
        if(writing)memcpy(bytes,USER(address+done),n);
#endif
        size=n;
        error=writing?write_bytes(bytes,&size):read((int8_t)native_fd,bytes,&size);
        /* The SDK leaves size unchanged on error; it is not a transferred
         * byte count. Never copy stale input or claim completion from it. */
        if(error) { native_error(error,writing); break; }
        if(size>n) { fail(writing?0xf0:0x99,ERR_INVALID_PARAMETER); break; }
#ifdef ZP_COMPACT_EXPERIMENT
        if(!writing&&size&&!copy_data(address+done,(uint8_t)size,0))break;
#else
        if(!writing&&size)memcpy(USER(address+done),bytes,size);
#endif
        if(!writing) native_position+=size;
        done+=size;
        if(!size) { if(writing) fail(0xf0,0); break; }
    }
    return done;
}
static uint8_t seek_file(uint32_t at) {
    uint32_t requested=at;
    uint8_t error=seek((int8_t)native_fd,(int32_t *)&at,SEEK_SET);
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
        error=writing?write_bytes(bytes+done,&size):read((int8_t)native_fd,bytes+done,&size);
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
/* Resume never recreates/truncates or rereads Pascal's unfinalized header. */
static uint8_t disk_open(uint8_t writing) {
    if(native_fd>=0)return 1;
    /* Read-only resume avoids ZealFS size writeback on close. Pascal access
     * permissions remain in state and are checked before acquisition. */
    native_fd=open_file(writing?O_WRONLY:O_RDONLY);
    if(native_fd<0) {native_error((uint8_t)-native_fd,writing);return 0;}
    if(!record->family) {text_position();return seek_file(native_position);}
    return 1;
}
static void disk_release(void) {
    uint8_t error;
    if(native_fd<0)return;
    error=close_file((uint8_t)native_fd);native_fd=-1;
    if(error)fail(error==ERR_NO_SUCH_ENTRY?0xff:error==ERR_CANNOT_REGISTER_MORE?0xf3:0xf0,error);
}
static uint8_t serial_open(void) {
    int8_t fd;
    if(native_fd>=0) return 1;
    fd=open_path("#SER0",O_RDWR);
    if(fd<0) { fail(-fd==ERR_CANNOT_REGISTER_MORE?0xf3:0x20,(uint8_t)-fd); return 0; }
    native_fd=(uint8_t)fd; return 1;
}
static uint8_t text_peek(void) {
    uint16_t size=1;
    uint8_t error;
    if(!(record->state&LOOKAHEAD)) {
        if(record->device&&!serial_open()) return 26;
        if(!record->device&&!disk_open(0))return 26;
        error=read((int8_t)native_fd,bytes,&size);
        if(error) { native_error(error,0); return 26; }
        if(size>1) { fail(0x99,ERR_INVALID_PARAMETER); return 26; }
        if(!size&&record->device) { fail(0x99,0); return 26; }
        if(!record->device) {native_position+=size;text_remember();}
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
        c=text_take(); if(!job->error) {
            if(record->device) record->state&=(uint16_t)~LOOKAHEAD;
            USER(out)[0]=c; job->actual=1;
        }
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
    if(c==13&&!record->device&&!job->error&&text_peek()==10) text_take();
}
static void text_write(uint8_t newline) {
    uint16_t n;
    if(!record->device&&!disk_open(1))return;
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
        if(disk_open(1)&&header(1)) record->state&=(uint16_t)~DIRTY;
    }
}
static void close_record(void) {
    if(record->device||!(record->state&OPENED))return;
    if((!record->family&&(record->state&WRITABLE))||
       (record->family==1&&(record->state&DIRTY))) {
        /* A failed acquisition has not attempted finalization. Keep the FIB
         * open so an unchecked caller can retrieve IOResult and retry Close. */
        if(!disk_open(1))return;
        if(!record->family) {bytes[0]=26;text_bytes(1);text_remember();}
        else flush();
    }
    record->state=0;
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
    error=stat_file();
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
    native_fd=fd; record->position=0; record->count=0;
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
    if(!transfer_span(job->data,size)) { fail(0x1ff,0); return; }
    if(!writing&&record->position>=record->count) { fail(0x99,0); return; }
    if(writing&&record->position==65535u) { fail(0xf2,0); return; }
    if(!disk_open(writing)||!seek_file(4ul+(uint32_t)record->position*size)) return;
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
    if(size>65535u||!transfer_span(job->data,(uint16_t)size)) { fail(0x1ff,0); return; }
    if((uint32_t)record->position+job->count>65535u) { fail(0xf2,0); return; }
    if(!disk_open(writing)||!seek_file((uint32_t)record->position*128u)) return;
    for(i=0;i<job->count;++i) {
        if(transfer(job->data+i*128u,128,writing)!=128) break;
        ++record->position; ++job->actual;
        if(writing&&record->position>record->count) record->count=record->position;
    }
    if(!partial&&job->actual!=job->count&&!job->error) fail(writing?0xf0:0x99,0);
}
/* Logical names bypass path normalization. Their fixed canonical spellings are
 * shared by Assign/Rename recognition; preassigned authority is still separate. */
static uint8_t logical_device(const char *path) {
    if(!strcmp(path,"KBD:")) return ZF_STANDARD_KBD;
    if(!strcmp(path,"CON:")) return ZF_STANDARD_CON;
    if(!strcmp(path,"TRM:")) return ZF_STANDARD_TRM;
    if(!strcmp(path,"LST:")) return ZF_STANDARD_LST;
    if(!strcmp(path,"AUX:")) return ZF_STANDARD_AUX;
    if(!strcmp(path,"USR:")) return ZF_STANDARD_USR;
    return 0;
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
    memcpy(transport.request.path,USER(job->data+1),n); transport.request.path[n]=0;
    if(n==4&&transport.request.path[3]==':') {
        memcpy(bytes,transport.request.path,5);
        for(n=0;n<3;++n) if(bytes[n]>='a'&&bytes[n]<='z') bytes[n]-=32;
        if(logical_device((char*)bytes)) { strcpy(transport.request.path,(char*)bytes); return 1; }
    }
    bytes[0]=0;
    if(n<2||transport.request.path[1]!=':') {
        error=curdir((char*)bytes);
        if(error) { fail(0x20,error); return 0; }
    }
    return path_resolve(transport.request.path,(char*)bytes,transport.request.path);
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
    if(logical_device(transport.request.path)) { fail(0x20,ERR_INVALID_PATH); return; }
    strcpy(rename_path,transport.request.path);
    error=stat(rename_path,&info);
    if(!error) { fail(0x20,ERR_ALREADY_EXIST); return; }
    if(error!=ERR_NO_SUCH_ENTRY) { native_error(error,2); return; }
    error=stat_file();
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
        rename_read(in,bytes,n); rename_read(out,transport.verify_bytes,n);
        if(!job->error&&memcmp(bytes,transport.verify_bytes,n)) fail(0xf0,0);
        left-=n;
    }
    if(!job->error) { rename_end(in); if(!job->error) rename_end(out); }
    rename_close(out); rename_close(in);
    if(job->error) return;
    error=remove_file();
    if(error) native_error(error,2);
    else strcpy(record->path,rename_path);
}
/* The native default Aux adapter owns one supported SER0 descriptor per
 * operation. Logical Close remains a no-op; only the adapter closes its own
 * transient handle. Input opens lazily: an already owned current byte needs no
 * descriptor or live device. Preserve the global driver's raw/cooked setting,
 * including after failed output. All SDK buffers live in this module prefix. */
static void serial_stream(void) {
    int8_t fd;
    uint8_t writing=job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN;
    uint8_t error,restore=0;
    if(job->operation==ZF_TEXT_BEGIN&&!(job->count&2)) return;
    if(job->operation==ZF_TEXT_WRITE&&!USER(job->data)[0]) return;
    native_fd=-1;
    if(writing) {
        if(!serial_open()) { native_fd=-1; return; }
        fd=(int8_t)native_fd;
        error=ioctl(fd,SERIAL_CMD_GET_ATTR,&serial_attribute);
        if(!error) {
            restore=1;
            error=ioctl(fd,SERIAL_CMD_SET_ATTR,(void *)(uintptr_t)(serial_attribute|SERIAL_ATTR_MODE_RAW));
        }
        if(error) fail(0x20,error);
    }
    if(!job->error) switch(job->operation) {
    case ZF_TEXT_BEGIN: text_peek(); break;
    case ZF_TEXT_READ: text_read(); break;
    case ZF_TEXT_READLN: text_readln(); break;
    case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN:
        text_write(job->operation==ZF_TEXT_WRITELN); break;
    default: text_predicate(job->operation); break;
    }
    if(restore) {
        fd=(int8_t)native_fd;
        error=ioctl(fd,SERIAL_CMD_SET_ATTR,(void *)(uintptr_t)serial_attribute);
        if(error) fail(0xf0,error);
    }
    if(native_fd>=0) {
        error=close_file((uint8_t)native_fd);
        if(error) fail(0xf0,error);
    }
    native_fd=-1;
}
/* The file module validates ownership while mapped; the restored prefix owns
 * keyboard services. Only the authenticated binding's small lookahead state
 * crosses that handoff. Logical Close owns no native descriptor and is a no-op. */
static void logical_stream(void) {
    switch(job->operation) {
    case ZF_RESET: case ZF_REWRITE:
        record->state=device_access(record->device);
        record->next=0;  return;
    case ZF_CLOSE: return;
    case ZF_TEXT_BEGIN: case ZF_TEXT_READLN: case ZF_EOF:
    case ZF_EOLN: case ZF_SEEKEOF: case ZF_SEEKEOLN:
        if(!access(0)) return;
        break;
    case ZF_TEXT_READ:
        if(!access(0)||!text_input_target()) return;
        break;
    case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN:
        if(!access(1)||(job->operation==ZF_TEXT_WRITE&&!text_output_value())) return;
        break;
    default: fail(0x20,0); return;
    }
    if((job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN)&&
       !(job->count&ZF_OUTPUT_NATIVE)&&(job->count&ZF_OUTPUT_OVERRIDES)&&
       ((record->device==ZF_STANDARD_LST&&(job->count&ZF_OUTPUT_LST))||
        (record->device==ZF_STANDARD_AUX&&(job->count&ZF_OUTPUT_AUX)))) {
        job->stream=record->device; return;
    }
    if(record->device==ZF_STANDARD_LST) { fail(0x20,ERR_NOT_SUPPORTED); return; }
    if(record->device==ZF_STANDARD_AUX) { serial_stream();  return; }
    job->stream=record->device;
    job->lookahead=(uint8_t)record->next|((record->state&LOOKAHEAD)?0x100u:0);
}
static void dispatch(FileJob *invocation) {
    uint16_t family,width;
    uint8_t valid,error;
    job=invocation;record=(Record *)USER(job->object);
    if(job->operation!=ZF_STREAM_RETURN) {
        job->error=0; job->detail=0; job->result=0; job->actual=0; job->stream=0; job->lookahead=0;
    }
    if(job->static_end<0x8000u||job->static_end>job->heap_floor||
       job->heap_floor>job->heap_frontier||job->heap_frontier>job->stack_pointer||
       job->stack_pointer<0xc800u||job->stack_pointer>0xfc00u||
       job->code_origin<0x8000u||job->code_origin>job->static_end||
       (job->heap_floor&1)||!descriptor(&family,&width)||!readable(job->object,ZF_FILE_BYTES)) {
        fail(0x1ff,0); return;
    }
    if(!standards()) return;
    valid=bound(family,width);
    if(valid&&record->standard!=standard_identity()) {
        if(job->operation==ZF_ASSIGN&&!standard_identity())valid=0;
        else {fail(0x1ff,0);return;}
    }
    if(job->operation==ZF_STREAM_RETURN) {
        if(!valid||record->device<ZF_STANDARD_KBD||record->device>ZF_STANDARD_USR||
           record->device==ZF_STANDARD_LST||record->device==ZF_STANDARD_AUX||
           job->stream!=record->device||job->lookahead>0x1ffu)
            fail(0x1ff,0);
        else {
            record->next=(uint8_t)job->lookahead;
            record->state=(record->state&~LOOKAHEAD)|((job->lookahead&0x100u)?LOOKAHEAD:0);
            flags();
        }
        return;
    }
    if(valid&&record->standard) {
        if(job->operation==ZF_ASSIGN||job->operation==ZF_RESET||job->operation==ZF_REWRITE||job->operation==ZF_CLOSE)
            fail(0x22,0);
        else if(record->device) logical_stream();
        else if(job->operation==ZF_TEXT_READ) {
            if(text_input_target()) job->stream=record->standard;
        } else if(job->operation==ZF_TEXT_WRITE) {
            if(text_output_value()) job->stream=record->standard;
        } else if(job->operation==ZF_TEXT_BEGIN||job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_WRITELN||
                  job->operation==ZF_EOF||(job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN))
            job->stream=record->standard;
        else fail(0x20,0);
        flags();return;
    }
    if(job->operation==ZF_ASSIGN) {
        if(!assigned_path()) { fail(0x20,ERR_INVALID_PATH); return; }
        error=logical_device(transport.request.path);
        if(error&&family) { fail(0x20,0); return; }
        initialize(family,width);
        record->device=error; record->state=error?device_access(error):0; record->next=0;
        strcpy(record->path,transport.request.path);flags();return;
    }
    if(!valid) {
        fail(family==2?4:job->operation==ZF_READ||job->operation==ZF_TEXT_READ||
             job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_BEGIN||job->operation==ZF_EOF||
             (job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN)?2:3,0); return;
    }
    if(record->device) { logical_stream();flags();return; }
    /* No descriptor is ever read from the copied/forged FIB slot. */
    switch(job->operation) {
    case ZF_TRANSFER_PATH: case ZF_TRANSFER_EXECUTE:
        width=(uint16_t)strlen(record->path)+1;
        if(job->count<width||!readable(job->data,width)) fail(0x20,0);
        else {memcpy(USER(job->data),record->path,width);job->actual=width-1;}
        break;
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
            error=stat_file();
            if(error) native_error(error,2);
            else if(!D_ISFILE(info.s_flags)) fail(0x20,0);
            else { error=remove_file(); if(error) native_error(error,2); }
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
    flags(); /* Reflect validated state changes even if transfer/close failed. */
}
void zf_dispatch(FileJob *invocation) ZFILE_CALL {
    native_fd=-1;
    dispatch(invocation);
    disk_release();
}
/* All native descriptors are released before returning to Pascal. This retained
 * ABI boundary performs no implicit Pascal Close or walk of dead variables. */
void zf_cleanup(FileCleanup *ending) ZFILE_CALL {
    ending->error=ending->detail=0;
}
