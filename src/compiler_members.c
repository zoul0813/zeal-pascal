#include "compiler_members.h"
#if defined(__SDCC) && defined(ZP_UNITS_EXPERIMENT)
#pragma codeseg BANK16
#pragma constseg BANK16
#endif
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>

#define MEMBER_HEADER 24u

static uint8_t failed(void) { if(!platform_error)platform_error=ERR_FAILURE; return 0; }
static uint8_t position(int8_t fd,uint32_t offset,zos_whence_t whence) {
    int32_t at=(int32_t)offset;
    platform_error=seek(fd,&at,whence);
    return !platform_error&&(uint32_t)at==offset;
}
uint8_t cc_members_begin(CompilerMemberStore *store,int8_t fd) ROUTINES {
    store->fd=fd;store->length=0;store->failed=1;
    if(fd<0||!position(fd,0,SEEK_END))return failed();
    store->failed=0;return 1;
}
static void word(uint8_t *header,uint8_t at,uint16_t value) {
    header[at]=(uint8_t)value;header[at+1]=(uint8_t)(value>>8);
}
/* Only completed member suffixes are streamed. The parent stays frozen at
 * origin, so all addresses retain the existing scalar relocation meaning. */
static uint8_t member_byte(const CompilerMember *m,const uint8_t *header,CompilerLate *late,uint16_t first,uint32_t at) {
    uint16_t p;
    if(at<MEMBER_HEADER)return header[(uint8_t)at];
    at-=MEMBER_HEADER;
    if(at<m->bytes) {
        p=m->origin+(uint16_t)at;
        return p<0xc000u?far_get(cc.image->pages[2],p-CC_CODE_BASE):far_get(cc.stage_page,p-0xc000u);
    }
    at-=m->bytes;
    if(at<(uint32_t)m->maps*MAP_RECORD_BYTES)
        return buffer_get(cc.image->map_page,m->map_first*MAP_RECORD_BYTES+(uint16_t)at);
    at-=(uint32_t)m->maps*MAP_RECORD_BYTES;
    if(at<(uint32_t)m->relocs*RELOCATION_BYTES)
        return cc_reloc_get(m->reloc_first*RELOCATION_BYTES+(uint16_t)at);
    at-=(uint32_t)m->relocs*RELOCATION_BYTES;
    return cc_late_byte(late,first+(uint16_t)(at/LATE_REFERENCE_BYTES),(uint8_t)(at%LATE_REFERENCE_BYTES));
}
uint8_t cc_members_flush(CompilerMemberStore *store,CompilerMember *member,CompilerLate *late,uint16_t first) ROUTINES {
    CompilerMember next;
    uint8_t header[MEMBER_HEADER];uint8_t buffer[128];
    uint16_t i,n;
    uint32_t at,total;
    if(store->failed)return failed();
    /* Any failure, including an unexpected end-of-file position, makes this
     * append unusable. Never retry after a possibly partial disk write. */
    store->failed=1;
    next=*member;
    if(!cc.image||member->origin<CC_CODE_BASE||member->origin>=cc.emitted||
       member->entry<member->origin||member->entry>=cc.emitted||
       member->map_first>cc.entries||member->reloc_first>cc.relocations||
       (uint32_t)cc.entries*MAP_RECORD_BYTES>SOURCE_LIMIT||
       page_role(cc.image->pages[2])!=PAGE_IMAGE||page_role(cc.image->map_page)!=PAGE_MAP||
       (cc.emitted>0xc000u&&page_role(cc.stage_page)!=PAGE_COMPILER))return failed();
    next.offset=store->length;next.bytes=cc.emitted-next.origin;
    next.maps=cc.entries-next.map_first;next.relocs=cc.relocations-next.reloc_first;
    if(late&&!cc_late_range(late,first,next.origin,cc.emitted))return 0;
    if(!late&&first)return failed();
    next.lates=late?late->references-first:0;
    total=MEMBER_HEADER+(uint32_t)next.bytes+(uint32_t)next.maps*MAP_RECORD_BYTES+
        (uint32_t)next.relocs*RELOCATION_BYTES+(uint32_t)next.lates*LATE_REFERENCE_BYTES;
    if(store->length>0x7ffffffful-total)return failed();
    /* SEEK_END returns the actual file size. Refuse unexpected appended data
     * rather than overwriting it. The transaction owns the opened file. */
    {
        int32_t end=0;
        platform_error=seek(store->fd,&end,SEEK_END);
        if(platform_error||(uint32_t)end!=store->length)return failed();
    }
    memset(header,0,sizeof(header));memcpy(header,"ZPMS",4);
    header[4]=2;word(header,6,MEMBER_HEADER);word(header,8,next.origin);
    word(header,10,next.bytes);word(header,12,next.entry-next.origin);
    word(header,14,next.maps);word(header,16,next.relocs);
    word(header,18,next.lates);
    for(at=0;at<total;at+=n) {
        n=total-at>128?128:(uint16_t)(total-at);
        for(i=0;i<n;++i)buffer[i]=member_byte(&next,header,late,first,at+i);
        if(late&&late->failed)return 0;
        if(!output_write_buffer(store->fd,buffer,n))return 0;
    }
    if(!position(store->fd,next.offset,SEEK_SET))return failed();
    for(at=0;at<total;at+=n) {
        n=total-at>128?128:(uint16_t)(total-at);
        if(!output_read_buffer(store->fd,buffer,n))return 0;
        for(i=0;i<n;++i)if(buffer[i]!=member_byte(&next,header,late,first,at+i))return failed();
        if(late&&late->failed)return failed();
    }
    {
        int32_t end=0;
        platform_error=seek(store->fd,&end,SEEK_END);
        if(platform_error||(uint32_t)end!=next.offset+total)return failed();
    }
    /* I/O callbacks may invalidate the journal after its last serialized byte,
     * including during read-back or the final seek. Disk equality is not an
     * ownership check. Publish metadata only while the source is still valid. */
    if(late&&!cc_late_range(late,first,next.origin,cc.emitted))return 0;
    *member=next;store->length+=total;store->failed=0;return 1;
}
