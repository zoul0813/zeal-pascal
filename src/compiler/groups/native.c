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
       M_FILE=32,M_CHECK=36 };
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
static uint8_t error(void) {if(!overlays.sealed&&cc.error&&!cc.error->code)cc_fail(COMPILE_PLATFORM_FAILURE);if(!platform_error)platform_error=ERR_FAILURE;return 0;}
static uint8_t seek_to(uint32_t at) {
    int32_t offset=(int32_t)at;platform_error=seek(overlays.fd,&offset,SEEK_SET);
    return !platform_error&&(uint32_t)offset==at?1:error();
}
static void read_record(uint16_t id,uint8_t *r) {
    if(overlays.sealed) {if(!seek_to(overlays.catalog+(uint32_t)id*RECORD_BYTES)||!output_read_buffer(overlays.fd,r,RECORD_BYTES))memset(r,0,RECORD_BYTES);}
    else cc_overlay_record_read((uint32_t)id*RECORD_BYTES,r,RECORD_BYTES);
}
static void write_record(uint16_t id,const uint8_t *r) {
    uint8_t i;for(i=0;i<RECORD_BYTES;++i)cc_overlay_record_put((uint32_t)id*RECORD_BYTES+i,r[i]);
}
static uint16_t word(const uint8_t *r,uint8_t at) {return r[at]|((uint16_t)r[at+1]<<8);}
static uint32_t dword(const uint8_t *r,uint8_t at) {return word(r,at)|((uint32_t)word(r,at+2)<<16);}
static void put16(uint8_t *r,uint8_t at,uint16_t n) {r[at]=(uint8_t)n;r[at+1]=(uint8_t)(n>>8);}
static void put32(uint8_t *r,uint8_t at,uint32_t n) {put16(r,at,(uint16_t)n);put16(r,at+2,(uint16_t)(n>>16));}
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
    put16(r,M_RELOCS,cc.relocations);put16(r,M_LINKS,cc.links);put16(r,M_LATES,overlays.late.late.references);
    total=(bank?cc.emitted-CC_CODE_BASE:0)+(uint32_t)cc.entries*8u+(uint32_t)cc.relocations*4u+
        (uint32_t)cc.links*8u+(uint32_t)overlays.late.late.references*8u;
    if(!seek_to(overlays.length))return 0;
    put32(r,M_AT,overlays.length);put32(r,M_LENGTH,total);
    for(at=0;at<total;at+=n) {
        n=total-at>128?128:(uint16_t)(total-at);
        for(i=0;i<n;++i){bytes[i]=snapshot_byte(bank,at+i);hash=(hash^bytes[i])*16777619UL;}
        if(cc.error->code||!output_write_buffer(overlays.fd,bytes,n))return error();
    }
    put32(r,M_HASH,hash);write_record(bank,r);overlays.length+=total;
    return !cc.error->code;
}
static uint8_t load_domain(uint16_t bank) {
    uint8_t r[RECORD_BYTES],bytes[128],link[8],late[8];uint16_t i,n;
    uint32_t at,total,code,maps,relocs,links,hash=2166136261UL;
    overlays.late.late.sealed=0;read_record(bank,r);total=dword(r,M_LENGTH);code=bank?word(r,M_END)-CC_CODE_BASE:0;
    maps=code+(uint32_t)word(r,M_MAPS)*8u;relocs=maps+(uint32_t)word(r,M_RELOCS)*4u;
    links=relocs+(uint32_t)word(r,M_LINKS)*8u;
    if(total!=links+(uint32_t)word(r,M_LATES)*8u||!cc_late_retire(&overlays.late.late,0)||!seek_to(dword(r,M_AT)))return error();
    cc.sources->bank=bank;cc.emitted=word(r,M_END);cc.entries=word(r,M_MAPS);cc.relocations=word(r,M_RELOCS);cc.links=0;
    if(!buffer_reserve(cc.image->map_page,cc.entries*8u))return error();
    for(at=0;at<total;at+=n) {
        n=total-at>128?128:(uint16_t)(total-at);
        if(!output_read_buffer(overlays.fd,bytes,n))return error();
        for(i=0;i<n;++i) {
            uint32_t p=at+i;uint8_t b=bytes[i];hash=(hash^b)*16777619UL;
            if(p<code)cc_overlay_put(CC_CODE_BASE+(uint16_t)p,b);
            else if(p<maps)buffer_set(cc.image->map_page,(uint16_t)(p-code),b);
            else if(p<relocs)cc_reloc_put((uint16_t)(p-maps),b);
            else if(p<links) {
                link[(uint8_t)((p-relocs)%8)]=b;
                if((p-relocs)%8==7)cc_link_reference(word(link,0),link[6],word(link,2),word(link,4),link[7]);
            } else {
                late[(uint8_t)((p-links)%8)]=b;
                if((p-links)%8==7&&!cc_late_reference(&overlays.late.late,word(late,0),word(late,2),word(late,4),late[6]))return error();
            }
        }
    }
    if(hash!=dword(r,M_HASH)||cc.error->code)return error();
    return 1;
}
uint8_t cc_overlay_switch(uint16_t bank) ROUTINES {
    if(cc.sources->bank==bank)return 1;
    return save_domain()&&load_domain(bank);
}
uint8_t cc_overlays_begin(void) ROUTINES {
    char parent[128];uint16_t n,i;uint8_t e,r[RECORD_BYTES];
    if(overlays.owner)return error();
    memset(&overlays,0,sizeof(overlays));overlays.page=255;overlays.fd=-1;
    cc_groups_init(&overlays.late);overlays.owner=cc.image;overlay_build_owned=1;
    if(!path_program(cc.sources->root->path,parent,0))return error();
    n=(uint16_t)strlen(parent);while(n&&parent[n-1]!='/')--n;
    if(n+7u>127u)return error();parent[n]=0;
    for(i=0;i<65535u;++i) {
        static const char hex[]="0123456789ABCDEF";
        strcpy(overlays.folder,parent);overlays.base[0]='Z';overlays.base[1]='P';
        overlays.base[2]=hex[generation>>12];overlays.base[3]=hex[(generation>>8)&15];
        overlays.base[4]=hex[(generation>>4)&15];overlays.base[5]=hex[generation&15];overlays.base[6]=0;++generation;
        strcat(overlays.folder,overlays.base);e=mkdir(overlays.folder);
        if(!e)break;if(e!=ERR_ALREADY_EXIST){platform_error=e;return error();}
    }
    if(i==65535u)return error();
    strcpy(overlays.spool,overlays.folder);strcat(overlays.spool,"/TEMP");strcat(overlays.base,"/P");
    overlays.fd=open(overlays.spool,O_RDWR|O_CREAT);if(overlays.fd<0){platform_error=(uint8_t)-overlays.fd;overlays.fd=-1;return error();}
    overlays.page=page_new(PAGE_COMPILER);if(overlays.page==255)return error();
    cc.sources->overlays=&overlays;cc.sources->groups=&overlays.late;
    memset(r,0,sizeof(r));put16(r,M_END,CC_CODE_BASE);put32(r,M_HASH,2166136261UL);write_record(0,r);
    cc.image->globals_bytes=OVERLAY_RESIDENT_BYTES;
    cc_root_request(LATE_ROOT,LATE_UNPACK);
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
    cc_rput(scope,R_BANK,bank);if(!cc_overlay_switch(bank))return 0;return bank;
}
void cc_overlay_alias(uint16_t scope) ROUTINES {
    uint16_t bank=cc.sources->bank,entry;SourcePosition position;
    if(!cc_overlay_switch(0))return;
    entry=cc.emitted;cc_rput(scope,R_ENTRY,entry);cc_rput(scope,R_CALLS,cc_link(0xc3,0));
    position=cc_reference_position(CCS(cc_rget(scope,R_SYMBOL),name_start));
    cc_mapping(entry-CC_CODE_BASE,cc.emitted-CC_CODE_BASE,position,position);
    cc_overlay_switch(bank);
}
static void indexed(uint8_t op,uint8_t offset) {cc_emit(0xdd);cc_emit(op);cc_emit(offset);}
static void dispatch(uint8_t op,uint16_t value) {
    cc_op16(0x21,value);cc_emit(0x3e);cc_emit(op);cc_address(0xcd,overlays.dispatch);
}
void cc_overlay_wrappers(uint16_t bank) ROUTINES {
    uint16_t old=cc.sources->bank,scope,saved=cc.scope,entry,body,reset,skip;uint8_t r[RECORD_BYTES];
    if(!cc_overlay_switch(0))return;read_record(bank,r);reset=word(r,M_RESET);
    for(scope=1;scope<=cc.routine_count&&!cc.error->code;++scope)if(cc_rget(scope,R_BANK)==bank) {
        cc.scope=scope;entry=cc.emitted;cc_patch_chain(cc_rget(scope,R_CALLS),entry);cc_rput(scope,R_CALLS,0);
        cc_frame_begin();cc_reserve(512);
        cc_emit(0x3e);cc_emit(0x80);cc_emit(0xdb);cc_emit(0xf2);cc_emit(0xf5);
        dispatch(0,bank-1);cc_emit(0xd3);cc_emit(0xf2);
        if(reset) {cc_emit(0x79);cc_emit(0xb7);skip=cc_forward(0xca);cc_op16(0xcd,reset);cc_patch(skip);}
        body=cc_rget(scope,R_BODY);cc_op16(0xcd,body);
        cc_emit(0xf1);cc_emit(0xd3);cc_emit(0xf2);
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
extern uint8_t cc_overlay_asset_get(uint16_t at) FRONTEND;
/* The saved image's otherwise unused page2 carries the cold bootstrap.
 * Code/global staging remains independent; publication and image reads share
 * this mapping so the bytes that are checked are the bytes that are saved. */
uint8_t cc_overlay_image_byte(const Build *build,uint16_t offset) FRONTEND {
    if(overlays.owner!=build||offset>=OVERLAY_PACKED_BYTES)return 0;
    return cc_overlay_asset_get(offset);
}
static void op_store(uint16_t address,uint16_t value) {cc_op16(0x21,value);cc_op16(0x22,address);}
static uint16_t control(void) {return 0xc000u;}
static void module_call(uint8_t operation) {
    uint16_t c=control(),j=c+8;
    cc_emit(0x3e);cc_emit(operation);cc_op16(0x32,j+8);cc_emit(0xaf);cc_op16(0x32,j+9);
    cc_op16(0x21,j);cc_op16(0xcd,0x4000);
}
static void runtime_emit(void) {
    uint16_t c=control(),j=c+8,entry,loaded,skip,oldcleanup;
    uint16_t dir_operand,name_operand,cleanup_failed,cleanup_done,cleanup_keep;
    uint16_t directory_bytes=16u+overlays.members*16u;uint8_t i;
    entry=cc.emitted;cc_word(overlays.dispatch+1,entry);
    cc_op16(0x32,j+8);cc_op16(0x22,j+10);cc_emit(0xaf);cc_op16(0x32,j+9);
    cc_emit(0xdd);cc_emit(0xe5);cc_emit(0xfd);cc_emit(0xe5);
    cc_op16(0x3a,c);cc_emit(0xb7);loaded=cc_forward(0xc2);
    cc_emit(0x2e);cc_emit(25);cc_emit(0xcf);cc_emit(0xb7);skip=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(0xf0);cc_emit(0x06);cc_emit(1);cc_op16(0xc3,RT_ERROR);cc_patch(skip);
    cc_emit(0x78);cc_op16(0x32,c);
    cc_emit(0x3e);cc_emit(0x80);cc_emit(0xdb);cc_emit(0xf2);cc_op16(0x32,c+2);
    cc_patch(loaded);
    cc_emit(0x3e);cc_emit(0x40);cc_emit(0xdb);cc_emit(0xf1);cc_op16(0x32,c+1);
    cc_op16(0x3a,c);cc_emit(0xd3);cc_emit(0xf1);
    cc_op16(0x3a,c+3);cc_emit(0xb7);loaded=cc_forward(0xc2);
    cc_op16(0x21,0x4000);cc_op16(0x11,0x4001);cc_op16(0x01,0x3fff);cc_emit(0x36);cc_emit(0);cc_emit(0xed);cc_emit(0xb0);
    cc_op16(0x21,OVERLAY_BOOT_ADDRESS);cc_op16(0x11,0x4000);
    cc_address(0xcd,cc.unpack_entry);
    /* The cold source occupies the replaceable overlay window, never page3.
     * Once unpacked, no packed copy is needed for later calls or cleanup. */
    cc_op16(0x21,OVERLAY_BOOT_ADDRESS);cc_op16(0x11,OVERLAY_BOOT_ADDRESS+1);
    cc_op16(0x01,0x3fff);cc_emit(0x36);cc_emit(0);cc_emit(0xed);cc_emit(0xb0);
    cc_emit(0x3e);cc_emit(1);cc_op16(0x32,c+3);cc_patch(loaded);
    op_store(j,0);dir_operand=cc.emitted+1;cc_address(0x21,0);cc_op16(0x22,j+2);
    name_operand=cc.emitted+1;cc_address(0x21,0);cc_op16(0x22,j+4);op_store(j+6,directory_bytes);
    cc_op16(0x21,j);cc_op16(0xcd,0x4000);
    cc_op16(0x3a,c+1);cc_emit(0xd3);cc_emit(0xf1);
    cc_emit(0xfd);cc_emit(0xe1);cc_emit(0xdd);cc_emit(0xe1);
    cc_op16(0x3a,j+14);cc_emit(0xb7);loaded=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(0xf0);cc_emit(0x06);cc_emit(1);cc_op16(0xc3,RT_ERROR);cc_patch(loaded);
    cc_op16(0x3a,j+13);cc_emit(0x4f);cc_op16(0x3a,j+12);cc_emit(0xc9);
    /* Terminal cleanup runs after the runtime has reset SP. Restore page2
     * before releasing execution banks, then release the support module too. */
    entry=cc.emitted;cc_word(overlays.cleanup+1,entry);
    cc_emit(0x78);cc_op16(0x32,c+4);
    cc_emit(0xf5);cc_emit(0xc5);cc_emit(0xd5);cc_emit(0xe5);cc_emit(0xdd);cc_emit(0xe5);cc_emit(0xfd);cc_emit(0xe5);
    cc_op16(0x3a,c);cc_emit(0xb7);skip=cc_forward(0xca);
    cc_op16(0x3a,c+2);cc_emit(0xd3);cc_emit(0xf2);
    cc_emit(0x3e);cc_emit(0x40);cc_emit(0xdb);cc_emit(0xf1);cc_op16(0x32,c+1);
    cc_op16(0x3a,c);cc_emit(0xd3);cc_emit(0xf1);module_call(3);
    cc_op16(0x3a,c+1);cc_emit(0xd3);cc_emit(0xf1);
    cc_op16(0x3a,j+14);cc_emit(0xb7);cleanup_failed=cc_forward(0xc2);
    cc_op16(0x3a,c);cc_emit(0x47);cc_emit(0x2e);cc_emit(26);cc_emit(0xcf);
    cc_emit(0xb7);cleanup_keep=cc_forward(0xc2);
    cc_emit(0xaf);cc_op16(0x32,c);cc_op16(0x32,c+3);cleanup_done=cc_forward(0xc3);
    cc_patch(cleanup_failed);cc_patch(cleanup_keep);
    cc_op16(0x32,0xfe73); /* secondary native detail survives a primary fault */
    cc_op16(0x3a,c+4);cc_emit(0xfe);cc_emit(2);cleanup_keep=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(0xf0);cc_runtime_op(0x32,cc_runtime_private(RP_ERROR_CODE));
    cc_emit(0x3e);cc_emit(1);cc_runtime_op(0x32,cc_runtime_private(RP_ERROR_NAMESPACE));
    cc_patch(cleanup_keep);cc_patch(cleanup_done);cc_patch(skip);
    cc_emit(0xfd);cc_emit(0xe1);cc_emit(0xdd);cc_emit(0xe1);cc_emit(0xe1);cc_emit(0xd1);cc_emit(0xc1);cc_emit(0xf1);
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
static uint8_t read_snapshot(uint16_t bank) {
    overlays.late.late.sealed=0;return load_domain(bank);
}
uint8_t cc_overlay_finish(void) ROUTINES {
    if(!cc.sources->overlays)return 1;
    runtime_emit();return !cc.error->code;
}
static uint8_t write_group(uint8_t group,const uint8_t *header) {
    char path[128];uint8_t r[RECORD_BYTES],buf[128];uint16_t id,i,n;int8_t fd;uint32_t at;
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
        if(!read_snapshot(id))return output_finish(fd,0);
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
        if(!read_snapshot(bank)||!apply_lates()||!cc_link_apply(CC_CODE_BASE,data))return error();
        if(!save_domain())return 0;
    }
    /* Root late references must be applied before ordinary root relocation.
     * This entry is called before the normal placement loop. */
    if(!read_snapshot(0))return 0;
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
    if(!read_snapshot(0))return 0;
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
