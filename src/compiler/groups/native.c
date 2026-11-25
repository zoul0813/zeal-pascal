#include "compiler/groups/native.h"
#ifdef ZP_OVERLAYS
#include "compiler/groups/groups.h"
#include "compiler/link/link.h"
#include "compiler/link/layout.h"
#include "compiler/disk.h"
#include "platform/banks/loader.h"
#include "storage/transport.h"
#include "common/path.h"
#include "overlay_asset.h"
#include "units/internal.h"
#include <zos_vfs.h>
#include <zos_errors.h>
#ifdef __SDCC
#pragma codeseg BANK20
#pragma constseg BANK20
#endif
/* All addresses below describe preferred code, until sealed. Metadata uses
 * the compiler's growing arena; immutable snapshots survive domain switches. */
#define RECORD_BYTES 40u
enum { M_SCOPE,M_GROUP=2,M_PARENT=4,M_ENTRY=6,M_RESET=8,M_END=10,
       M_MAPS=12,M_RELOCS=14,M_LINKS=16,M_LATES=18,M_AT=20,M_LENGTH=24,M_HASH=28,
       M_FILE=32,M_CHECK=36,M_UNSIGNED=38 };
struct CompilerOverlays {
    const Build *owner;CompilerGroups late;
    uint16_t members,dispatch,cleanup,directory,root_origin,groups;
    uint8_t page,sealed,published;
    int8_t fd;uint32_t length,catalog;
    char folder[128],spool[128],base[12];
};
static struct CompilerOverlays overlays;
uint8_t overlay_build_owned;

static uint16_t generation;
static uint8_t source_cache[128],source_cached;
static uint32_t source_cache_at;
static uint8_t error(void) {if(!overlays.sealed&&cc.error&&!cc.error->code)cc_fail(COMPILE_PLATFORM_FAILURE);if(!platform_error)platform_error=ERR_FAILURE;return 0;}
static uint8_t seek_to(uint32_t at) {
    int32_t offset=(int32_t)at;platform_error=seek(overlays.fd,&offset,SEEK_SET);
    return !platform_error&&(uint32_t)offset==at?1:error();
}
uint8_t cc_overlay_source_store(const Source *source,uint16_t base,uint32_t *offset) FRONTEND {
    uint16_t at,i,n;uint8_t bytes[128];
    *offset=overlays.length;
    if(!seek_to(*offset))return 0;
    for(at=0;at<source->length;at+=n) {
        if(!cc_poll())return 0;
        n=source->length-at;if(n>sizeof(bytes))n=sizeof(bytes);
        for(i=0;i<n;++i)bytes[i]=buffer_view_get(source->page,base,at+i);
        if(!output_write_buffer(overlays.fd,bytes,n))return error();
    }
    overlays.length+=source->length;return 1;
}
uint8_t cc_overlay_source_byte(uint32_t offset) FRONTEND {
    uint16_t n;
    if(offset>=overlays.length){cc_fail(99);return 0;}
    if(!source_cached||offset<source_cache_at||offset>=source_cache_at+source_cached) {
        source_cache_at=offset&~127UL;n=overlays.length-source_cache_at;
        if(overlays.length-source_cache_at>128u)n=128;
        if(!seek_to(source_cache_at)||!output_read_buffer(overlays.fd,source_cache,n))return error();
        source_cached=(uint8_t)n;
    }
    return source_cache[(uint8_t)(offset-source_cache_at)];
}
static void read_record(uint16_t id,uint8_t *r) {
    if(overlays.sealed) {if(!seek_to(overlays.catalog+(uint32_t)id*RECORD_BYTES)||!output_read_buffer(overlays.fd,r,RECORD_BYTES))memset(r,0,RECORD_BYTES);}
    else cc_overlay_record_read((uint32_t)id*RECORD_BYTES,r,RECORD_BYTES);
}
static void write_record(uint16_t id,const uint8_t *r) {
    uint8_t i;for(i=0;i<RECORD_BYTES;++i)cc_overlay_record_put((uint32_t)id*RECORD_BYTES+i,r[i]);
}
#ifdef __SDCC
#pragma save
#pragma disable_warning 85 /* Parameters are consumed by the assembly ABI. */
/* SDCC call1: record pointer in HL, byte offset on the stack. Word/void
 * results pop stack arguments;32-bit results leave their byte for the caller.
 * IX/IY are untouched. These helpers only visit their checked local record. */
static uint16_t word(const uint8_t *r,uint8_t at) NATIVE __naked {
    __asm
        ld c,l
        ld b,h
        ld hl,#2
        add hl,sp
        ld a,(hl)
        ld l,a
        ld h,#0
        add hl,bc
        ld e,(hl)
        inc hl
        ld d,(hl)
        pop hl
        inc sp
        jp (hl)
    __endasm;
}
static uint32_t dword(const uint8_t *r,uint8_t at) NATIVE __naked {
    __asm
        ld c,l
        ld b,h
        ld hl,#2
        add hl,sp
        ld a,(hl)
        ld l,a
        ld h,#0
        add hl,bc
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        ret
    __endasm;
}
static void put16(uint8_t *r,uint8_t at,uint16_t n) NATIVE __naked {
    __asm
        ld c,l
        ld b,h
        ld hl,#2
        add hl,sp
        ld e,(hl)
        ld d,#0
        inc hl
        ex de,hl
        add hl,bc
        ld a,(de)
        ld (hl),a
        inc de
        inc hl
        ld a,(de)
        ld (hl),a
        pop hl
        pop af
        inc sp
        jp (hl)
    __endasm;
}
static void put32(uint8_t *r,uint8_t at,uint32_t n) NATIVE __naked {
    __asm
        ld c,l
        ld b,h
        ld hl,#2
        add hl,sp
        ld e,(hl)
        ld d,#0
        inc hl
        ex de,hl
        add hl,bc
        ld a,(de)
        ld (hl),a
        inc de
        inc hl
        ld a,(de)
        ld (hl),a
        inc de
        inc hl
        ld a,(de)
        ld (hl),a
        inc de
        inc hl
        ld a,(de)
        ld (hl),a
        pop hl
        pop af
        pop af
        inc sp
        jp (hl)
    __endasm;
}
#pragma restore
#else
static uint16_t word(const uint8_t *r,uint8_t at) {r+=at;return r[0]|((uint16_t)r[1]<<8);}
static uint32_t dword(const uint8_t *r,uint8_t at) {r+=at;return (uint32_t)r[0]|((uint32_t)r[1]<<8)|((uint32_t)r[2]<<16)|((uint32_t)r[3]<<24);}
static void put16(uint8_t *r,uint8_t at,uint16_t n) {r+=at;r[0]=(uint8_t)n;r[1]=(uint8_t)(n>>8);}
static void put32(uint8_t *r,uint8_t at,uint32_t n) {r+=at;r[0]=(uint8_t)n;r[1]=(uint8_t)(n>>8);r[2]=(uint8_t)(n>>16);r[3]=(uint8_t)(n>>24);}
#endif
void cc_overlay_put(uint16_t at,uint8_t value) FRONTEND {
    if(at<CC_CODE_BASE||at>=0xc000u||overlays.page==255){cc_fail(99);return;}
    far_set(overlays.page,at-CC_CODE_BASE,value);
}
uint8_t cc_overlay_get(uint16_t at) FRONTEND {return far_get(overlays.page,at-CC_CODE_BASE);}
static uint8_t snapshot_byte(uint16_t bank,uint32_t at) {
    uint32_t n=(uint32_t)(cc.emitted-CC_CODE_BASE);uint16_t i;
    if(bank) {if(at<n)return cc_overlay_get(CC_CODE_BASE+(uint16_t)at);at-=n;}
    n=(uint32_t)cc.entries*MAP_RECORD_BYTES;
    if(at<n)return buffer_get(cc.image->map_page,(uint16_t)at);at-=n;
    n=(uint32_t)cc.relocations*RELOCATION_BYTES;
    if(at<n)return cc_reloc_get((uint16_t)at);at-=n;
    n=(uint32_t)cc.links*CC_LINK_BYTES;
    if(at<n) {uint8_t r[8];i=(uint16_t)(at/8);cc_link_read(i,r);return r[(uint8_t)(at%8)];}at-=n;
    return cc_late_byte(&overlays.late.late,(uint16_t)(at/8),(uint8_t)(at%8));
}
static uint8_t save_domain(void) {
    uint8_t r[RECORD_BYTES],bytes[128];uint16_t i,n,bank=cc.sources->bank;
    uint32_t total,at,hash=2166136261UL;
    read_record(bank,r);put16(r,M_END,cc.emitted);put16(r,M_MAPS,cc.entries);
    /* This eager helper contains member-relative internal addresses. Its cache
     * belongs to the code domain, unlike deferred resident runtime helpers. */
    put16(r,M_UNSIGNED,cc.unsigned_entry);
    put16(r,M_RELOCS,cc.relocations);put16(r,M_LINKS,cc.links);put16(r,M_LATES,overlays.late.late.references);
    total=(bank?cc.emitted-CC_CODE_BASE:0)+(uint32_t)cc.entries*8u+(uint32_t)cc.relocations*4u+
        (uint32_t)cc.links*8u+(uint32_t)overlays.late.late.references*8u;
    if(!seek_to(overlays.length))return 0;
    put32(r,M_AT,overlays.length);put32(r,M_LENGTH,total);
    for(at=0;at<total;) {
        n=total-at>128?128:(uint16_t)(total-at);
        for(i=0;i<n;++i,++at){bytes[i]=snapshot_byte(bank,at);hash=(hash^bytes[i])*16777619UL;}
        if(cc.error->code||!output_write_buffer(overlays.fd,bytes,n))return error();
    }
    put32(r,M_HASH,hash);write_record(bank,r);overlays.length+=total;
    return !cc.error->code;
}
static uint8_t load_domain(uint16_t bank) {
    uint8_t r[RECORD_BYTES],bytes[128],item[8],used=0;uint16_t i,n;
    uint32_t at,total,code,maps,relocs,links,hash=2166136261UL;
    overlays.late.late.sealed=0;
    if(bank&&overlays.page==255) {
        overlays.page=page_new(PAGE_COMPILER);if(overlays.page==255)return error();
    }
    read_record(bank,r);total=dword(r,M_LENGTH);code=bank?word(r,M_END)-CC_CODE_BASE:0;
    maps=code+(uint32_t)word(r,M_MAPS)*8u;relocs=maps+(uint32_t)word(r,M_RELOCS)*4u;
    links=relocs+(uint32_t)word(r,M_LINKS)*8u;
    if(total!=links+(uint32_t)word(r,M_LATES)*8u||!cc_late_retire(&overlays.late.late,0)||!seek_to(dword(r,M_AT)))return error();
    cc.unsigned_entry=word(r,M_UNSIGNED);
    cc.sources->bank=bank;cc.emitted=word(r,M_END);cc.entries=word(r,M_MAPS);cc.relocations=word(r,M_RELOCS);cc.links=0;
    if(!buffer_reserve(cc.image->map_page,cc.entries*8u))return error();
    for(at=0;at<total;) {
        n=total-at>128?128:(uint16_t)(total-at);
        if(!output_read_buffer(overlays.fd,bytes,n))return error();
        for(i=0;i<n;++i,++at) {
            uint8_t b=bytes[i];hash=(hash^b)*16777619UL;
            if(at<code)cc_overlay_put(CC_CODE_BASE+(uint16_t)at,b);
            else if(at<maps)buffer_set(cc.image->map_page,(uint16_t)(at-code),b);
            else if(at<relocs)cc_reloc_put((uint16_t)(at-maps),b);
            else {
                /* Both journals contain complete eight-byte records, so one
                 * assembly cursor spans block and journal boundaries. */
                item[used++]=b;
                if(used==8) {
                    used=0;
                    if(at<links)cc_link_reference(word(item,0),item[6],word(item,2),word(item,4),item[7]);
                    else if(!cc_late_reference(&overlays.late.late,word(item,0),word(item,2),word(item,4),item[6]))return error();
                }
            }
        }
    }
    if(hash!=dword(r,M_HASH)||cc.error->code)return error();
    return 1;
}
uint8_t cc_overlay_switch(uint16_t bank) ROUTINES {
    if(cc.sources->bank==bank)return 1;
    if(!save_domain())return 0;
    if(!load_domain(bank))return 0;
    /* Resident code uses the disk image, so its inactive member window can
     * lend RAM to source loading and shared declaration metadata. */
    if(!bank&&overlays.page!=255) {
        if(!page_drop(overlays.page))return error();overlays.page=255;
    }
    return 1;
}
uint8_t cc_overlays_begin(void) ROUTINES {
    char parent[128];zos_stat_t info;uint16_t n,i;uint8_t e,r[RECORD_BYTES];
    if(overlays.owner)return error();
    memset(&overlays,0,sizeof(overlays));overlays.page=255;overlays.fd=-1;source_cached=0;
    cc_groups_init(&overlays.late);overlays.owner=cc.image;overlay_build_owned=1;
    if(!path_program(cc.sources->root->path,parent,0))return error();
    n=(uint16_t)strlen(parent);while(n&&parent[n-1]!='/')--n;
    if(n+7u>127u)return error();parent[n]=0;
    for(i=0;i<65535u;++i) {
        static const char hex[]="0123456789ABCDEF";
        overlays.base[0]='Z';overlays.base[1]='P';
        overlays.base[2]=hex[generation>>12];overlays.base[3]=hex[(generation>>8)&15];
        overlays.base[4]=hex[(generation>>4)&15];overlays.base[5]=hex[generation&15];overlays.base[6]=0;++generation;
        strcpy(parent+n,overlays.base);
        /* Existing generations are immutable. HostFS can report a generic
         * mkdir failure for EEXIST, so identify occupied names with stat.
         * Do not give cleanup ownership until creation actually succeeds. */
        e=stat(parent,&info);if(!e)continue;
        if(e!=ERR_NO_SUCH_ENTRY){platform_error=e;return error();}
        e=mkdir(parent);if(!e){strcpy(overlays.folder,parent);break;}
        if(e==ERR_ALREADY_EXIST||!stat(parent,&info))continue;
        platform_error=e;return error();
    }
    if(i==65535u)return error();
    strcpy(overlays.spool,overlays.folder);strcat(overlays.spool,"/TEMP");strcat(overlays.base,"/P");
    overlays.fd=open(overlays.spool,O_RDWR|O_CREAT);if(overlays.fd<0){platform_error=(uint8_t)-overlays.fd;overlays.fd=-1;return error();}
    cc.sources->overlays=&overlays;cc.sources->groups=&overlays.late;
    if(!cc_sources_spool())return 0;
    memset(r,0,sizeof(r));put16(r,M_END,CC_CODE_BASE);put32(r,M_HASH,2166136261UL);write_record(0,r);
    cc.image->globals_bytes=OVERLAY_RESIDENT_BYTES;
    n=cc_forward(0xc3);overlays.dispatch=cc.emitted;cc_address(0xc3,0);
    overlays.cleanup=cc.emitted;cc_address(0xc3,0);cc_patch(n);
    return !cc.error->code;
}
uint8_t cc_overlay_group(void) ROUTINES {
    if(overlays.groups==100){cc_fail(99);return 255;}return (uint8_t)overlays.groups++;
}
uint16_t cc_overlay_member(uint8_t group,uint16_t scope) ROUTINES {
    uint8_t r[RECORD_BYTES];uint16_t bank=++overlays.members,parent=cc.sources->bank;
    if(overlays.members>4094u){cc_fail(99);return 0;}
    memset(r,0,sizeof(r));put16(r,M_SCOPE,scope);put16(r,M_GROUP,group);put16(r,M_PARENT,parent?parent-1:65535u);
    put16(r,M_END,CC_CODE_BASE);put32(r,M_HASH,2166136261UL);write_record(bank,r);
    cc_rput(scope,R_BANK,bank);if(!cc_overlay_switch(bank))return 0;
    if(!cc_rget(scope,R_ENTRY))cc_overlay_alias(scope);
    return bank;
}
void cc_overlay_alias(uint16_t scope) ROUTINES {
    uint16_t bank=cc.sources->bank,entry;SourcePosition position;
    if(!cc_overlay_switch(0))return;
    entry=cc.emitted;cc_rput(scope,R_ENTRY,entry);cc_rput(scope,R_CALLS,cc_link(0xc3,0));
    position=cc_reference_position(CCS(cc_rget(scope,R_SYMBOL),name_start));
    cc_mapping(entry-CC_CODE_BASE,cc.emitted-CC_CODE_BASE,position,position);
    cc_overlay_switch(bank);
}
/* Batch fixed opcodes; relocation-bearing operands still use cc_address. */
static void emit_bytes(const char *bytes,uint8_t count) {while(count--)cc_emit((uint8_t)*bytes++);}
#define EMIT_FIXED(code) emit_bytes(code,(uint8_t)(sizeof(code)-1u))
static void dispatch(uint8_t op,uint16_t value) {
    cc_op16(0x21,value);cc_emit(0x3e);cc_emit(op);cc_address(0xcd,overlays.dispatch);
}
void cc_overlay_wrappers(uint16_t bank) ROUTINES {
    uint16_t old=cc.sources->bank,scope,saved=cc.scope,entry,body,reset,skip;uint8_t r[RECORD_BYTES];
    SourcePosition position;
    if(!cc_overlay_switch(bank))return;
    for(scope=1;scope<=cc.routine_count&&!cc.error->code;++scope)
        if(cc_rget(scope,R_BANK)==bank&&cc_rget(scope,R_LOCAL_CALLS)) {
            cc.scope=scope;entry=cc.emitted;
            cc_patch_chain(cc_rget(scope,R_LOCAL_CALLS),entry);cc_rput(scope,R_LOCAL_CALLS,0);
            cc_rput(scope,R_LOCAL_ENTRY,entry);
            cc_frame_begin();cc_reserve(4);
            /* Raw Exit unwinds past the mapping word and body return. Keep
             * that same four-byte body ABI when no bank change is needed. */
            cc_emit(0xf5);cc_op16(0xcd,cc_rget(scope,R_BODY));cc_emit(0xf1);cc_frame_end();
            position=cc_reference_position(CCS(cc_rget(scope,R_SYMBOL),name_start));
            cc_mapping(entry-CC_CODE_BASE,cc.emitted-CC_CODE_BASE,position,position);
        }
    if(!cc_overlay_switch(0))return;read_record(bank,r);reset=word(r,M_RESET);
    for(scope=1;scope<=cc.routine_count&&!cc.error->code;++scope)if(cc_rget(scope,R_BANK)==bank&&
        cc_rget(scope,R_ENTRY)&&!(cc_rget(scope,R_FLAGS)&RF_EXTERNAL)) {
        cc.scope=scope;entry=cc.emitted;cc_patch_chain(cc_rget(scope,R_CALLS),entry);cc_rput(scope,R_CALLS,0);
        cc_frame_begin();cc_reserve(512);
        EMIT_FIXED("\x3e\x80\xdb\xf2\xf5");
        dispatch(0,bank-1);cc_emit(0xd3);cc_emit(0xf2);
        if(reset) {cc_emit(0x79);cc_emit(0xb7);skip=cc_forward(0xca);cc_op16(0xcd,reset);cc_patch(skip);}
        body=cc_rget(scope,R_BODY);cc_op16(0xcd,body);
        EMIT_FIXED("\xf1\xd3\xf2");
        dispatch(1,bank-1);cc_frame_end();
    }
    cc.scope=saved;cc_overlay_switch(old);
}
void cc_overlay_finish_member(uint16_t bank) ROUTINES {
    uint8_t r[RECORD_BYTES];read_record(bank,r);put16(r,M_ENTRY,cc_rget(word(r,M_SCOPE),R_BODY));write_record(bank,r);
}
uint16_t cc_overlay_reset(uint16_t entry) FRONTEND {
    uint8_t r[RECORD_BYTES];uint16_t old;read_record(cc.sources->bank,r);old=word(r,M_RESET);
    put16(r,M_RESET,entry);write_record(cc.sources->bank,r);return old;
}
void cc_overlay_unit_call(uint8_t opcode,uint16_t scope) FRONTEND {
    uint16_t bank=cc.sources->bank,proxy;
    if(!cc_overlay_switch(0))return;
    proxy=cc.emitted;cc_units_call(0xc3,scope);
    if(!cc_overlay_switch(bank))return;
    cc_overlay_address(opcode,proxy);
}
void cc_overlay_address(uint8_t op,uint16_t address) FRONTEND {
    if(!cc.sources->overlays){cc_address(op,address);return;}
    if(cc.sources->bank) {cc_link_reference(cc.emitted+1,CL_ROOT,0,address,CF_WORD16);cc_op16(op,address);}
    else cc_address(op,address);
}
void cc_overlay_pc(uint16_t at) FRONTEND {if(cc.sources->bank)cc_link_reference(at,CL_ROOT_ADJUST,65535u,0,CF_WORD16);}
void cc_overlay_chain(uint16_t operand,uint8_t root) FRONTEND {
    uint16_t symbol=cc_late_symbol(&overlays.late.late,LATE_ROOT,root,0);
    cc_word(operand,0);cc_link_reference(operand,CL_ROOT_ADJUST,1,0,CF_WORD16);
    if(symbol==LATE_NONE||!cc_late_reference(&overlays.late.late,operand,symbol,1,0))cc_fail(99);
}
void cc_overlay_ovrdrive(void) FRONTEND {
    cc_next();cc_expect('(',4);if(cc_family(cc_expression())!=K_INTEGER)cc_fail(23);cc_expect(')',5);cc_reserve(512);
    cc_emit(0x3e);cc_emit(2);cc_overlay_address(0xcd,overlays.dispatch);
}
#endif
#ifdef ZP_OVERLAYS
/* Runtime dispatch is resident with Pascal data/wrappers. It lends only page1
 * to the self-contained C module and always restores the original prefix. */
extern const uint8_t overlay_asset_start[];
/* The saved image's otherwise unused page2 carries the cold bootstrap.
 * Code/global staging remains independent; publication and image reads share
 * this mapping so the bytes that are checked are the bytes that are saved. */
uint8_t cc_overlay_image_byte(const Build *build,uint16_t offset) FRONTEND {
    if(overlays.owner!=build||offset>=OVERLAY_PACKED_BYTES)return 0;
    return overlay_asset_start[offset];
}
static void op_store(uint16_t address,uint16_t value) {cc_op16(0x21,value);cc_op16(0x22,address);}
#define OVERLAY_CONTROL 0xc000u
#define OVERLAY_JOB (OVERLAY_CONTROL+8u)
/* Control bytes: page, saved page1, saved page2, unpacked flag, cleanup B.
 * The fixed byte runs below encode this generated-program control ABI. */
_Static_assert(OVERLAY_CONTROL==0xc000u&&OVERLAY_JOB==0xc008u&&
    OVERLAY_JOB_BYTES==16u&&OVERLAY_RESIDENT_BYTES==24u&&OVERLAY_BOOT_ADDRESS==0x8000u,
    "overlay bootstrap ABI changed");
static void module_call(uint8_t operation) {
    cc_emit(0x3e);cc_emit(operation);EMIT_FIXED("\x32\x10\xc0\xaf\x32\x11\xc0\x21\x08\xc0\xcd\x00\x40");
}
static void runtime_emit(void) {
    uint16_t entry,loaded,skip,oldcleanup;
    uint16_t dir_operand,name_operand,cleanup_failed,cleanup_done,cleanup_keep;
    uint16_t directory_bytes=16u+overlays.members*16u;uint8_t i;
    entry=cc.emitted;cc_word(overlays.dispatch+1,entry);
    EMIT_FIXED("\x32\x10\xc0\x22\x12\xc0\xaf\x32\x11\xc0\xdd\xe5\xfd\xe5\x3a\x00\xc0\xb7");loaded=cc_forward(0xc2);
    EMIT_FIXED("\x2e\x19\xcf\xb7");skip=cc_forward(0xca);
    EMIT_FIXED("\x3e\xf0\x06\x01");cc_op16(0xc3,RT_ERROR);cc_patch(skip);
    EMIT_FIXED("\x78\x32\x00\xc0\x3e\x80\xdb\xf2\x32\x02\xc0");
    cc_patch(loaded);
    EMIT_FIXED("\x3e\x40\xdb\xf1\x32\x01\xc0\x3a\x00\xc0\xd3\xf1\x3a\x03\xc0\xb7");loaded=cc_forward(0xc2);
    EMIT_FIXED("\x21\x00\x40\x11\x01\x40\x01\xff\x3f\x36\x00\xed\xb0\x21\x00\x80\x11\x00\x40");
    cc_address(0xcd,cc.unpack_entry);
    /* The cold source occupies the replaceable overlay window, never page3.
     * Once unpacked, no packed copy is needed for later calls or cleanup. */
    EMIT_FIXED("\x21\x00\x80\x11\x01\x80\x01\xff\x3f\x36\x00\xed\xb0\x3e\x01\x32\x03\xc0");cc_patch(loaded);
    op_store(OVERLAY_JOB,0);dir_operand=cc.emitted+1;cc_address(0x21,0);cc_op16(0x22,OVERLAY_JOB+2);
    name_operand=cc.emitted+1;cc_address(0x21,0);cc_op16(0x22,OVERLAY_JOB+4);op_store(OVERLAY_JOB+6,directory_bytes);
    EMIT_FIXED("\x21\x08\xc0\xcd\x00\x40\x3a\x01\xc0\xd3\xf1\xfd\xe1\xdd\xe1\x3a\x16\xc0\xb7");loaded=cc_forward(0xca);
    EMIT_FIXED("\x3e\xf0\x06\x01");cc_op16(0xc3,RT_ERROR);cc_patch(loaded);
    EMIT_FIXED("\x3a\x15\xc0\x4f\x3a\x14\xc0\xc9");
    /* Terminal cleanup runs after the runtime has reset SP. Restore page2
     * before releasing execution banks, then release the support module too. */
    entry=cc.emitted;cc_word(overlays.cleanup+1,entry);
    EMIT_FIXED("\x78\x32\x04\xc0\xf5\xc5\xd5\xe5\xdd\xe5\xfd\xe5\x3a\x00\xc0\xb7");skip=cc_forward(0xca);
    EMIT_FIXED("\x3a\x02\xc0\xd3\xf2\x3e\x40\xdb\xf1\x32\x01\xc0\x3a\x00\xc0\xd3\xf1");module_call(3);
    EMIT_FIXED("\x3a\x01\xc0\xd3\xf1\x3a\x16\xc0\xb7");cleanup_failed=cc_forward(0xc2);
    EMIT_FIXED("\x3a\x00\xc0\x47\x2e\x1a\xcf\xb7");cleanup_keep=cc_forward(0xc2);
    EMIT_FIXED("\xaf\x32\x00\xc0\x32\x03\xc0");cleanup_done=cc_forward(0xc3);
    cc_patch(cleanup_failed);cc_patch(cleanup_keep);
    cc_op16(0x32,0xfe73); /* secondary native detail survives a primary fault */
    EMIT_FIXED("\x3a\x04\xc0\xfe\x02");cleanup_keep=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(0xf0);cc_runtime_op(0x32,cc_runtime_private(RP_ERROR_CODE));
    cc_emit(0x3e);cc_emit(1);cc_runtime_op(0x32,cc_runtime_private(RP_ERROR_NAMESPACE));
    cc_patch(cleanup_keep);cc_patch(cleanup_done);cc_patch(skip);
    EMIT_FIXED("\xfd\xe1\xdd\xe1\xe1\xd1\xc1\xf1");
    oldcleanup=cc_runtime_get(RT_FILECLEANUP-0x4000u+1)|((uint16_t)cc_runtime_get(RT_FILECLEANUP-0x4000u+2)<<8);
    cc_runtime_op(0xc3,oldcleanup);
    entry=cc.emitted;cc_word(name_operand,entry);
    for(i=0;overlays.base[i];++i)cc_emit((uint8_t)overlays.base[i]);cc_emit(0);
    overlays.directory=cc.emitted;cc_word(dir_operand,overlays.directory);
    for(entry=0;entry<directory_bytes;++entry)cc_emit(0);
}
static uint8_t apply_lates(void) {
    if(!cc_late_finish(&overlays.late.late))return error();
    overlays.late.late.sealed=0;overlays.late.late.references=0;return 1;
}
uint8_t cc_overlay_finish(void) ROUTINES {
    if(!cc.sources->overlays)return 1;
    runtime_emit();return !cc.error->code;
}
static uint8_t write_group(uint8_t group,const uint8_t *header) {
    /* One member is bounded by the16KiB execution window; only its file
     * location needs32bits, not this in-member copy cursor. */
    char path[128];uint8_t r[RECORD_BYTES],buf[128];uint16_t id,i,n,at;int8_t fd;
    strcpy(path,overlays.folder);strcat(path,"/P.000");i=(uint16_t)strlen(path);
    path[i-2]='0'+group/10;path[i-1]='0'+group%10;
    fd=open(path,O_WRONLY|O_CREAT);if(fd<0){platform_error=(uint8_t)-fd;return error();}
    memcpy(buf,header,16);memcpy(buf,"ZBG1",4);put16(buf,12,group);
    if(!output_write_buffer(fd,buf,16))return output_finish(fd,0);
    for(id=1;id<=overlays.members;++id) {
        read_record(id,r);put16(buf,0,word(r,M_GROUP));put16(buf,2,word(r,M_PARENT));
        put16(buf,4,word(r,M_END)-CC_CODE_BASE);put16(buf,6,word(r,M_ENTRY)-CC_CODE_BASE);
        put32(buf,8,dword(r,M_FILE));put16(buf,12,word(r,M_CHECK));put16(buf,14,0);
        if(!output_write_buffer(fd,buf,16))return output_finish(fd,0);
    }
    for(id=1;id<=overlays.members;++id) {
        read_record(id,r);if(word(r,M_GROUP)!=group)continue;
        if(!load_domain(id))return output_finish(fd,0);
        for(at=0;at<cc.emitted-CC_CODE_BASE;at+=n) {
            n=cc.emitted-CC_CODE_BASE-(uint16_t)at;if(n>128)n=128;
            for(i=0;i<n;++i)buf[i]=cc_overlay_get(CC_CODE_BASE+(uint16_t)at+i);
            if(!output_write_buffer(fd,buf,n))return output_finish(fd,0);
        }
    }
    return output_finish(fd,1);
}
static uint8_t place(uint16_t origin,uint16_t data) {
    uint8_t r[RECORD_BYTES],h[16],g;uint16_t bank,i,at,a,z;
    uint32_t offsets[100];
    overlays.root_origin=origin;
    if(!apply_lates()||!save_domain())return 0;
    for(bank=1;bank<=overlays.members;++bank) {
        if(!load_domain(bank)||!apply_lates()||!cc_link_apply(CC_CODE_BASE,data))return error();
        if(!save_domain())return 0;
    }
    /* Root late references must be applied before ordinary root relocation.
     * This entry is called before the normal placement loop. */
    if(!load_domain(0))return 0;
    memset(h,0,16);memcpy(h,"ZBD1",4);put16(h,4,1);put16(h,6,overlays.members);put16(h,8,overlays.groups);put16(h,10,16);
    at=overlays.directory;for(i=0;i<16;++i)cc_put(at+i,h[i]);
    for(g=0;g<overlays.groups;++g)offsets[g]=16u+overlays.members*16u;
    for(bank=1;bank<=overlays.members;++bank) {
        if(!cc_overlay_switch(bank))return 0;read_record(bank,r);a=z=0;
        for(i=CC_CODE_BASE;i<cc.emitted;++i){a=(a+cc_overlay_get(i))%255u;z=(z+a)%255u;}
        g=(uint8_t)word(r,M_GROUP);put32(r,M_FILE,offsets[g]);offsets[g]+=cc.emitted-CC_CODE_BASE;
        put16(r,M_CHECK,(z<<8)|a);write_record(bank,r);
        if(!cc_overlay_switch(0))return 0;
        at=overlays.directory+16u*bank;
        cc_word(at,word(r,M_GROUP));cc_word(at+2,word(r,M_PARENT));cc_word(at+4,word(r,M_END)-CC_CODE_BASE);
        cc_word(at+6,word(r,M_ENTRY)-CC_CODE_BASE);cc_word(at+8,word(r,M_FILE));cc_word(at+10,word(r,M_FILE+2));
        cc_word(at+12,word(r,M_CHECK));cc_word(at+14,0);
    }
    if(!save_domain())return 0;
    for(g=0;g<overlays.groups;++g)if(!write_group(g,h))return error();
    if(!load_domain(0))return 0;
    /* Keep source maps/catalog in the private spool after the compiler arena
     * is reclaimed by the IDE job. Published group generations are immutable. */
    overlays.catalog=overlays.length;
    if(!seek_to(overlays.catalog))return 0;
    for(bank=0;bank<=overlays.members;++bank){read_record(bank,r);if(!seek_to(overlays.catalog+(uint32_t)bank*RECORD_BYTES)||!output_write_buffer(overlays.fd,r,RECORD_BYTES))return error();}
    overlays.sealed=1;
    if(overlays.page!=255){if(!page_drop(overlays.page))return error();overlays.page=255;}
    if(!cc_late_discard(&overlays.late.late))return error();
    return 1;
}
uint8_t cc_overlay_place(uint16_t origin,uint16_t data) ROUTINES {
    uint8_t ok,primary;
    if(!code_phase_unit_finish_begin())return error();
    ok=place(origin,data);primary=platform_error;
    if(!code_phase_unit_finish_end())return error();
    if(!ok)platform_error=primary;return ok;
}
void cc_overlays_adopt(const Build *from,Build *to) SOURCE_BLOCK_CALL {if(overlays.owner==from)overlays.owner=to;}
void cc_overlays_close(const Build *b) SOURCE_BLOCK_CALL {
    uint8_t i,e,prior=platform_error;char path[128];
    if(overlays.owner!=b)return;
    if(overlays.fd>=0){e=close(overlays.fd);overlays.fd=-1;if(!prior)prior=e;}
    if(overlays.spool[0]){e=rm(overlays.spool);if(e!=ERR_NO_SUCH_ENTRY&&!prior)prior=e;}
    if(!overlays.published&&overlays.folder[0]) {
        strcpy(path,overlays.folder);strcat(path,"/NEW.COM");e=rm(path);if(e!=ERR_NO_SUCH_ENTRY&&!prior)prior=e;
        strcpy(path,overlays.folder);strcat(path,"/OLD.COM");e=rm(path);if(e!=ERR_NO_SUCH_ENTRY&&!prior)prior=e;
        for(i=0;i<overlays.groups;++i) {
            strcpy(path,overlays.folder);strcat(path,"/P.000");e=(uint8_t)strlen(path);path[e-2]='0'+i/10;path[e-1]='0'+i%10;
            e=rm(path);if(e!=ERR_NO_SUCH_ENTRY&&!prior)prior=e;
        }
        e=rm(overlays.folder);if(e!=ERR_NO_SUCH_ENTRY&&!prior)prior=e;
    }
    if(overlays.page!=255){if(page_drop(overlays.page))overlays.page=255;else if(!prior)prior=platform_error;}
    if(!cc_late_discard(&overlays.late.late)&&!prior)prior=platform_error;
    if(overlays.page==255&&overlays.late.late.page==255){overlays.owner=0;overlay_build_owned=0;}
    platform_error=prior;
}

/* COM replacement is prepared and verified before removing a previous image.
 * Group generations are immutable, so rollback restores its exact companions. */
static uint8_t same_file(const char *a,const char *b) {
    int8_t x=-1,y=-1;uint8_t left[128],right[128],ok=1;uint16_t n,m;
    x=open(a,O_RDONLY);if(x<0){platform_error=(uint8_t)-x;return 0;}
    y=open(b,O_RDONLY);if(y<0){platform_error=(uint8_t)-y;return output_finish(x,0);}
    for(;;) {
        n=m=128;platform_error=read(x,left,&n);
        if(!platform_error)platform_error=read(y,right,&m);
        if(platform_error||n!=m||n>128||memcmp(left,right,n)){ok=0;break;}
        if(!n)break;
    }
    if(!ok&&!platform_error)platform_error=ERR_ENTRY_CORRUPTED;
    ok=output_finish(x,ok);return output_finish(y,ok);
}
static uint8_t remove_owned(const char *path) {
    zos_stat_t info;uint8_t e=stat(path,&info);
    if(e==ERR_NO_SUCH_ENTRY)return 1;
    if(!e&&!D_ISFILE(info.s_flags))e=ERR_NOT_A_FILE;
    if(!e)e=rm(path);
    if(e){platform_error=e;return 0;}return 1;
}
uint8_t cc_overlay_publish(const Build *b,const char *path) SOURCE_BLOCK_CALL {
    char staged[128],backup[128];zos_stat_t info;uint8_t e,exists,primary;
    if(overlays.owner!=b)return 2;
    if(!overlays.sealed)return 0;
    strcpy(staged,overlays.folder);strcat(staged,"/NEW.COM");
    strcpy(backup,overlays.folder);strcat(backup,"/OLD.COM");
    if(!remove_owned(staged)||!cc_disk_publish(b,staged))return 0;
    e=stat(path,&info);exists=e!=ERR_NO_SUCH_ENTRY;
    if(e&&exists){platform_error=e;return 0;}
    if(exists) {
        if(!D_ISFILE(info.s_flags)){platform_error=ERR_NOT_A_FILE;return 0;}
        if(!remove_owned(backup)||!output_copy_file(path,backup)||!same_file(path,backup))return 0;
    }
    if(!remove_owned(path)||!output_copy_file(staged,path)||!same_file(staged,path)) {
        primary=platform_error;output_job_end(); /* rollback cannot be cancelled */
        if(!remove_owned(path)||(exists&&(!output_copy_file(backup,path)||!same_file(backup,path))))
            overlays.published=1; /* retain OLD.COM if rollback itself fails */
        platform_error=primary;return 0;
    }
    overlays.published=1;remove_owned(staged);if(exists)remove_owned(backup);return 1;
}
uint8_t cc_overlay_locate(const Build *b,uint16_t pc,uint8_t *file,uint16_t *position) SOURCE_BLOCK_CALL {
    uint16_t id,i,start,end,best=65535u,target=(uint16_t)(pc+overlays.root_origin);uint8_t r[RECORD_BYTES],m[8];uint32_t offset;
    if(overlays.owner!=b||!overlays.sealed||target<0x8000u||target>=0xc000u)return 0;
    for(id=1;id<=overlays.members;++id) {
        read_record(id,r);if(target>=word(r,M_END))continue;
        offset=dword(r,M_AT)+(uint32_t)(word(r,M_END)-CC_CODE_BASE);
        if(!seek_to(offset))return 0;
        for(i=0;i<word(r,M_MAPS);++i) {
            if(!output_read_buffer(overlays.fd,m,8))return 0;
            start=word(m,0);end=word(m,2);
            if(target-CC_CODE_BASE>=start&&target-CC_CODE_BASE<end&&end-start<=best) {
                best=end-start;*file=(uint8_t)word(m,4);*position=word(m,6);
            }
        }
        if(best!=65535u)return 1;
    }
    return 0;
}

#endif

#ifdef ZP_OVERLAYS
uint16_t cc_overlay_origin(void) FRONTEND {return overlays.root_origin;}
#endif
