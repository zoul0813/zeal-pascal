#include "compiler_groups.h"
#include <zos_errors.h>

/* Eight-byte symbols grow up; eight-byte references grow down. Only written
 * records are read. The unused gap never leaves this private far page. */
#define SYMBOL_BYTES 8u
enum { SY_KIND,SY_ID,SY_OWNER,SY_STATE,SY_VALUE=4 };
enum { RF_SITE,RF_SYMBOL=2,RF_COEFFICIENT=4,RF_BYTE=6 };
static uint8_t fail(CompilerLate *l,uint8_t error) {
    if(!platform_error)platform_error=error;
    l->failed=1;return 0;
}
static uint8_t ready(CompilerLate *l) {
    if(l->failed)return 0;
    if(l->page==255)return !l->symbols&&!l->references?1:fail(l,ERR_FAILURE);
    if(page_role(l->page)!=PAGE_COMPILER||page_lifetime(l->page)!=l->lifetime||
       page_versions[l->page]!=l->version)return fail(l,ERR_FAILURE);
    return 1;
}
static uint8_t room(CompilerLate *l,uint16_t symbols,uint16_t refs) {
    if(!ready(l)||l->sealed)return fail(l,ERR_FAILURE);
    if((uint32_t)(l->symbols+symbols)*SYMBOL_BYTES+
       (uint32_t)(l->references+refs)*LATE_REFERENCE_BYTES>PAGE_SIZE)return fail(l,ERR_NO_MORE_MEMORY);
    if(l->page==255) {
        l->page=page_new(PAGE_COMPILER);
        if(l->page==255)return fail(l,ERR_NO_MORE_MEMORY);
        l->lifetime=page_lifetime(l->page);l->version=page_versions[l->page];
    }
    return 1;
}
static uint8_t get(CompilerLate *l,uint16_t at) {return far_get(l->page,at);}
static uint16_t word(CompilerLate *l,uint16_t at) {return get(l,at)|((uint16_t)get(l,at+1)<<8);}
static void put(CompilerLate *l,uint16_t at,uint8_t value) {far_set(l->page,at,value);}
static void put_word(CompilerLate *l,uint16_t at,uint16_t value) {put(l,at,(uint8_t)value);put(l,at+1,(uint8_t)(value>>8));}
static void touch(CompilerLate *l) {l->version=page_versions[l->page];}
static uint16_t ref_at(uint16_t index) {return PAGE_SIZE-(index+1u)*LATE_REFERENCE_BYTES;}
static uint16_t code_word(uint16_t at) {return cc_code_get(at)|((uint16_t)cc_code_get(at+1)<<8);}
void cc_late_init(CompilerLate *l) ROUTINES {memset(l,0,sizeof(*l));l->page=255;}
uint8_t cc_late_discard(CompilerLate *l) ROUTINES {
    if(l->page!=255) {
        if(page_role(l->page)!=PAGE_COMPILER||page_lifetime(l->page)!=l->lifetime)return fail(l,ERR_FAILURE);
        if(!page_drop(l->page))return 0;
    }
    cc_late_init(l);return 1;
}
uint16_t cc_late_symbol(CompilerLate *l,uint8_t kind,uint8_t id,uint8_t owner) ROUTINES {
    uint16_t i,at;
    if(!ready(l))return LATE_NONE;
    if(l->sealed) {fail(l,ERR_FAILURE);return LATE_NONE;}
    if(kind>LATE_ROUTINE||owner>cc.routine_count||
       (kind==LATE_ROOT&&(id>=LATE_ROOT_COUNT||owner))||
       (kind==LATE_BRIDGE&&owner)||
       ((kind==LATE_BRIDGE||kind==LATE_ROUTINE)&&(!id||id>cc.routine_count))) {
        fail(l,ERR_INVALID_PARAMETER);return LATE_NONE;
    }
    for(i=0;i<l->symbols;++i) {
        at=i*SYMBOL_BYTES;
        if(get(l,at+SY_KIND)==kind&&get(l,at+SY_ID)==id) {
            if(get(l,at+SY_OWNER)!=owner) {fail(l,ERR_INVALID_PARAMETER);return LATE_NONE;}
            return i;
        }
    }
    if(!room(l,1,0))return LATE_NONE;
    at=l->symbols*SYMBOL_BYTES;
    for(i=0;i<SYMBOL_BYTES;++i)put(l,at+i,0);
    put(l,at+SY_KIND,kind);put(l,at+SY_ID,id);put(l,at+SY_OWNER,owner);put(l,at+SY_STATE,1);
    i=l->symbols++;touch(l);return i;
}
uint8_t cc_late_bind(CompilerLate *l,uint16_t symbol,uint8_t owner,uint16_t value) ROUTINES {
    uint16_t at=symbol*SYMBOL_BYTES;
    if(!ready(l)||l->sealed)return fail(l,ERR_FAILURE);
    if(symbol>=l->symbols||value<CC_CODE_BASE||value>=cc.emitted||
       get(l,at+SY_OWNER)!=owner)return fail(l,ERR_INVALID_PARAMETER);
    if(get(l,at+SY_STATE)==2)return word(l,at+SY_VALUE)==value?1:fail(l,ERR_INVALID_PARAMETER);
    put_word(l,at+SY_VALUE,value);put(l,at+SY_STATE,2);touch(l);return 1;
}
uint8_t cc_late_value(CompilerLate *l,uint16_t symbol,uint16_t *value) ROUTINES {
    uint16_t at=symbol*SYMBOL_BYTES;
    if(!ready(l))return 0;
    if(symbol>=l->symbols)return fail(l,ERR_INVALID_PARAMETER);
    if(get(l,at+SY_STATE)!=2)return 0;
    *value=word(l,at+SY_VALUE);return 1;
}
static void reference(CompilerLate *l,uint16_t site,uint16_t symbol,uint16_t coefficient,uint8_t byte) {
    uint16_t at=ref_at(l->references++);
    put_word(l,at+RF_SITE,site);put_word(l,at+RF_SYMBOL,symbol);
    put_word(l,at+RF_COEFFICIENT,coefficient);put(l,at+RF_BYTE,byte);put(l,at+7,0);
}
uint8_t cc_late_reference(CompilerLate *l,uint16_t site,uint16_t symbol,uint16_t coefficient,uint8_t byte) ROUTINES {
    if(!ready(l))return 0;
    if(symbol>=l->symbols||byte>1||!coefficient||site<CC_CODE_BASE||
       (uint32_t)site+(byte?1u:2u)>cc.emitted)return fail(l,ERR_INVALID_PARAMETER);
    if(!room(l,0,1))return 0;
    reference(l,site,symbol,coefficient,byte);touch(l);return 1;
}
uint8_t cc_late_chain(CompilerLate *l,uint16_t first,uint16_t head,uint16_t symbol,uint16_t *parent_head) ROUTINES {
    uint16_t at=head,next,n=0;
    if(!ready(l))return 0;
    if(first<CC_CODE_BASE||first>cc.emitted||symbol>=l->symbols)return fail(l,ERR_INVALID_PARAMETER);
    while(at>=first) {
        if((uint32_t)at+2u>cc.emitted)return fail(l,ERR_INVALID_PARAMETER);
        next=code_word(at);
        if(cc.error->code||next>=at)return fail(l,ERR_INVALID_PARAMETER);
        at=next;++n;
    }
    if(at&&at<CC_CODE_BASE)return fail(l,ERR_INVALID_PARAMETER);
    if(n&&!room(l,0,n))return 0;
    at=head;
    while(n) {
        --n;
        next=code_word(at);reference(l,at,symbol,1,0);cc_word(at,0);at=next;
    }
    if(l->page!=255)touch(l);
    *parent_head=at;return 1;
}
uint8_t cc_late_range(CompilerLate *l,uint16_t first,uint16_t origin,uint16_t end) ROUTINES {
    uint16_t i,at,site;uint8_t byte;
    if(!ready(l))return 0;
    if(first>l->references||origin<CC_CODE_BASE||end>cc.emitted||origin>end)return fail(l,ERR_INVALID_PARAMETER);
    for(i=first;i<l->references;++i) {
        at=ref_at(i);site=word(l,at+RF_SITE);byte=get(l,at+RF_BYTE);
        if(byte>1||get(l,at+7)||word(l,at+RF_SYMBOL)>=l->symbols||
           !word(l,at+RF_COEFFICIENT)||site<origin||(uint32_t)site+(byte?1u:2u)>end)
            return fail(l,ERR_INVALID_PARAMETER);
    }
    return 1;
}
uint8_t cc_late_byte(CompilerLate *l,uint16_t index,uint8_t offset) ROUTINES {
    if(!ready(l))return 0;
    if(index>=l->references||offset>=LATE_REFERENCE_BYTES) {fail(l,ERR_INVALID_PARAMETER);return 0;}
    return get(l,ref_at(index)+offset);
}
uint8_t cc_late_retire(CompilerLate *l,uint16_t first) ROUTINES {
    if(!ready(l))return 0;
    if(first>l->references||l->sealed)return fail(l,ERR_INVALID_PARAMETER);
    l->references=first;return 1;
}
uint8_t cc_late_finish(CompilerLate *l) ROUTINES {
    uint16_t i,at,value,site;uint8_t byte;
    if(!ready(l)||l->sealed)return fail(l,ERR_FAILURE);
    if(!cc_late_range(l,0,CC_CODE_BASE,cc.emitted))return 0;
    /* No partial patch when an owner has not yet supplied its address. */
    for(i=0;i<l->references;++i)
        if(!cc_late_value(l,word(l,ref_at(i)+RF_SYMBOL),&value))return fail(l,ERR_FAILURE);
    for(i=0;i<l->references;++i) {
        at=ref_at(i);site=word(l,at+RF_SITE);byte=get(l,at+RF_BYTE);
        cc_late_value(l,word(l,at+RF_SYMBOL),&value);
        value=(uint16_t)((uint32_t)value*word(l,at+RF_COEFFICIENT));
        if(byte)cc_put(site,(uint8_t)(cc_code_get(site)+value));
        else cc_word(site,(uint16_t)(code_word(site)+value));
    }
    if(cc.error->code)return fail(l,ERR_FAILURE);
    l->sealed=1;return 1;
}
static uint16_t root_symbol(CompilerGroups *g,uint8_t kind,uint8_t id) {
    uint16_t i,at;
    if(!ready(&g->late))return LATE_NONE;
    for(i=0;i<g->late.symbols;++i) {
        at=i*SYMBOL_BYTES;
        if(get(&g->late,at+SY_KIND)==kind&&get(&g->late,at+SY_ID)==id)return i;
    }
    return LATE_NONE;
}
static void compiler_failure(void) {cc_fail(99);}
uint8_t cc_root_member(void) ROUTINES {return cc.sources->groups&&cc.sources->groups->active;}
void cc_root_request(uint8_t kind,uint8_t id) ROUTINES {
    CompilerGroups *g=cc.sources->groups;
    if(!g||cc.error->code||cc.suppress)return;
    if(cc_late_symbol(&g->late,kind,id,0)==LATE_NONE)compiler_failure();
}
uint8_t cc_root_pending(uint8_t kind,uint8_t id) ROUTINES {
    CompilerGroups *g=cc.sources->groups;uint16_t symbol;
    if(!g)return 0;
    symbol=root_symbol(g,kind,id);
    if(g->late.failed) {compiler_failure();return 0;}
    return symbol!=LATE_NONE&&get(&g->late,symbol*SYMBOL_BYTES+SY_STATE)==1;
}
void cc_root_bind(uint8_t kind,uint8_t id,uint16_t value) ROUTINES {
    CompilerGroups *g=cc.sources->groups;uint16_t symbol;
    if(!g||cc.error->code||cc.suppress)return;
    if(g->active) {cc_fail(99);return;}
    symbol=root_symbol(g,kind,id);
    if(g->late.failed)compiler_failure();
    else if(symbol!=LATE_NONE&&!cc_late_bind(&g->late,symbol,0,value))compiler_failure();
}
void cc_root_operand(uint8_t kind,uint8_t id,uint16_t value,uint16_t addend) ROUTINES {
    CompilerGroups *g=cc.sources->groups;uint16_t symbol=LATE_NONE,at=cc.emitted;
    if(cc.error->code||cc.suppress)return;
    if(!value) {
        if(!g||!g->active) {cc_fail(99);return;}
        symbol=cc_late_symbol(&g->late,kind,id,0);
        if(symbol==LATE_NONE) {compiler_failure();return;}
    }
    value+=addend;cc_relocate(at);cc_emit((uint8_t)value);cc_emit((uint8_t)(value>>8));
    if(symbol!=LATE_NONE&&!cc.error->code&&!cc_late_reference(&g->late,at,symbol,1,0))compiler_failure();
}
void cc_root_address(uint8_t opcode,uint8_t kind,uint8_t id,uint16_t value,uint16_t addend) ROUTINES {
    cc_emit(opcode);cc_root_operand(kind,id,value,addend);
}
