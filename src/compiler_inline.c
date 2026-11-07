#include "compiler.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#endif

/* Raw Inline operands describe storage slots, not loads. Recursive slots
 * retain static workspace addresses; a VAR slot contains the referent pointer.
 * Routine operands are native entries (nested callers supply the lexical link),
 * while Addr's captured nested thunk remains a Pascal-callable pointer value. */
typedef struct { uint16_t code,global; } InlineDelta;
static uint16_t address(uint16_t id,InlineDelta *delta) {
    uint16_t scope;
    while(CCS(id,role)==S_ALIAS)id=CCS(id,address);
    if(CCS(id,role)==S_PROCEDURE||CCS(id,role)==S_FUNCTION||CCS(id,role)==S_RESULT) {
        scope=CCS(id,role)==S_RESULT?CCS(id,owner):CCS(id,initial_type);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc_rget(scope,R_FLAGS)&RF_UNIT_IMPORT){cc_fail(UNIT_UNSUPPORTED);return 0;}
#endif
        delta->code=(cc_rget(scope,R_FLAGS)&RF_EXTERNAL)?0:1;
        return cc_rget(scope,R_ENTRY);
    }
    if(CCS(id,role)==S_TYPED) {delta->code=1;return CCS(id,address);}
    if(CCS(id,role)==S_VARIABLE||CCS(id,role)==S_VALUE||CCS(id,role)==S_REFERENCE||
       CCS(id,role)==S_SYSTEM||CCS(id,role)==S_ADDRESS||CCS(id,role)==S_STANDARD||CCS(id,role)==S_HOOK) {
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&(CCS(id,role)==S_SYSTEM||CCS(id,role)==S_STANDARD||CCS(id,role)==S_HOOK)) {
            cc_fail(UNIT_UNSUPPORTED);return 0;
        }
#endif
        cc_bind_storage(id);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&CCS(id,role)!=S_ADDRESS&&CCS(id,address)>=GLOBAL_BASE)delta->global=1;
#endif
        return CCS(id,address);
    }
    cc_fail(74);return 0;
}
static uint16_t data(InlineDelta *delta,uint8_t *word) {
    int16_t id;
    uint16_t value=0;
    uint8_t sign=0;TypeId family;
    delta->code=delta->global=0;
    if(cc.token=='+'||cc.token=='-') {sign=cc.token=='-'?2:1;cc_next();}
    if(cc.token==T_NUMBER) {
        if(cc.number_overflow
#ifdef ZP_UNITS_EXPERIMENT
           ||cc.unsigned_magnitude>65535UL
#endif
           ||(!cc.hexadecimal&&cc.magnitude>32767u&&
           !(sign==2&&cc.magnitude==32768u)))cc_fail(56);
        value=cc.magnitude;
    } else if(cc.token==T_ID) {
        id=cc_lookup();
        if(id>=0&&CCS(id,role)==S_CONSTANT) {
            family=cc_family(CCS(id,type));
            if(family!=K_INTEGER
#ifdef ZP_UNITS_EXPERIMENT
               &&family!=K_WORD
#endif
            )cc_fail(sign&&family!=K_REAL?25:22);
            value=CCS(id,address);
        } else if(id<0&&cc_is("maxint"))value=32767;
        else if(id<0&&(id=cc_predefined_color())>=0)value=(uint16_t)id;
        else if(id<0&&(cc_is("true")||cc_is("false")||cc_is("pi")))cc_fail(sign&&!cc_is("pi")?25:22);
        else if(sign)cc_fail(25);
        else if(id>=0) {*word=1;value=address((uint16_t)id,delta);}
        else cc_fail(74);
    } else if(cc.token=='*'&&!sign) {*word=1;value=cc.emitted;delta->code=1;}
    else if(cc.token==T_REAL)cc_fail(22);
    else if(cc.token==T_STRING||cc.token=='^')cc_fail(sign?25:22);
    else cc_fail(sign?25:74);
    if(sign==2)value=(uint16_t)(0u-value);
    cc_next();return value;
}
void cc_inline(void) FRONTEND {
    uint16_t value,term;
    InlineDelta delta,part;
    uint8_t force;uint8_t word;uint8_t subtract;
#ifdef ZP_UNITS_EXPERIMENT
    cc_require_hardware();
    /* A unit records raw access explicitly. Ordinary opaque Inline retains
     * its historical conservative Real requirement. */
    if(!cc.sources->unit_mode)cc_require_real();
#else
    if(!cc.suppress)cc.console_dynamic=1;
    cc_require_real();
#endif
    cc_next();cc_expect('(',4);
    cc_guard(); /* No generated instruction may split the raw byte stream. */
    do {
        force=0;word=0;delta.code=delta.global=0;value=0;subtract=0;
        if(cc.token=='<'||cc.token=='>') {force=cc.token;cc_next();}
        for(;;) {
            term=data(&part,&word);
            if(subtract) {value-=term;delta.code-=part.code;delta.global-=part.global;}
            else {value+=term;delta.code+=part.code;delta.global+=part.global;}
            if(cc.error->code|| (cc.token!='+'&&cc.token!='-'))break;
            subtract=cc.token=='-';cc_next();
        }
        word=force=='<'?0:force=='>'||word||value>255;
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&delta.global) {
            if(delta.global!=1||delta.code||!word)cc_fail(UNIT_UNSUPPORTED);
            else cc_units_note(UR_GLOBAL,cc.emitted,value);
        } else
#endif
        cc_relocate_value(cc.emitted,delta.code,!word);
        cc_emit((uint8_t)value);if(word)cc_emit((uint8_t)(value>>8));
        if(cc.token!='/')break;
        cc_next();
    } while(!cc.error->code);
    cc_expect(')',5);
}
