#include "compiler.h"

static uint8_t expression(void);
static uint8_t set_constructor(void) {
    uint8_t base=255,kind,upper;
    uint16_t before;
    cc_temporary(32); cc_op16(0x21,0); cc_emit(0x39); cc_call(RT_SETZERO); cc_next();
    while(cc.token!=']'&&!cc.error->code) {
        before=cc.temporaries; kind=expression();
        if(!cc_ordinal(kind)) cc_fail(44);
        if(base==255) base=cc_family(kind); else if(base!=cc_family(kind)) cc_fail(44);
        cc_release(before); cc_guard(); cc_emit(0xe5);
        if(cc.token==T_RANGE) {
            cc_next(); upper=expression();
            if(!cc_ordinal(upper)||cc_family(upper)!=base) cc_fail(44);
            cc_release(before);
        }
        cc_emit(0xeb); cc_op16(0x21,2); cc_emit(0x39);
        cc_emit(0x44); cc_emit(0x4d); cc_emit(0xe1); cc_call(RT_SETADD);
        if(cc.token!=',') break; cc_next();
    }
    cc_expect(']',9); cc_op16(0x21,0); cc_emit(0x39);
    return base==255?K_EMPTY_SET:K_SET_EXPR+base;
}
static uint8_t set_binary(uint8_t left,uint8_t right,uint8_t op,uint16_t before,uint16_t right_bytes) {
    if(cc_family(right)!=K_SET||!cc_compatible(left,right)) cc_fail(44);
    cc_op16(0x01,cc.temporaries-before+(right_bytes?2:0)); cc_emit(0x3e); cc_emit(op); cc_call(RT_SETBIN);
    cc.temporaries=before+32;
    return cc_set_base(left)==255?right:left;
}
uint8_t cc_index(uint8_t type) BACKEND {
    uint16_t before,stride;
    uint8_t kind,index,array;
    int16_t field;
    while(!cc.error->code) {
        if(cc_family(type)==K_POINTER&&cc.token=='^') {
            cc_load_indirect(type); type=cc.types[type].element; cc_next(); continue;
        }
        if(cc_family(type)==K_RECORD&&cc.token=='.') {
            cc_next(); field=cc.token==T_ID?cc_field_lookup(type):-1;
            if(field<0) { cc_fail(41); return type; }
            if(cc.field_offset) { cc_op16(0x11,cc.field_offset); cc_emit(0x19); }
            type=cc.symbols[field].type; cc_next(); continue;
        }
        if((cc_family(type)!=K_STRING&&cc_family(type)!=K_ARRAY)||cc.token!='[') break;
        cc_next();
        for(;;) {
            array=cc_family(type)==K_ARRAY;
            index=array?cc.types[type].capacity:K_INTEGER;
            cc_guard(); cc_emit(0xe5); before=cc.temporaries;
            kind=expression();
            if(cc_family(kind)!=cc_family(index)) cc_fail(array?44:23);
            cc_release(before);
            if(cc.range_check) {
                cc_op16(0x11,array?(uint16_t)cc.types[index].low:0);
                cc_op16(0x01,array?(uint16_t)cc.types[index].high:cc.types[type].capacity);
                cc_call(RT_INDEX);
            }
            if(array) {
                if(cc.types[index].low) {
                    cc_op16(0x11,(uint16_t)cc.types[index].low); cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52);
                }
                type=cc.types[type].element; stride=cc.types[type].width;
                if(stride!=1) { cc_op16(0x11,stride); cc_call(RT_MUL); }
            } else type=K_CHAR;
            cc_emit(0xd1); cc_emit(0x19);
            if(cc.token!=','||cc_family(type)!=K_ARRAY) break;
            cc_next();
        }
        cc_expect(']',9);
    }
    return type;
}
uint8_t cc_selection(uint16_t id) BACKEND {
    uint8_t type=cc.symbols[id].type;
    cc_symbol_address(id); cc_next();
    return cc.symbols[id].role==S_RESULT?type:cc_index(type);
}
/* Mem is addressable byte storage; Port is an I/O operation, never a Var. */
static uint8_t raw_kind(void) {
    return cc_is("mem")?1:cc_is("port")?2:0;
}
static void raw_index(void) {
    uint16_t before=cc.temporaries;
    cc_next(); cc_expect('[',8);
    if(cc_family(expression())!=K_INTEGER) cc_fail(23);
    cc_release(before); cc_expect(']',9);
}
uint8_t cc_reference(uint8_t code) BACKEND {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    if(id<0&&cc_is("mem")) { raw_index(); return K_BYTE; }
    if((id<0&&cc_is("port"))||(id>=0&&cc.symbols[id].role==S_RESULT)) { cc_fail(41); return K_INTEGER; }
    if(!cc_variable(id)) { cc_fail(code); return K_INTEGER; }
    if(cc.symbols[id].control) { cc_fail(44); return K_INTEGER; }
    return cc_selection((uint16_t)id);
}
/* Array values are variable selections or an exact-length string constant
 * for a Char array. They do not acquire general scalar-expression semantics. */
uint8_t cc_aggregate_actual(uint8_t type) BACKEND {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    uint8_t actual;
    uint16_t value;
    if(cc_char_array(type)&&(cc.token==T_STRING||(id>=0&&cc.symbols[id].role==S_CONSTANT))) {
        cc_constant(&actual,&value);
        if(actual!=K_STRING) cc_fail(60);
        else {
            cc_constant_literal(value);
            if(cc.literal_length!=cc.types[type].width) cc_fail(60);
            cc_emit_literal(); cc_emit(0x23); /* raw characters after the length */
        }
    } else if(cc.token==T_STRING) cc_fail(44);
    else if(cc_variable(id)) {
        actual=cc_selection((uint16_t)id);
        if(actual!=type) cc_fail(44);
    } else cc_fail(41);
    return type;
}
enum { F_ORD,F_CHR,F_SUCC,F_PRED,F_ABS,F_SQR,F_ODD,F_HI,F_LO,F_SWAP,F_UPCASE,F_SIZEOF,F_ADDR,F_LENGTH,F_COPY,F_POS,F_CONCAT,F_PTR,F_MEMAVAIL,F_MAXAVAIL,F_STACKPTR,F_TRUNC,F_ROUND,F_INT,F_FRAC,F_SQRT,F_LN,F_EXP,F_SIN,F_COS,F_ARCTAN };
static void string_value(void) {
    uint16_t before=cc.temporaries;
    uint8_t kind;
    if(cc.token==')'||cc.token==','||cc.token==T_EOF) { cc_fail(41); return; }
    kind=cc_family(cc_text_value(expression()));
    if(kind==K_CHAR) {
        cc_temporary(2); cc_emit(0x3e); cc_emit(1); cc_emit(0x12);
        cc_emit(0x13); cc_emit(0x7d); cc_emit(0x12); cc_op16(0x21,0); cc_emit(0x39);
    } else if(kind!=K_STRING) cc_fail(33);
    else if(cc.temporaries==before) { cc_call(RT_HOLDSTRING); cc.temporaries+=256; }
}
static void integer_value(void) { if(cc_family(expression())!=K_INTEGER) cc_fail(23); }
void cc_string_value(void) BACKEND { string_value(); }
static void push_value(void) { cc_guard(); cc_emit(0xe5); }
static uint8_t string_function(uint8_t op) {
    uint16_t before=cc.temporaries,right_before,right_bytes,owned;
    string_value();
    if(op==F_CONCAT) {
        while(cc.token==','&&!cc.error->code) {
            push_value(); right_before=cc.temporaries; cc_next(); string_value();
            right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
            cc_op16(0x01,cc.temporaries-before+(right_bytes?2:0)); cc_emit(0x3e); cc_emit(0);
            cc_call(RT_CONCAT); cc.temporaries=before+256;
        }
        cc_expect(')',5); return K_STRING;
    }
    push_value(); cc_expect(',',3);
    if(op==F_POS) {
        right_before=cc.temporaries; string_value(); cc_expect(')',5);
        right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes); cc_call(RT_POS);
        if(right_bytes) cc_drop_stack(2);
        cc_release(before); return K_INTEGER;
    }
    integer_value(); push_value(); cc_expect(',',3); integer_value(); push_value(); cc_expect(')',5);
    owned=cc.temporaries-before; cc_temporary(256);
    cc_op16(0x21,256); cc_emit(0x39); cc_op16(0x11,owned+6); cc_call(RT_SUBSTRING);
    cc.temporaries=before+256; return K_STRING;
}
static uint8_t string_target(void) {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    uint8_t type;
    if(id>=0&&cc.symbols[id].role==S_RESULT) { cc_fail(41); return K_STRING; }
    if(!cc_variable(id)||cc.symbols[id].control) { cc_fail(34); return K_STRING; }
    type=cc_selection((uint16_t)id);
    if(cc_family(type)!=K_STRING) cc_fail(34);
    return type;
}
void cc_string_statement(uint8_t insert) BACKEND {
    uint16_t before=cc.temporaries;
    uint8_t type=K_STRING;
    cc_next(); cc_expect('(',4);
    if(insert) { string_value(); push_value(); cc_expect(',',3); }
    type=string_target(); push_value(); cc_expect(',',3);
    integer_value();
    if(insert) {
        cc_emit(0x44); cc_emit(0x4d); cc_emit(0xd1); cc_emit(0xe1);
        cc_emit(0x3e); cc_emit(cc.types[type].capacity);
    } else {
        push_value(); cc_expect(',',3); integer_value();
        cc_emit(0xeb); cc_emit(0xc1); cc_emit(0xe1);
    }
    cc_expect(')',5); cc_call(insert?RT_INSERT:RT_DELETE); cc_release(before);
}
static int16_t predefined_function(void) {
    static const char *const functions[]={"ord","chr","succ","pred","abs","sqr","odd","hi","lo","swap","upcase","sizeof","addr","length","copy","pos","concat","ptr","memavail","maxavail","stackptr","trunc","round","int","frac","sqrt","ln","exp","sin","cos","arctan"};
    uint8_t i;
    for(i=0;i<sizeof(functions)/sizeof(functions[0]);++i) if(cc_is(functions[i])) return i;
    return -1;
}
static void negate(void) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); cc_emit(0x23); }
static uint8_t intrinsic(uint8_t op) {
    uint8_t kind=K_INTEGER;
    uint16_t before=cc.temporaries;
    uint8_t saved;
    int16_t id;
    cc_next();
    if(op==F_STACKPTR) { cc_op16(0x21,0); cc_emit(0x39); return K_INTEGER; }
    if(op==F_MEMAVAIL||op==F_MAXAVAIL) {
        cc_emit(0x3e); cc_emit(op==F_MAXAVAIL); cc_call(RT_AVAIL); return K_INTEGER;
    }
    cc_expect('(',4);
    if(op>=F_COPY&&op<=F_CONCAT) return string_function(op);
    if(op==F_SIZEOF||op==F_ADDR) {
        id=cc.token==T_ID?cc_lookup():-1;
        if(id<0&&cc_is("port")) { cc_fail(41); return K_INTEGER; }
        if(id<0&&cc_is("mem")) {
            saved=cc.suppress; if(op==F_SIZEOF) ++cc.suppress;
            raw_index(); cc.suppress=saved; cc.temporaries=before;
            if(op==F_SIZEOF) cc_op16(0x21,1);
        } else if(op==F_ADDR) {
            if(id>=0&&(cc.symbols[id].role==S_PROCEDURE||cc.symbols[id].role==S_FUNCTION||cc.symbols[id].role==S_RESULT)) {
                cc_routine_address((uint16_t)id); cc_next();
            } else if(!cc_variable(id)) cc_fail(44); else cc_selection((uint16_t)id);
        } else if(id>=0&&cc_variable(id)&&cc.symbols[id].type!=255) {
            saved=cc.suppress; ++cc.suppress; kind=cc_selection((uint16_t)id);
            cc.suppress=saved; cc.temporaries=before; cc_op16(0x21,cc.types[kind].width);
        } else if(id>=0&&cc.symbols[id].role==S_TYPE) { cc_op16(0x21,cc.types[cc.symbols[id].type].width); cc_next(); }
        else if(id<0&&(id=cc_predefined_type())>=0) { cc_op16(0x21,cc.types[id].width); cc_next(); }
        else cc_fail(41);
        cc_expect(')',5); return K_INTEGER;
    }
    kind=cc_text_value(expression()); cc_expect(')',5);
    if(cc.error->code) return K_INTEGER;
    if(op==F_LENGTH) {
        if(cc_family(kind)==K_STRING) { cc_emit(0x6e); cc_emit(0x26); cc_emit(0); }
        else if(cc_family(kind)==K_CHAR) cc_op16(0x21,1);
        else cc_fail(cc_aggregate(kind)?62:44);
        cc_release(before); return K_INTEGER;
    }
    if(op==F_PTR) { if(cc_family(kind)!=K_INTEGER) cc_fail(23); return K_ANY_POINTER; }
    if(op==F_ORD||op==F_SUCC||op==F_PRED) {
        if(cc_family(kind)==K_STRING) { cc_call(RT_STRINGCHAR); cc_release(before); kind=K_CHAR; }
        if(!cc_ordinal(kind)&&!(op==F_ORD&&cc_family(kind)==K_POINTER)) {
            cc_fail(31); return K_INTEGER;
        }
        if(op==F_ORD) return K_INTEGER;
        cc_emit(op==F_SUCC?0x23:0x2b); cc_check_range(kind);
        if(cc.types[kind].width==1) { cc_emit(0x26); cc_emit(0); }
        return kind;
    }
    if(op==F_UPCASE) {
        if(cc_family(kind)==K_STRING) cc_call(RT_STRINGCHAR); else if(cc_family(kind)!=K_CHAR) cc_fail(44);
        cc_emit(0x7d); cc_emit(0xfe); cc_emit('a'); cc_emit(0x38); cc_emit(7);
        cc_emit(0xfe); cc_emit('z'+1); cc_emit(0x30); cc_emit(3); cc_emit(0xd6); cc_emit(32); cc_emit(0x6f);
        cc_release(before);
        return K_CHAR;
    }
    if(op>=F_TRUNC||((op==F_ABS||op==F_SQR)&&cc_family(kind)==K_REAL)) {
        cc_real_unary(op==F_TRUNC?ZR_TRUNC:op==F_ROUND?ZR_ROUND:op==F_INT?ZR_INT:op==F_FRAC?ZR_FRAC:op==F_SQRT?ZR_SQRT:op==F_LN?ZR_LN:op==F_EXP?ZR_EXP:op==F_SIN?ZR_SIN:op==F_COS?ZR_COS:op==F_ARCTAN?ZR_ARCTAN:op==F_ABS?ZR_ABS:ZR_SQR,kind,before);
        return op==F_TRUNC||op==F_ROUND?K_INTEGER:K_REAL;
    }
    if(cc_family(kind)!=K_INTEGER) cc_fail(23);
    if(op==F_CHR) { cc_check_range(K_CHAR); cc_emit(0x26); cc_emit(0); return K_CHAR; }
    if(op==F_ABS) { cc_emit(0xcb); cc_emit(0x7c); cc_emit(0x28); cc_emit(7); negate(); }
    else if(op==F_SQR) { cc_emit(0x54); cc_emit(0x5d); cc_call(RT_MUL); }
    else if(op==F_ODD) { cc_emit(0x7d); cc_emit(0xe6); cc_emit(1); cc_emit(0x6f); cc_emit(0x26); cc_emit(0); return K_BOOLEAN; }
    else if(op==F_HI) { cc_emit(0x6c); cc_emit(0x26); cc_emit(0); }
    else if(op==F_LO) { cc_emit(0x26); cc_emit(0); }
    else if(op==F_SWAP) { cc_emit(0x7c); cc_emit(0x65); cc_emit(0x6f); }
    return K_INTEGER;
}
static uint8_t factor(void) {
    int16_t id,fn=-1;
    uint16_t before,value_before=cc.temporaries;
    uint8_t negative,kind=K_INTEGER;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    if(cc.token=='^') cc_control_literal();
    if(cc_is("nil")) { cc_op16(0x21,0); kind=K_NIL; cc_next(); }
    else if(cc.token=='+'||cc.token=='-') {
        negative=cc.token=='-'; cc_next();
        if(negative&&cc.token==T_NUMBER&&!cc.hexadecimal&&!cc.number_overflow&&cc.magnitude==32768u) {
            cc_op16(0x21,0x8000); cc_next();
        } else {
            kind=factor();
            if(!cc_numeric(kind)) cc_fail(47);
            if(cc_family(kind)==K_REAL) { if(negative) cc_real_unary(ZR_NEGATE,kind,value_before); }
            else { kind=K_INTEGER; if(negative) negate(); }
        }
    } else if(cc_is("not")) {
        cc_next(); kind=factor();
        if(cc_family(kind)==K_BOOLEAN) { cc_emit(0x7d); cc_emit(0xee); cc_emit(1); cc_emit(0x6f); kind=K_BOOLEAN; }
        else if(cc_family(kind)==K_INTEGER) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); kind=K_INTEGER; }
        else cc_fail(47);
    } else if(cc.token==T_NUMBER) {
        if(cc.number_overflow||(!cc.hexadecimal&&cc.magnitude>32767u)) cc_fail(56);
        cc_op16(0x21,cc.magnitude); cc_next();
    } else if(cc.token==T_REAL) {
        cc_real_emit(cc.token_start); kind=K_REAL; cc_next();
    } else if(cc.token==T_STRING) {
        if(cc.literal_length==1) { cc_op16(0x21,cc.literal[0]); kind=K_CHAR; }
        else { cc_emit_literal(); kind=K_STRING; }
        cc_next();
    } else if(cc.token=='[') kind=set_constructor();
    else if(cc.token=='(') { cc_next(); kind=expression(); cc_expect(')',5); }
    else if(cc.token==T_ID) {
        id=cc_lookup();
        if((id>=0&&cc.symbols[id].role==S_TYPE)||(id<0&&(fn=cc_predefined_type())>=0)) {
            kind=id>=0?cc.symbols[id].type:(uint8_t)fn;
            cc_next(); cc_expect('(',4);
            before=cc.temporaries;
            negative=cc_text_value(expression()); cc_expect(')',5);
            if(cc_family(kind)==K_POINTER||cc_family(kind)==K_REAL) cc_fail(30);
            else if(!cc_ordinal(kind)) cc_fail(44);
            else if(cc_family(kind)==K_CHAR&&cc_family(negative)==K_STRING) cc_call(RT_STRINGCHAR);
            else if(!cc_ordinal(negative)) cc_fail(cc_family(negative)==K_REAL?31:44);
            cc_check_range(kind);
            if(cc.types[kind].width==1) { cc_emit(0x26); cc_emit(0); }
            cc_release(before);
        } else if(id<0&&raw_kind()) {
            negative=raw_kind(); raw_index(); kind=K_BYTE;
            if(negative==1) cc_load_indirect(K_BYTE);
            else { cc_emit(0x4d); cc_emit(0x44); cc_emit(0xed); cc_emit(0x68); cc_emit(0x26); cc_emit(0); }
        } else if(id<0&&(fn=predefined_function())>=0) kind=intrinsic((uint8_t)fn);
        else if(id<0&&(cc_is("true")||cc_is("false"))) {
            cc_op16(0x21,cc_is("true")?1:0); kind=K_BOOLEAN; cc_next();
        } else if(id<0&&cc_is("maxint")) { cc_op16(0x21,32767); cc_next();
        } else if(id<0&&cc_is("pi")) { cc_real_emit(REAL_PI); kind=K_REAL; cc_next();
        } else if(id<0) cc_fail(41);
        else if(cc.symbols[id].role==S_CONSTANT) {
            kind=cc.symbols[id].type;
            if(cc_family(kind)==K_STRING) { cc_constant_literal(cc.symbols[id].address); cc_emit_literal(); }
            else if(cc_family(kind)==K_REAL) cc_real_emit(cc.symbols[id].address);
            else cc_op16(0x21,cc.symbols[id].address);
            cc_next();
        } else if(cc.symbols[id].role==S_FUNCTION||cc.symbols[id].role==S_RESULT) { kind=cc_invoke((uint16_t)id); if(cc.token=='^') cc_fail(66); }
        else if(cc_variable(id)) {
            kind=cc.symbols[id].type; cc_next();
            if(((cc_family(kind)==K_STRING||cc_family(kind)==K_ARRAY)&&cc.token=='[')||
               (cc_family(kind)==K_RECORD&&cc.token=='.')||
               (cc_family(kind)==K_POINTER&&cc.token=='^')) {
                cc_symbol_address((uint16_t)id); kind=cc_index(kind); cc_load_indirect(kind);
            } else cc_load_symbol((uint16_t)id);
        } else cc_fail(41);
    } else cc_fail(54);
    if(cc_family(kind)==K_REAL&&cc.temporaries==value_before) cc_real_hold();
    --cc.depth;
    return kind;
}
static void bitwise(uint8_t op) {
    /* DE left, HL right. Boolean operands are already normalized to 0/1. */
    cc_emit(0x7d); cc_emit(op==0?0xa3:op==1?0xb3:0xab); cc_emit(0x6f);
    cc_emit(0x7c); cc_emit(op==0?0xa2:op==1?0xb2:0xaa); cc_emit(0x67);
}
static uint8_t term(void) {
    uint16_t before=cc.temporaries,right_before,right_bytes;
    uint8_t op,right,base,kind=factor();
    while(!cc.error->code&&(cc.token=='*'||cc.token=='/'||cc_is("div")||cc_is("mod")||cc_is("and")||cc_is("shl")||cc_is("shr"))) {
        op=cc.token=='*'?0:cc_is("div")?1:cc_is("mod")?2:cc_is("and")?3:cc_is("shl")?4:cc_is("shr")?5:6;
        base=cc_family(kind);
        if(base==K_SET) {
            if(op!=0) cc_fail(47);
            cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
            right=factor(); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
            kind=set_binary(kind,right,1,before,right_bytes); continue;
        }
        if(op==0||op==6) { if(!cc_numeric(kind)) cc_fail(47); }
        else if(base!=K_INTEGER&&(op!=3||base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=factor(); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
        if(op==6||(op==0&&(base==K_REAL||cc_family(right)==K_REAL))) {
            cc_real_binary(op==6?ZR_DIV:ZR_MUL,kind,right,before,right_bytes); kind=K_REAL; continue;
        }
        if(cc_family(right)!=base) cc_fail(47);
        if(op==3) bitwise(0);
        else cc_call(op==0?RT_MUL:op==1?RT_DIV:op==2?RT_MOD:op==4?RT_SHL:RT_SHR);
        kind=base==K_INTEGER?K_INTEGER:K_BOOLEAN;
    }
    return kind;
}
static uint8_t text(uint8_t kind) { return cc_family(kind)==K_STRING||cc_family(kind)==K_CHAR||cc_char_array(kind); }
static void hold_left(uint8_t kind,uint16_t before) {
    if(cc_char_array(kind)) cc_text_value(kind);
    else if(cc_family(kind)==K_STRING&&cc.temporaries==before) {
        cc_call(RT_HOLDSTRING); cc.temporaries+=256;
    }
}
/* Right-hand string results may lie below the saved left operand. Reading
 * that word must not pop through live result bytes before the operation. */
static uint8_t simple_expression(void) {
    uint16_t op,before=cc.temporaries,right_before,right_bytes,owned;
    uint8_t right,base,kind=term();
    while(!cc.error->code&&(cc.token=='+'||cc.token=='-'||cc_is("or")||cc_is("xor"))) {
        op=cc.token=='+'||cc.token=='-'?cc.token:cc_is("or")?'|':'^';
        base=cc_family(kind);
        if(base==K_SET) { if(op!='+'&&op!='-') cc_fail(47); }
        else if(op=='+'&&text(kind)) hold_left(kind,before);
        else if((op=='+'||op=='-')?!cc_numeric(kind):(base!=K_INTEGER&&base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=term(); if(op=='+'&&text(kind)) cc_text_value(right); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
        if(base==K_SET) { kind=set_binary(kind,right,op=='+'?0:2,before,right_bytes); continue; }
        if((op=='+'||op=='-')&&(base==K_REAL||cc_family(right)==K_REAL)) {
            cc_real_binary(op=='+'?ZR_ADD:ZR_SUB,kind,right,before,right_bytes); kind=K_REAL; continue;
        }
        if(op=='+'&&text(kind)) {
            if(!text(right)) cc_fail(47);
            owned=cc.temporaries-before+(right_bytes?2:0);
            cc_op16(0x01,owned); cc_emit(0x3e);
            cc_emit((cc_family(kind)==K_CHAR?1:0)|(cc_family(right)==K_CHAR?2:0)); cc_call(RT_CONCAT);
            cc.temporaries=before+256; kind=K_STRING; continue;
        }
        if(cc_family(right)!=base) cc_fail(47);
        if(op=='+') cc_emit(0x19);
        else if(op=='-') { cc_emit(0xeb); cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52); }
        else bitwise(op=='|'?1:2);
        kind=base==K_INTEGER?K_INTEGER:K_BOOLEAN;
    }
    return kind;
}
static uint8_t expression(void) {
    uint16_t before=cc.temporaries,right_before,right_bytes;
    uint8_t kind=simple_expression(),right,jump;
    uint16_t op=cc_is("in")?'i':cc.token;
    if(op=='i') {
        if(!cc_ordinal(kind)) cc_fail(44);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=simple_expression(); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
        if(cc_family(right)!=K_SET||(cc_set_base(right)!=255&&cc_set_base(right)!=cc_family(kind))) cc_fail(44);
        cc_call(RT_SETMEM); if(right_bytes) cc_drop_stack(2); cc_release(before); return K_BOOLEAN;
    }
    if(op!='='&&op!=T_NE&&op!='<'&&op!='>'&&op!=T_LE&&op!=T_GE) return kind;
    hold_left(kind,before);
    cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
    right=simple_expression(); cc_text_value(right); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
    if(cc_family(kind)==K_SET||cc_family(right)==K_SET) {
        if(!cc_compatible(kind,right)) cc_fail(44);
        if(op=='<'||op=='>') cc_fail(47);
        cc_emit(0x3e); cc_emit(op=='='?0:op==T_NE?1:op==T_LE?2:3); cc_call(RT_SETCMP);
        if(right_bytes) cc_drop_stack(2); cc_release(before); return K_BOOLEAN;
    }
    if(cc_family(kind)==K_REAL||cc_family(right)==K_REAL) {
        cc_real_binary(ZR_COMPARE,kind,right,before,right_bytes);
    } else {
    if((cc_aggregate(kind)&&!cc_char_array(kind))||(cc_aggregate(right)&&!cc_char_array(right))) cc_fail(62);
    else if(!cc_compatible(kind,right)) cc_fail(47);
    if((cc_family(kind)==K_POINTER||cc_family(right)==K_POINTER)&&op!='='&&op!=T_NE) cc_fail(47);
    if(text(kind)||text(right)) {
        cc_emit(0x3e); cc_emit((cc_family(kind)==K_CHAR?1:0)|(cc_family(right)==K_CHAR?2:0)); cc_call(RT_TEXTCMP);
    } else cc_call(RT_COMPARE);
    if(right_bytes) cc_drop_stack(2);
    cc_release(before);
    }
    /* Three-way comparison returns exactly -1, 0, +1. Normalize every
     * relational expression to a one-byte Boolean, held zero-extended in HL. */
    if(op=='='||op==T_NE) { cc_emit(0x7c); cc_emit(0xb5); jump=op=='='?0x28:0x20; }
    else if(op=='<'||op==T_GE) { cc_emit(0xcb); cc_emit(0x7c); jump=op=='<'?0x20:0x28; }
    else { cc_emit(0x7d); cc_emit(0xfe); cc_emit(1); jump=op=='>'?0x28:0x20; }
    cc_op16(0x11,1); cc_emit(jump); cc_emit(3); cc_op16(0x11,0); cc_emit(0xeb);
    return K_BOOLEAN;
}
uint8_t cc_expression(void) BACKEND { return expression(); }

/* Heap pointer destinations are selected exactly once before size evaluation.
 * The runtime commits a returned pointer only after allocation succeeds. */
static void block_statement(uint8_t fill) {
    uint16_t before=cc.temporaries;
    uint8_t kind;
    cc_next(); cc_expect('(',4); cc_reference(41); push_value(); cc_expect(',',3);
    if(!fill) { cc_reference(41); push_value(); cc_expect(',',3); }
    integer_value(); cc_release(before);
    if(fill) {
        push_value(); cc_expect(',',3); kind=cc_text_value(expression());
        if(cc_family(kind)==K_STRING) { cc_call(RT_STRINGCHAR); kind=K_CHAR; }
        if(!cc_ordinal(kind)) cc_fail(31);
        cc_release(before); cc_emit(0x7d); cc_emit(0xc1); cc_emit(0xe1);
    } else { cc_emit(0x44); cc_emit(0x4d); cc_emit(0xd1); cc_emit(0xe1); }
    cc_expect(')',5); cc_call(fill?RT_FILL:RT_MOVE);
}
static void raw_store(uint8_t port) {
    uint16_t before=cc.temporaries;
    uint8_t kind;
    raw_index(); push_value(); cc_expect(T_ASSIGN,7);
    kind=expression();
    if(port) { if(cc_family(kind)!=K_INTEGER) cc_fail(23); }
    else cc_convert(K_BYTE,kind);
    cc_release(before); cc_emit(0xd1);
    if(port) { cc_emit(0x4b); cc_emit(0x42); cc_emit(0x7d); cc_emit(0xed); cc_emit(0x79); }
    else { cc_emit(0x7d); cc_emit(0x12); }
}
uint8_t cc_runtime_statement(void) BACKEND {
    uint8_t op,type,kind;
    int16_t id;
    uint16_t before=cc.temporaries,width=0;
    if(cc.token!=T_ID||cc_lookup()>=0) return 0;
    if(raw_kind()) { raw_store(raw_kind()==2); return 1; }
    if(cc_is("move")||cc_is("fillchar")) { block_statement(cc_is("fillchar")); return 1; }
    if(cc_is("stackptr")) {
        cc_next(); cc_expect(T_ASSIGN,7); integer_value(); cc_release(before);
        cc_call(RT_STACKSET); return 1;
    }
    op=cc_is("new")?0:cc_is("getmem")?1:cc_is("mark")?2:
       cc_is("dispose")?3:cc_is("freemem")?4:cc_is("release")?5:255;
    if(op==255) return 0;
    cc_next(); cc_expect('(',4);
    if(op<=3) {
        id=cc.token==T_ID?cc_lookup():-1;
        if(!cc_variable(id)||cc.symbols[id].control||cc.symbols[id].role==S_RESULT) { cc_fail(44); return 1; }
        type=cc_selection((uint16_t)id);
        if(cc_family(type)!=K_POINTER) { cc_fail(44); return 1; }
        width=cc.types[cc.types[type].element].width;
        if(op==3) { cc_load_indirect(type); cc_op16(0x11,width); }
        else {
            push_value();
            if(op==1) { cc_expect(',',3); integer_value(); cc_release(before); }
            else cc_op16(0x21,width);
        }
    } else {
        kind=expression();
        if(cc_family(kind)!=K_POINTER) cc_fail(44);
        cc_release(before);
        if(op==4) {
            push_value(); cc_expect(',',3); integer_value(); cc_release(before);
            cc_emit(0xeb); cc_emit(0xe1);
        }
    }
    cc_expect(')',5);
    if(op==2) { cc_emit(0x3e); cc_emit(2); cc_call(RT_AVAIL); }
    else cc_call(op<=1?RT_ALLOC:op<=4?RT_FREE:RT_RELEASE);
    if(op<=2) { cc_emit(0xd1); cc_store_indirect(K_INTEGER); }
    return 1;
}
