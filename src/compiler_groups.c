#include "compiler_groups.h"
#include "output_transport.h"
#include <zos_errors.h>

/* Explicit far records; no host/SDCC structure layout is serialized.
 * 48 +100*18 +255*24 +256 scope owners fit one page. Routine IDs
 * in this dormant group format remain eight-bit; ordinary compilation uses
 * resource-backed wider scope IDs. */
#define GROUP_BASE 48u
#define GROUP_BYTES 18u
#define MEMBER_BASE (GROUP_BASE+100u*GROUP_BYTES)
#define MEMBER_BYTES 24u
#define DOMAIN_BASE (MEMBER_BASE+255u*MEMBER_BYTES)
typedef char catalog_fits[(DOMAIN_BASE+256u<=PAGE_SIZE)?1:-1];
enum { G_ORIGIN,G_BYTES=2,G_MAP=4,G_RELOC=6,G_OWNER=8,G_PARENT,G_PARENT_GROUP,G_PARENT_MEMBER,G_MEMBERS,G_STATE=14,G_LATE=16 };
enum { M_GROUP,M_ORDINAL,M_STATE,M_ORIGIN=4,M_BYTES=6,M_ENTRY=8,M_MAP=10,M_MAPS=12,M_RELOC=14,M_RELOCS=16,M_LATES=18,M_OFFSET=20 };
enum { OPEN=1,CLOSED=2,ACTIVE=1,COMMITTED=2 };

static uint8_t fail(CompilerGroups *g,uint8_t error) {
    if(!platform_error)platform_error=error;
    g->store.failed=1;return 0;
}
static uint8_t ready(CompilerGroups *g) {
    if(g->store.failed)return 0;
    if(g->late.failed)return fail(g,ERR_FAILURE);
    if(g->page==255||page_role(g->page)!=PAGE_COMPILER||page_lifetime(g->page)!=g->lifetime||
       page_versions[g->page]!=g->version)return fail(g,ERR_FAILURE);
    return 1;
}
static uint8_t get(CompilerGroups *g,uint16_t at) {return far_get(g->page,at);}
static uint16_t word(CompilerGroups *g,uint16_t at) {return get(g,at)|((uint16_t)get(g,at+1)<<8);}
static void put(CompilerGroups *g,uint16_t at,uint8_t value) {far_set(g->page,at,value);}
static void put_word(CompilerGroups *g,uint16_t at,uint16_t value) {put(g,at,(uint8_t)value);put(g,at+1,(uint8_t)(value>>8));}
static void touch(CompilerGroups *g) {g->version=page_versions[g->page];}
static uint16_t group_at(uint8_t id) {return GROUP_BASE+(uint16_t)id*GROUP_BYTES;}
static uint16_t member_at(uint8_t scope) {return MEMBER_BASE+(uint16_t)(scope-1u)*MEMBER_BYTES;}
static uint8_t frozen(CompilerGroups *g,uint16_t at) {
    return g->active==get(g,at+G_PARENT)&&cc.emitted==word(g,at+G_ORIGIN)&&
        cc.entries==word(g,at+G_MAP)&&cc.relocations==word(g,at+G_RELOC)&&
        g->late.references==word(g,at+G_LATE);
}
void cc_groups_init(CompilerGroups *g) ROUTINES {
    memset(g,0,sizeof(*g));g->page=255;g->store.fd=-1;g->store.failed=1;cc_late_init(&g->late);
}
uint8_t cc_groups_begin(CompilerGroups *g,int8_t fd) ROUTINES {
    uint16_t at;
    if(g->page!=255||g->store.fd>=0||g->lifetime||g->late.page!=255) {platform_error=ERR_ALREADY_OPENED;return 0;}
    if(fd<0) {if(!platform_error)platform_error=ERR_INVALID_FILEDEV;return 0;}
    if(!cc_members_begin(&g->store,fd))return 0;
    g->page=page_new(PAGE_COMPILER);
    if(g->page==255)return fail(g,ERR_NO_MORE_MEMORY);
    g->lifetime=page_lifetime(g->page);touch(g);
    for(at=0;at<PAGE_SIZE;++at) {
        if(!(at&255)&&!output_poll()) {g->store.failed=1;return 0;}
        put(g,at,0);
    }
    touch(g);page_initialized(g->page,PAGE_SIZE);return 1;
}
uint8_t cc_groups_discard(CompilerGroups *g) ROUTINES {
    uint8_t prior=platform_error;uint8_t first=0;uint8_t ok=1;
    if(g->store.fd>=0) {
        if(!output_finish(g->store.fd,1)) {first=platform_error;ok=0;}
        g->store.fd=-1; /* VFS revokes a handle even when close reports failure. */
    }
    if(g->page!=255) {
        if(page_role(g->page)!=PAGE_COMPILER||page_lifetime(g->page)!=g->lifetime) {
            first=first?first:ERR_FAILURE;ok=0;
        } else if(!page_drop(g->page)) {if(!first)first=platform_error;ok=0;}
        else {g->page=255;g->lifetime=0;}
    }
    if(!cc_late_discard(&g->late)) {if(!first)first=platform_error;ok=0;}
    g->store.failed=1;
    if(g->page==255&&g->store.fd<0&&g->late.page==255)cc_groups_init(g);
    if(prior||first)platform_error=prior?prior:first;
    return ok;
}
uint8_t cc_groups_scope(CompilerGroups *g,uint8_t scope) ROUTINES {
    if(!ready(g))return 0;
    if(!scope||scope>cc.routine_count)return fail(g,ERR_INVALID_PARAMETER);
    put(g,DOMAIN_BASE+scope,g->active);touch(g);return 1;
}
uint8_t cc_groups_domain(CompilerGroups *g,uint8_t scope,uint8_t *domain) ROUTINES {
    if(!ready(g))return 0;
    if(scope>cc.routine_count)return fail(g,ERR_INVALID_PARAMETER);
    *domain=get(g,DOMAIN_BASE+scope);return 1;
}
uint8_t cc_group_open(CompilerGroups *g,TypeId owner) ROUTINES {
    uint8_t id;uint8_t i;uint16_t at,parent;
    if(!ready(g))return 255;
    if(g->groups==100) {fail(g,ERR_NO_MORE_ENTRIES);return 255;}
    if(owner>255u||owner>cc.routine_count||cc.emitted<CC_CODE_BASE||cc.emitted>=CC_STATIC_LIMIT) {
        fail(g,ERR_INVALID_PARAMETER);return 255;
    }
    for(i=0;i<g->groups;++i) {
        at=group_at(i);
        if(get(g,at+G_STATE)==OPEN&&get(g,at+G_PARENT)==g->active) {
            fail(g,ERR_INVALID_PARAMETER);return 255;
        }
    }
    id=g->groups++;at=group_at(id);
    put_word(g,at+G_ORIGIN,cc.emitted);put_word(g,at+G_MAP,cc.entries);put_word(g,at+G_RELOC,cc.relocations);
    put_word(g,at+G_LATE,g->late.references);
    put(g,at+G_OWNER,owner);put(g,at+G_PARENT,g->active);
    parent=g->active?member_at(g->active):0;
    put(g,at+G_PARENT_GROUP,parent?get(g,parent+M_GROUP):255);
    put(g,at+G_PARENT_MEMBER,parent?get(g,parent+M_ORDINAL):255);
    put(g,at+G_STATE,OPEN);touch(g);return id;
}
uint8_t cc_group_member_begin(CompilerGroups *g,uint8_t id,uint8_t scope) ROUTINES {
    uint16_t at,m,count;
    if(!ready(g))return 0;
    if(id>=g->groups||!scope||scope>cc.routine_count)return fail(g,ERR_INVALID_PARAMETER);
    at=group_at(id);m=member_at(scope);count=word(g,at+G_MEMBERS);
    if(get(g,at+G_STATE)!=OPEN||!frozen(g,at)||get(g,m+M_STATE)||count>=255)
        return fail(g,ERR_INVALID_PARAMETER);
    put(g,m+M_GROUP,id);put(g,m+M_ORDINAL,(uint8_t)count);put(g,m+M_STATE,ACTIVE);
    put_word(g,at+G_MEMBERS,count+1);g->active=scope;touch(g);return 1;
}
uint8_t cc_group_member_end(CompilerGroups *g,uint8_t id,uint8_t scope,uint16_t entry) ROUTINES {
    static const uint8_t roots[]={LATE_OUTPUT,LATE_TRANSFER,LATE_UNPACK};
    CompilerMember record;uint16_t at,m,i,heads[3],symbol;uint8_t j;
    if(!ready(g))return 0;
    if(id>=g->groups||!scope||scope>cc.routine_count||g->active!=scope)return fail(g,ERR_INVALID_PARAMETER);
    at=group_at(id);m=member_at(scope);
    if(get(g,at+G_STATE)!=OPEN||get(g,m+M_STATE)!=ACTIVE||get(g,m+M_GROUP)!=id)
        return fail(g,ERR_INVALID_PARAMETER);
    for(j=0;j<g->groups;++j)
        if(get(g,group_at(j)+G_STATE)==OPEN&&get(g,group_at(j)+G_PARENT)==scope)
            return fail(g,ERR_INVALID_PARAMETER);
    memset(&record,0,sizeof(record));record.origin=word(g,at+G_ORIGIN);record.entry=entry;
    record.map_first=word(g,at+G_MAP);record.reloc_first=word(g,at+G_RELOC);
    /* These fields are prepend heads until the root finishers run. Keep
     * their parent tails local until disk commit; never leave a successful
     * member's operand as the next member's global chain head. */
    heads[0]=cc.hook_output_calls;heads[1]=cc.transfer_entry;heads[2]=cc.unpack_entry;
    for(j=0;j<3;++j)if(heads[j]>=record.origin) {
        symbol=cc_late_symbol(&g->late,LATE_ROOT,roots[j],0);
        if(symbol==LATE_NONE||!cc_late_chain(&g->late,record.origin,heads[j],symbol,&heads[j]))
            return fail(g,ERR_FAILURE);
    }
    if(!cc_members_flush(&g->store,&record,&g->late,word(g,at+G_LATE)))return 0;
    /* Recheck the catalog after I/O and retire the journal before publishing
     * heads, member state or compiler cursors. No fallible I/O follows. */
    if(!ready(g)||!cc_late_retire(&g->late,word(g,at+G_LATE)))return fail(g,ERR_FAILURE);
    cc.hook_output_calls=heads[0];cc.transfer_entry=heads[1];cc.unpack_entry=heads[2];
    put_word(g,m+M_ORIGIN,record.origin);put_word(g,m+M_BYTES,record.bytes);put_word(g,m+M_ENTRY,record.entry);
    put_word(g,m+M_MAP,record.map_first);put_word(g,m+M_MAPS,record.maps);
    put_word(g,m+M_RELOC,record.reloc_first);put_word(g,m+M_RELOCS,record.relocs);
    put_word(g,m+M_LATES,record.lates);
    for(i=0;i<4;++i)put(g,m+M_OFFSET+i,(uint8_t)(record.offset>>(i*8)));
    put(g,m+M_STATE,COMMITTED);
    if(record.bytes>word(g,at+G_BYTES))put_word(g,at+G_BYTES,record.bytes);
    cc.entries=record.map_first;cc.relocations=record.reloc_first;cc.emitted=record.origin;
    g->active=get(g,at+G_PARENT);touch(g);return 1;
}
uint8_t cc_group_close(CompilerGroups *g,uint8_t id) ROUTINES {
    uint16_t at,end,p;
    if(!ready(g))return 0;
    if(id>=g->groups)return fail(g,ERR_INVALID_PARAMETER);
    at=group_at(id);
    if(get(g,at+G_STATE)!=OPEN||!frozen(g,at)||!word(g,at+G_MEMBERS)||!word(g,at+G_BYTES))
        return fail(g,ERR_INVALID_PARAMETER);
    end=cc.emitted+word(g,at+G_BYTES);
    if(!cc.image||page_role(cc.image->pages[2])!=PAGE_IMAGE||
       (end>0xc000u&&(cc.stage_page==g->page||page_role(cc.stage_page)!=PAGE_COMPILER)))
        return fail(g,ERR_FAILURE);
    for(p=cc.emitted;p<end;++p) {
        if(!(p&255)&&!output_poll()) {g->store.failed=1;return 0;}
        if(p<0xc000u)far_set(cc.image->pages[2],p-CC_CODE_BASE,0);
        else far_set(cc.stage_page,p-0xc000u,0);
    }
    cc.emitted=end;put(g,at+G_STATE,CLOSED);touch(g);return 1;
}
uint8_t cc_group_get(CompilerGroups *g,uint8_t id,CompilerGroup *record) ROUTINES {
    uint16_t at;
    if(!ready(g))return 0;
    if(id>=g->groups)return fail(g,ERR_INVALID_PARAMETER);
    at=group_at(id);
    record->origin=word(g,at+G_ORIGIN);record->bytes=word(g,at+G_BYTES);
    record->map_first=word(g,at+G_MAP);record->reloc_first=word(g,at+G_RELOC);
    record->members=word(g,at+G_MEMBERS);record->owner=get(g,at+G_OWNER);
    record->parent=get(g,at+G_PARENT);record->parent_group=get(g,at+G_PARENT_GROUP);
    record->parent_member=get(g,at+G_PARENT_MEMBER);record->closed=get(g,at+G_STATE)==CLOSED;
    return 1;
}
uint8_t cc_group_member_get(CompilerGroups *g,uint8_t scope,uint8_t *id,uint8_t *ordinal,CompilerMember *record) ROUTINES {
    uint16_t m;uint8_t i;
    if(!ready(g))return 0;
    if(!scope||scope>cc.routine_count)return fail(g,ERR_INVALID_PARAMETER);
    m=member_at(scope);
    if(get(g,m+M_STATE)!=COMMITTED)return 0;
    *id=get(g,m+M_GROUP);*ordinal=get(g,m+M_ORDINAL);
    record->origin=word(g,m+M_ORIGIN);record->bytes=word(g,m+M_BYTES);record->entry=word(g,m+M_ENTRY);
    record->map_first=word(g,m+M_MAP);record->maps=word(g,m+M_MAPS);
    record->reloc_first=word(g,m+M_RELOC);record->relocs=word(g,m+M_RELOCS);
    record->lates=word(g,m+M_LATES);
    record->offset=0;for(i=0;i<4;++i)record->offset|=(uint32_t)get(g,m+M_OFFSET+i)<<(i*8);
    return 1;
}
