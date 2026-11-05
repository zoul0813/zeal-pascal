#include "compiler_units.h"
#include "compiler_job.h"
#include "library.h"
#include "compiler_disk.h"
#include "output_transport.h"
#include <zos_errors.h>

/* Only the synchronous invocation writes this compiler-private owner. A
 * failed cleanup retains its capability for retry; no transient stack pointer
 * escapes. Artifact descriptors remain owned by the caller. */
static struct {
    uint8_t page,initialized;uint32_t lifetime;
    struct {uint8_t page,role;uint32_t lifetime;} pending[3];
} owner;
static uint8_t requested;
static int8_t output_descriptor;
static uint8_t ready(void) {
    if(!owner.lifetime||page_role(owner.page)!=PAGE_COMPILER||page_lifetime(owner.page)!=owner.lifetime) {
        cc_fail(COMPILE_PLATFORM_FAILURE);return 0;
    }
    return 1;
}
uint8_t cc_unit_byte(uint16_t at) ROUTINES {
    if(at>=PAGE_SIZE||!ready()){cc_fail(99);return 0;}return far_get(owner.page,at);
}
void cc_unit_set(uint16_t at,uint8_t value) ROUTINES {
    if(at>=PAGE_SIZE||!ready()){cc_fail(99);return;}far_set(owner.page,at,value);
}
uint16_t cc_unit_word(uint16_t at) ROUTINES {
    uint16_t value=cc_unit_byte(at);return value|((uint16_t)cc_unit_byte(at+1)<<8);
}
void cc_unit_put(uint16_t at,uint16_t value) ROUTINES {
    cc_unit_set(at,(uint8_t)value);cc_unit_set(at+1,(uint8_t)(value>>8));
}
uint16_t cc_unit_record(uint16_t kind,uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint16_t e) ROUTINES {
    uint16_t at=cc_unit_word(UH_USED);
    if(cc.error->code)return 0;
    if(at<UNIT_RECORD_BASE||(uint32_t)at+UNIT_RECORD_BYTES>PAGE_SIZE){cc_fail(99);return 0;}
    cc_unit_put(at,kind);cc_unit_put(at+2,a);cc_unit_put(at+4,b);
    cc_unit_put(at+6,c);cc_unit_put(at+8,d);cc_unit_put(at+10,e);
    cc_unit_put(UH_RECORDS,cc_unit_word(UH_RECORDS)+1);cc_unit_put(UH_USED,at+UNIT_RECORD_BYTES);
    return at;
}
uint8_t cc_units_requested(void) ROUTINES {return requested;}
int8_t cc_units_output(void) ROUTINES {return output_descriptor;}
uint8_t cc_units_begin(void) ROUTINES {
    uint16_t at;
    if(cc.sources->unit_page!=255)return ready();
    if(owner.lifetime&&!cc_units_close())return 0;
    owner.page=page_new(PAGE_COMPILER);if(owner.page==255){cc_fail(99);return 0;}
    owner.lifetime=page_lifetime(owner.page);owner.initialized=0;
    if(!far_fill(owner.page,0,0,256)){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    owner.initialized=1;cc_unit_put(UH_USED,UNIT_RECORD_BASE);
    for(at=256;at<PAGE_SIZE;at+=256) {
        if(!cc_poll()||!far_fill(owner.page,at,0,256)){if(!cc.error->code)cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    }
    page_initialized(owner.page,PAGE_SIZE);cc.sources->unit_page=owner.page;
    cc.sources->unit_start=cc.emitted;cc.sources->unit_owner=0;cc.sources->unit_real=0;return 1;
}
uint8_t cc_units_close(void) ROUTINES {
    uint16_t at,end;uint8_t p,ok=1,i;uint32_t life;
    if(!owner.lifetime)return 1;
    if(!ready())return 0;
    for(i=0;i<3;++i)if(owner.pending[i].lifetime) {
        p=owner.pending[i].page;
        if(page_role(p)!=owner.pending[i].role||page_lifetime(p)!=owner.pending[i].lifetime||!page_drop(p))ok=0;
        else owner.pending[i].lifetime=0;
    }
    if(owner.initialized) {
        end=cc_unit_word(UH_USED);
        if(end<UNIT_RECORD_BASE||end>PAGE_SIZE||(end-UNIT_RECORD_BASE)%UNIT_RECORD_BYTES){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
        for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_MODULE) {
            p=(uint8_t)cc_unit_word(at+2);if(p==255)continue;
            if(at+2u*UNIT_RECORD_BYTES>end||cc_unit_word(at+UNIT_RECORD_BYTES)!=UR_SEAL){ok=0;continue;}
            life=cc_unit_word(at+UNIT_RECORD_BYTES+2)|((uint32_t)cc_unit_word(at+UNIT_RECORD_BYTES+4)<<16);
            if(page_role(p)!=PAGE_COMPILER||page_lifetime(p)!=life||!page_drop(p))ok=0;
            else cc_unit_put(at+2,255);
        }
    }
    if(!ok||!page_drop(owner.page)){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    memset(&owner,0,sizeof(owner));cc.sources->unit_page=255;return 1;
}
void cc_units_note(uint16_t kind,uint16_t site,uint16_t value) ROUTINES {
    if(!cc.sources->unit_mode||cc.suppress||cc.error->code)return;
    {uint16_t at=cc_unit_record(kind,site,value,0,0,0),tail;
    if(kind==UR_GLOBAL&&at) {
        tail=cc_unit_word(UH_REFERENCE_TAIL);
        if(tail)cc_unit_put(tail+6,at);else cc_unit_put(UH_REFERENCES,at);
        cc_unit_put(UH_REFERENCE_TAIL,at);cc_unit_put(UH_REFERENCE_COUNT,cc_unit_word(UH_REFERENCE_COUNT)+1);
    }}
}
void cc_units_global(uint8_t opcode,uint16_t address) ROUTINES {
    cc_units_note(UR_GLOBAL,cc.emitted+1,address);cc_op16(opcode,address);
}
void cc_units_owner(uint16_t scope) ROUTINES {
    if(!cc.sources->unit_mode||cc.sources->unit_owner==scope||cc.error->code)return;
    if(cc.emitted>cc.sources->unit_start)
        cc_unit_record(UR_FRAGMENT,cc.sources->unit_start,cc.emitted,cc.sources->unit_owner,cc.sources->unit_real,0);
    cc.sources->unit_start=cc.emitted;cc.sources->unit_owner=scope;cc.sources->unit_real=0;
}
static uint8_t primitive(TypeId type) {
    return type==K_INTEGER||type==K_BOOLEAN||type==K_CHAR||type==K_BYTE||type==K_REAL;
}
void cc_units_export(uint16_t symbol,uint16_t scope) ROUTINES {
    uint16_t first=cc_rget(scope,R_PARAMETERS),count=cc_rget(scope,R_COUNT),i,end=cc_unit_word(UH_USED);
    for(i=UNIT_RECORD_BASE;i<end;i+=UNIT_RECORD_BYTES)
        if(cc_unit_word(i)==UR_EXPORT&&cc_unit_word(i+2)==symbol){cc_fail(UNIT_INTERFACE);return;}
    if(CCS(symbol,role)==S_FUNCTION&&!primitive(CCS(symbol,type))){cc_fail(UNIT_UNSUPPORTED);return;}
    for(i=0;i<count;++i)if(!primitive(CCS(first+i,type))&&
        !(CCS(first+i,role)==S_REFERENCE&&CCS(first+i,type)==TYPE_NONE)){cc_fail(UNIT_UNSUPPORTED);return;}
    cc_unit_record(UR_EXPORT,symbol,scope,0,0,0);
}
void cc_units_header(void) ROUTINES {
    uint8_t i,n;
    if(!cc_is("unit")||!requested){cc_fail(UNIT_INTERFACE);return;}
    if(!cc_units_begin())return;
    cc_next();if(cc.token!=T_ID){cc_fail(53);return;}
    n=(uint8_t)strlen(cc.name);
    /* Zeal's initial canonical lookup uses UPPERCASE.ZPU and never truncates
     * identities. Longer-name/output mapping needs a separate search contract. */
    if(!n||n>11){cc_fail(UNIT_UNSUPPORTED);return;}
    cc_unit_put(UH_NAME_LENGTH,n);for(i=0;i<n;++i)cc_unit_set(UH_NAME+i,(uint8_t)cc.name[i]);
    cc_next();cc_expect(';',1);cc_keyword("interface",UNIT_INTERFACE);cc_units_interface();
}
uint8_t compiler_compile_unit(const Source *source,Build *scratch,Diagnostic *diagnostic,
                     int8_t output,uint8_t interruptible) {
    uint8_t ok;
    if(requested||compiler_invocation||compiler_disk_requested||output<0){memset(diagnostic,0,sizeof(*diagnostic));diagnostic->code=COMPILE_PLATFORM_FAILURE;return 0;}
    requested=1;output_descriptor=output;
    ok=compile_project(source,scratch,diagnostic,interruptible,0);
    requested=0;output_descriptor=-1;return ok;
}

uint16_t cc_unit_owned_page(void) ROUTINES {
    uint16_t module,seal;uint8_t p;uint32_t life;
    module=cc_unit_record(UR_MODULE,255,0,0,0,0);if(!module)return 0;
    seal=cc_unit_record(UR_SEAL,0,0,0,0,0);if(!seal)return 0;
    p=page_new(PAGE_COMPILER);if(p==255){cc_fail(99);return 0;}
    life=page_lifetime(p);cc_unit_put(seal+2,(uint16_t)life);cc_unit_put(seal+4,(uint16_t)(life>>16));
    cc_unit_put(module+2,p);return module;
}
uint8_t cc_unit_module_ready(uint16_t module) ROUTINES {
    uint8_t p=(uint8_t)cc_unit_word(module+2);uint16_t seal=module+UNIT_RECORD_BYTES;
    uint32_t life=cc_unit_word(seal+2)|((uint32_t)cc_unit_word(seal+4)<<16);
    uint32_t version=cc_unit_word(seal+6)|((uint32_t)cc_unit_word(seal+8)<<16);
    if(p>=64||page_role(p)!=PAGE_COMPILER||page_lifetime(p)!=life||page_versions[p]!=version) {
        cc_fail(UNIT_FORMAT);return 0;
    }
    return 1;
}
uint8_t cc_unit_module_byte(uint16_t module,uint16_t at) ROUTINES {
    if(at>=cc_unit_word(module+4)||!cc_unit_module_ready(module)){cc_fail(UNIT_FORMAT);return 0;}
    return far_get((uint8_t)cc_unit_word(module+2),at);
}
uint16_t cc_unit_module_word(uint16_t module,uint16_t at) ROUTINES {
    uint16_t v=cc_unit_module_byte(module,at);return v|((uint16_t)cc_unit_module_byte(module,at+1)<<8);
}
uint8_t cc_unit_fragment(uint16_t preferred,uint16_t *fragment,uint16_t *offset) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED),id=0,first,last;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_FRAGMENT) {
        first=cc_unit_word(at+2);last=cc_unit_word(at+4);
        if(preferred>=first&&preferred<last){*fragment=id;*offset=preferred-first;return 1;}++id;
    }
    cc_fail(UNIT_FORMAT);return 0;
}

uint8_t cc_units_hold(uint8_t page,uint32_t lifetime,uint8_t role) ROUTINES {
    uint8_t i;
    if(!ready())return 0;
    for(i=0;i<3;++i)if(!owner.pending[i].lifetime) {
        owner.pending[i].page=page;owner.pending[i].role=role;owner.pending[i].lifetime=lifetime;return 1;
    }
    cc_fail(COMPILE_PLATFORM_FAILURE);return 0;
}
