#include "compiler.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#endif

/* Raw Inline operands describe storage slots, not loads. Recursive slots
 * retain static workspace addresses; a VAR slot contains the referent pointer.
 * Routine operands are native entries (nested callers supply the lexical link),
 * while Addr's captured nested thunk remains a Pascal-callable pointer value. */
static uint16_t address(uint16_t id,uint16_t *coefficient) {
    uint16_t scope;
    while(CCS(id,role)==S_ALIAS)id=CCS(id,address);
    if(CCS(id,role)==S_PROCEDURE||CCS(id,role)==S_FUNCTION||CCS(id,role)==S_RESULT) {
        scope=CCS(id,role)==S_RESULT?CCS(id,owner):CCS(id,initial_type);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc_rget(scope,R_FLAGS)&RF_UNIT_IMPORT){cc_fail(UNIT_UNSUPPORTED);return 0;}
#endif
        *coefficient=(cc_rget(scope,R_FLAGS)&RF_EXTERNAL)?0:1;
        return cc_rget(scope,R_ENTRY);
    }
    if(CCS(id,role)==S_TYPED) {*coefficient=1;return CCS(id,address);}
    if(CCS(id,role)==S_VARIABLE||CCS(id,role)==S_VALUE||CCS(id,role)==S_REFERENCE||
       CCS(id,role)==S_SYSTEM||CCS(id,role)==S_ADDRESS||CCS(id,role)==S_STANDARD||CCS(id,role)==S_HOOK) {
        cc_bind_storage(id);return CCS(id,address);
    }
    cc_fail(74);return 0;
}
static uint16_t data(uint16_t *coefficient,uint8_t *word) {
    int16_t id;
    uint16_t value=0;
    uint8_t sign=0;TypeId family;
    *coefficient=0;
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
            if(family!=K_INTEGER)cc_fail(sign&&family!=K_REAL?25:22);
            value=CCS(id,address);
        } else if(id<0&&cc_is("maxint"))value=32767;
        else if(id<0&&(id=cc_predefined_color())>=0)value=(uint16_t)id;
        else if(id<0&&(cc_is("true")||cc_is("false")||cc_is("pi")))cc_fail(sign&&!cc_is("pi")?25:22);
        else if(sign)cc_fail(25);
        else if(id>=0) {*word=1;value=address((uint16_t)id,coefficient);}
        else cc_fail(74);
    } else if(cc.token=='*'&&!sign) {*word=1;value=cc.emitted;*coefficient=1;}
    else if(cc.token==T_REAL)cc_fail(22);
    else if(cc.token==T_STRING||cc.token=='^')cc_fail(sign?25:22);
    else cc_fail(sign?25:74);
    if(sign==2)value=(uint16_t)(0u-value);
    cc_next();return value;
}
void cc_inline(void) FRONTEND {
    uint16_t value,term,coefficient,part;
    uint8_t force;uint8_t word;uint8_t subtract;
    if(!cc.suppress)cc.console_dynamic=1;
    cc_require_real();
    cc_next();cc_expect('(',4);
    cc_guard(); /* No generated instruction may split the raw byte stream. */
    do {
        force=0;word=0;coefficient=0;value=0;subtract=0;
        if(cc.token=='<'||cc.token=='>') {force=cc.token;cc_next();}
        for(;;) {
            term=data(&part,&word);
            if(subtract) {value-=term;coefficient-=part;}
            else {value+=term;coefficient+=part;}
            if(cc.error->code|| (cc.token!='+'&&cc.token!='-'))break;
            subtract=cc.token=='-';cc_next();
        }
        word=force=='<'?0:force=='>'||word||value>255;
        cc_relocate_value(cc.emitted,coefficient,!word);
        cc_emit((uint8_t)value);if(word)cc_emit((uint8_t)(value>>8));
        if(cc.token!='/')break;
        cc_next();
    } while(!cc.error->code);
    cc_expect(')',5);
}
