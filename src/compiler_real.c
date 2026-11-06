#include "compiler.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#endif

void cc_require_full_memory(void) FRONTEND {
    if(cc.suppress||cc.error->code)return;
    cc.full_memory=1;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode)cc.sources->unit_real|=ZPU_FULL_MEMORY;
#endif
}

#if defined(ZP_OPTIONAL_REAL) || defined(ZP_UNITS_EXPERIMENT)
void cc_require_real(void) FRONTEND {
    if(cc.suppress||cc.error->code)return;
#ifdef ZP_OPTIONAL_REAL
    cc.real_needed=1;
#endif
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode)cc.sources->unit_real|=ZPU_REAL;
#endif
}
#endif

static uint8_t source_byte(SourcePosition at) { return cc_source_byte(at); }
static uint8_t digit(uint8_t c) { return c>='0'&&c<='9'; }
/* Real constant references retain the original literal and one sign bit.
 * Re-decoding does not change the current token, name, or string literal. */
void cc_real_literal(uint16_t reference) FRONTEND {
    static const uint8_t pi[6]={130,33,162,218,15,73};
    uint8_t text[128];uint8_t n=0;uint8_t i;
    SourcePosition at;
    uint16_t id=reference&0x7fffu;
    if(id==REAL_PI) { zr_error=0; for(i=0;i<6;++i) zr_result[i]=pi[i]; }
    else {
        at=cc_reference_position(id);
#ifdef ZP_UNITS_EXPERIMENT
        if(at&0x80000000UL) {
            zr_error=0;for(i=0;i<6;++i)zr_result[i]=source_byte(at+i);
            goto decoded;
        }
#endif
        while(digit(source_byte(at))&&n<127) text[n++]=source_byte(at++);
        if(source_byte(at)=='.'&&source_byte(at+1)!='.') {
            if(n<127) text[n++]=source_byte(at++);
            while(digit(source_byte(at))&&n<127) text[n++]=source_byte(at++);
        }
        if(source_byte(at)=='e'||source_byte(at)=='E') {
            if(n<127) text[n++]=source_byte(at++);
            if((source_byte(at)=='+'||source_byte(at)=='-')&&n<127) text[n++]=source_byte(at++);
            if(digit(source_byte(at))&&n<127) text[n++]=source_byte(at++);
            if(digit(source_byte(at))&&n<127) text[n++]=source_byte(at++);
        }
        zr_op=ZR_DECIMAL; zr_text=text; zr_length=n; zr_eval();
        if(zr_error) cc_fail(57);
    }
#ifdef ZP_UNITS_EXPERIMENT
 decoded:
#endif
    if((reference&0x8000u)&&zr_result[0]) zr_result[5]^=128;
}
void cc_real_constant(TypeId type,uint16_t value) FRONTEND {
    if(cc_family(type)==K_REAL) cc_real_literal(value);
    else { zr_op=ZR_FROM_INT; zr_integer=(int16_t)value; zr_eval(); }
}
void cc_real_emit(uint16_t reference) FRONTEND {
    uint8_t i;
    uint16_t address=cc.emitted+3;
    cc_real_literal(reference);
    cc_address(0xc3,address+6);
    for(i=0;i<6;++i) cc_emit(zr_result[i]);
    cc_address(0x21,address);
}
void cc_real_hold(void) FRONTEND {
    cc_temporary(6); cc_op16(0x01,6); cc_call(RT_MOVE);
    cc_op16(0x21,0); cc_emit(0x39);
}
static void math_call(uint8_t op) {
    cc_require_real();
    cc_emit(0x3e); cc_emit(op);
    cc_reserve(256); cc_op16(0xcd,RT_REAL);
}
void cc_real_unary(uint8_t op,TypeId type,uint16_t before) FRONTEND {
    if(!cc_numeric(type)) { cc_fail(23); return; }
    cc_emit(0x54); cc_emit(0x5d);
    math_call(op|(cc_family(type)==K_INTEGER?0xc0:0));
    cc_release(before);
    if(op!=ZR_TRUNC&&op!=ZR_ROUND) cc_real_hold();
}
void cc_real_binary(uint8_t op,TypeId left,TypeId right,uint16_t before,uint16_t right_bytes) FRONTEND {
    if(!cc_numeric(left)||!cc_numeric(right)) { cc_fail(47); return; }
    math_call(op|(cc_family(left)==K_INTEGER?0x40:0)|(cc_family(right)==K_INTEGER?0x80:0));
    if(right_bytes) cc_drop_stack(2);
    cc_release(before);
    if(op!=ZR_COMPARE) cc_real_hold();
}
