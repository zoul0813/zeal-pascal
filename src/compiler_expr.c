#include "compiler.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_unsigned_internal.h"
#endif

static TypeId expression(void);
static TypeId set_constructor(void) {
    TypeId base=TYPE_NONE;TypeId kind;TypeId upper;
    uint16_t before;
    cc_temporary(32); cc_op16(0x21,0); cc_emit(0x39); cc_call(RT_SETZERO); cc_next();
    while(cc.token!=']'&&!cc.error->code) {
        before=cc.temporaries; kind=expression();
        if(!cc_ordinal(kind)||cc_family(kind)==K_WORD) cc_fail(44);
        if(base==TYPE_NONE) base=cc_family(kind); else if(base!=cc_family(kind)) cc_fail(44);
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
    return base==TYPE_NONE?K_EMPTY_SET:K_SET_EXPR+base;
}
static TypeId set_binary(TypeId left,TypeId right,uint8_t op,uint16_t before,uint16_t right_bytes) {
    if(cc_family(right)!=K_SET||!cc_compatible(left,right)) cc_fail(44);
    cc_op16(0x01,cc.temporaries-before+(right_bytes?2:0)); cc_emit(0x3e); cc_emit(op); cc_call(RT_SETBIN);
    cc.temporaries=before+32;
    return cc_set_base(left)==TYPE_NONE?right:left;
}
/* HL is the checked, lower-bound-adjusted ordinal index. Both forms compute
 * the same sixteen-bit byte offset; DE is scratch until the caller pops its
 * saved base. X+ avoids the general multiplication loop. X- chooses its
 * thirteen-byte guarded call only when that saves code over shifts/adds. */
static void scale_index(uint16_t stride) {
    uint16_t bit;
    uint8_t cost=0;uint8_t mixed;
    if(stride==1)return;
    if(!stride){cc_op16(0x21,0);return;}
    mixed=(stride&(stride-1))!=0;
    bit=0x8000u;
    while(!(stride&bit))bit>>=1;
    if(mixed)cost=2;
    for(bit>>=1;bit;bit>>=1)cost+=1+((stride&bit)!=0);
    if(!cc.index_speed&&cost>13) {
        cc_op16(0x11,stride);cc_call(RT_MUL);return;
    }
    if(mixed){cc_emit(0x54);cc_emit(0x5d);} /* DE = original index */
    bit=0x8000u;
    while(!(stride&bit))bit>>=1;
    for(bit>>=1;bit;bit>>=1) {
        cc_emit(0x29); /* ADD HL,HL */
        if(stride&bit)cc_emit(0x19); /* ADD HL,DE */
    }
}
TypeId cc_index(TypeId type) BACKEND {
    uint16_t before,stride;
    TypeId kind;TypeId index;TypeId array;
    int16_t field;
    while(!cc.error->code) {
        if(cc_family(type)==K_POINTER&&cc.token=='^') {
            cc_require_full_memory();
            cc_load_indirect(type); type=CCT(type,element); cc_next(); continue;
        }
        if(cc_family(type)==K_RECORD&&cc.token=='.') {
            cc_next(); field=cc.token==T_ID?cc_field_lookup(type):-1;
            if(field<0) { cc_fail(41); return type; }
            if(cc.field_offset) { cc_op16(0x11,cc.field_offset); cc_emit(0x19); }
            type=CCS(field,type); cc_next(); continue;
        }
        if((cc_family(type)!=K_STRING&&cc_family(type)!=K_ARRAY)||cc.token!='[') break;
        cc_next();
        for(;;) {
            array=cc_family(type)==K_ARRAY;
            index=array?CCT(type,capacity):K_INTEGER;
            cc_guard(); cc_emit(0xe5); before=cc.temporaries;
            kind=expression();
            if(cc_family(kind)!=cc_family(index)
#ifdef ZP_UNITS_EXPERIMENT
                &&!(cc_integral(kind)&&cc_integral(index)&&cc_family(kind)!=K_LONGWORD)
#endif
            )cc_fail(array?44:23);
            cc_release(before);
            if(cc.range_check) {
#ifdef ZP_UNITS_EXPERIMENT
                if(cc_unsigned(kind)||cc_unsigned(index))cc_unsigned_index(array?index:TYPE_NONE,kind,array?0:CCT(type,capacity));
                else {
#endif
                cc_op16(0x11,array?(uint16_t)CCT(index,low):0);
                cc_op16(0x01,array?(uint16_t)CCT(index,high):CCT(type,capacity));
                cc_call(RT_INDEX);
#ifdef ZP_UNITS_EXPERIMENT
                }
#endif
            }
            if(array) {
                if(CCT(index,low)) {
                    cc_op16(0x11,(uint16_t)CCT(index,low)); cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52);
                }
                type=CCT(type,element); stride=CCT(type,width);
                scale_index(stride);
            } else type=K_CHAR;
            cc_emit(0xd1); cc_emit(0x19);
            if(cc.token!=','||cc_family(type)!=K_ARRAY) break;
            cc_next();
        }
        cc_expect(']',9);
    }
    return type;
}
TypeId cc_selection(uint16_t id) BACKEND {
    TypeId type=CCS(id,type);
    cc_symbol_address(id); cc_next();
    return CCS(id,role)==S_RESULT?type:cc_index(type);
}
/* Mem is addressable byte storage; Port is an I/O operation, never a Var. */
static TypeId raw_kind(void) {
    return cc_is("mem")?1:cc_is("port")?2:0;
}
static void raw_index(void) {
    uint16_t before=cc.temporaries;
    TypeId kind;
    if(!cc.suppress)cc.console_dynamic=1;
    cc_next(); cc_expect('[',8);
    kind=expression();
    if(cc_family(kind)!=K_INTEGER
#ifdef ZP_UNITS_EXPERIMENT
       &&cc_family(kind)!=K_WORD
#endif
    ) cc_fail(23);
    cc_release(before); cc_expect(']',9);
}
TypeId cc_reference(uint8_t code) BACKEND {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    if(id<0&&cc_is("mem")) { raw_index(); return K_BYTE; }
    if((id<0&&cc_is("port"))||(id>=0&&CCS(id,role)==S_RESULT)) { cc_fail(41); return K_INTEGER; }
    if(!cc_variable(id)) { cc_fail(code); return K_INTEGER; }
    if(CCS(id,control)) { cc_fail(44); return K_INTEGER; }
    return cc_selection((uint16_t)id);
}
/* Array values are variable selections or an exact-length string constant
 * for a Char array. They do not acquire general scalar-expression semantics. */
TypeId cc_aggregate_actual(TypeId type) BACKEND {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    TypeId actual;
    uint16_t value;
    if(cc_char_array(type)&&(cc.token==T_STRING||(id>=0&&CCS(id,role)==S_CONSTANT))) {
        cc_constant(&actual,&value);
        if(actual!=K_STRING) cc_fail(60);
        else {
            cc_constant_literal(value);
            if(cc.literal_length!=CCT(type,width)) cc_fail(60);
            cc_emit_literal(); cc_emit(0x23); /* raw characters after the length */
        }
    } else if(cc.token==T_STRING) cc_fail(44);
    else if(cc_variable(id)) {
        actual=cc_selection((uint16_t)id);
        if(actual!=type) cc_fail(44);
    } else cc_fail(41);
    return type;
}
static void integer_value(void) { if(cc_family(expression())!=K_INTEGER) cc_fail(23); }
static void push_value(void) { cc_guard(); cc_emit(0xe5); }
static int16_t predefined_function(void) {
    static const char *const functions[]={"ord","chr","succ","pred","abs","sqr","odd","hi","lo","swap","upcase","sizeof","addr","length","copy","pos","concat","ptr","memavail","maxavail","stackptr","trunc","round","int","frac","sqrt","ln","exp","sin","cos","arctan","random","ioresult","paramcount","paramstr"
#ifndef ZP_UNITS_EXPERIMENT
        ,"wherex","wherey"
#endif
    };
    uint8_t i;
    for(i=0;i<sizeof(functions)/sizeof(functions[0]);++i) if(cc_is(functions[i])) return i;
    return -1;
}
static void negate(void) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); cc_emit(0x23); }
static TypeId intrinsic(uint8_t op) {
    TypeId kind=K_INTEGER;
    uint16_t before=cc.temporaries;
    uint8_t saved;
    int16_t id;
    if(op==F_PTR||op==F_ADDR||op==F_STACKPTR)cc_require_full_memory();
    cc_next();
#ifndef ZP_UNITS_EXPERIMENT
    if(op==F_WHEREX||op==F_WHEREY) { cc_crt_query(op==F_WHEREY); return K_BYTE; }
#endif
    if(op==F_IORESULT) { cc_call(RT_IORESULT); return K_INTEGER; }
    if(op==F_PARAMCOUNT) { cc_params(0); return K_INTEGER; }
    if(op==F_RANDOM&&cc.token!='(') { cc_random(0); return K_REAL; }
    if(op==F_STACKPTR) { cc_op16(0x21,0); cc_emit(0x39); return K_INTEGER; }
    if(op==F_MEMAVAIL||op==F_MAXAVAIL) {
        cc_emit(0x3e); cc_emit(op==F_MAXAVAIL); cc_call(RT_AVAIL); return K_INTEGER;
    }
    cc_expect('(',4);
    if(op==F_RANDOM&&(cc.token==')'||cc.token==','||cc.token==T_EOF)) {
        cc_fail(41); return K_INTEGER;
    }
    if(op>=F_COPY&&op<=F_CONCAT) return cc_string_function((uint8_t)(op-F_COPY));
    if(op==F_SIZEOF||op==F_ADDR) {
        id=cc.token==T_ID?cc_lookup():-1;
        if(id<0&&cc_is("port")) { cc_fail(41); return K_INTEGER; }
        if(id<0&&cc_is("mem")) {
            saved=cc.suppress; if(op==F_SIZEOF) ++cc.suppress;
            raw_index(); cc.suppress=saved; cc.temporaries=before;
            if(op==F_SIZEOF) cc_op16(0x21,1);
        } else if(op==F_ADDR) {
            if(id>=0&&(CCS(id,role)==S_PROCEDURE||CCS(id,role)==S_FUNCTION||CCS(id,role)==S_RESULT)) {
                cc_routine_address((uint16_t)id); cc_next();
            } else if(!cc_variable(id)) cc_fail(44); else cc_selection((uint16_t)id);
        } else if(id>=0&&cc_variable(id)&&CCS(id,type)!=TYPE_NONE) {
            saved=cc.suppress; ++cc.suppress; kind=cc_selection((uint16_t)id);
            cc.suppress=saved; cc.temporaries=before; cc_op16(0x21,CCT(kind,width));
        } else if(id>=0&&CCS(id,role)==S_TYPE) { cc_op16(0x21,CCT(CCS(id,type),width)); cc_next(); }
        else if(id<0&&(id=cc_predefined_type())>=0) { cc_op16(0x21,CCT(id,width)); cc_next(); }
        else cc_fail(41);
        cc_expect(')',5); return K_INTEGER;
    }
    kind=cc_text_value(expression()); cc_expect(')',5);
    if(cc.error->code) return K_INTEGER;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc_unsigned(kind)&&op<=F_SWAP)return cc_unsigned_intrinsic(op,kind,before);
#endif
    if(op==F_PARAMSTR) {
        if(cc_family(kind)!=K_INTEGER) cc_fail(23);
        cc_release(before); cc_params(1); return K_STRING;
    }
    if(op==F_RANDOM) {
        if(cc_family(kind)!=K_INTEGER) cc_fail(23);
        cc_release(before); cc_random(1); return K_INTEGER;
    }
    if(op==F_LENGTH) {
        if(cc_family(kind)==K_STRING) { cc_emit(0x6e); cc_emit(0x26); cc_emit(0); }
        else if(cc_family(kind)==K_CHAR) cc_op16(0x21,1);
        else cc_fail(cc_aggregate(kind)?62:44);
        cc_release(before); return K_INTEGER;
    }
    if(op==F_PTR) { if(!cc.suppress)cc.console_dynamic=1; if(cc_family(kind)!=K_INTEGER) cc_fail(23); return K_ANY_POINTER; }
    if(op==F_ORD||op==F_SUCC||op==F_PRED) {
        if(cc_family(kind)==K_STRING) { cc_call(RT_STRINGCHAR); cc_release(before); kind=K_CHAR; }
        if(!cc_ordinal(kind)&&!(op==F_ORD&&cc_family(kind)==K_POINTER)) {
            cc_fail(31); return K_INTEGER;
        }
        if(op==F_ORD) return K_INTEGER;
        cc_emit(op==F_SUCC?0x23:0x2b); cc_check_range(kind);
        if(CCT(kind,width)==1) { cc_emit(0x26); cc_emit(0); }
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
static TypeId factor(void) {
    int16_t id,fn=-1;
    uint16_t before,value_before=cc.temporaries;
    uint8_t negative;TypeId kind=K_INTEGER,cast_source;
    if(++cc.depth==0||!cc_stack_check()) { cc_fail(99); --cc.depth; return 0; }
    if(cc.token=='^') cc_control_literal();
    if(cc_is("nil")) { cc_op16(0x21,0); kind=K_NIL; cc_next(); }
    else if(cc.token=='+'||cc.token=='-') {
        negative=cc.token=='-'; cc_next();
#ifdef ZP_UNITS_EXPERIMENT
        cc_unsigned_negative_literal(negative);
#endif
        if(negative&&cc.token==T_NUMBER&&!cc.number_overflow
#ifdef ZP_UNITS_EXPERIMENT
           &&cc.unsigned_magnitude<=32768UL
#endif
        ) {
            cc_op16(0x21,(uint16_t)(0u-cc.magnitude)); cc_next();
        } else {
            kind=factor();
            if(!cc_numeric(kind)) cc_fail(47);
            if(cc_family(kind)==K_REAL) { if(negative) cc_real_unary(ZR_NEGATE,kind,value_before); }
#ifdef ZP_UNITS_EXPERIMENT
            else if(cc_unsigned(kind)) {
                if(negative&&cc_family(kind)==K_LONGWORD) {
                    cc_op16(0x11,0);kind=cc_unsigned_binary(1,K_INTEGER,kind,value_before,0);
                } else if(negative)negate();
            }
#endif
            else { kind=K_INTEGER; if(negative) negate(); }
        }
    } else if(cc_is("not")) {
        cc_next(); kind=factor();
        if(cc_family(kind)==K_BOOLEAN) { cc_emit(0x7d); cc_emit(0xee); cc_emit(1); cc_emit(0x6f); kind=K_BOOLEAN; }
#ifdef ZP_UNITS_EXPERIMENT
        else if(cc_family(kind)==K_LONGWORD)cc_unsigned_not(kind,value_before);
#endif
        else if(cc_family(kind)==K_INTEGER||cc_family(kind)==K_WORD) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); if(cc_family(kind)!=K_WORD)kind=K_INTEGER; }
        else cc_fail(47);
    } else if(cc.token==T_NUMBER) {
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.number_overflow)cc_fail(56);
        if(cc.unsigned_magnitude>65535UL){kind=cc_unsigned_type(1);cc_unsigned_emit(cc.unsigned_magnitude);}
        else {if(cc.magnitude>32767u&&!cc.hexadecimal)kind=cc_unsigned_type(0);cc_op16(0x21,cc.magnitude);}
#else
        if(cc.number_overflow||(!cc.hexadecimal&&cc.magnitude>32767u)) cc_fail(56);
        cc_op16(0x21,cc.magnitude);
#endif
        cc_next();
    } else if(cc.token==T_REAL) {
        cc_real_emit(cc_source_reference(cc.token_start)); kind=K_REAL; cc_next();
    } else if(cc.token==T_STRING) {
        if(cc.literal_length==1) { cc_op16(0x21,cc.literal[0]); kind=K_CHAR; }
        else { cc_emit_literal(); kind=K_STRING; }
        cc_next();
    } else if(cc.token=='[') kind=set_constructor();
    else if(cc.token=='(') { cc_next(); kind=expression(); cc_expect(')',5); }
    else if(cc.token==T_ID) {
        id=cc_lookup();
        if((id>=0&&CCS(id,role)==S_TYPE)||(id<0&&(fn=cc_predefined_type())>=0)) {
            kind=id>=0?CCS(id,type):(TypeId)fn;
            cc_next(); cc_expect('(',4);
            before=cc.temporaries;
            cast_source=cc_text_value(expression()); cc_expect(')',5);
#ifdef ZP_UNITS_EXPERIMENT
            if(cc_unsigned_convert(kind,cast_source,1)) { }
            else
#endif
            if(cc_family(kind)==K_POINTER||cc_family(kind)==K_REAL) cc_fail(30);
            else if(!cc_ordinal(kind)) cc_fail(44);
            else if(cc_family(kind)==K_CHAR&&cc_family(cast_source)==K_STRING) cc_call(RT_STRINGCHAR);
            else if(!cc_ordinal(cast_source)) cc_fail(cc_family(cast_source)==K_REAL?31:44);
#ifdef ZP_UNITS_EXPERIMENT
            if(!cc_unsigned(kind)&&!cc_unsigned(cast_source))
#endif
            cc_check_range(kind);
            if(CCT(kind,width)==1) { cc_emit(0x26); cc_emit(0); }
            if(cc_family(kind)!=K_LONGWORD)cc_release(before);
        } else if(id<0&&raw_kind()) {
            negative=raw_kind(); raw_index(); kind=K_BYTE;
            if(negative==1) cc_load_indirect(K_BYTE);
            else { cc_emit(0x4d); cc_emit(0x44); cc_emit(0xed); cc_emit(0x68); cc_emit(0x26); cc_emit(0); }
        } else if(id<0&&cc_is("keypressed")) { cc_next(); cc_keypressed(); kind=K_BOOLEAN; }
        else if(id<0&&(fn=cc_file_function())>=0) kind=(uint8_t)fn;
        else if(id<0&&(fn=predefined_function())>=0) kind=intrinsic((uint8_t)fn);
        else if(id<0&&(cc_is("true")||cc_is("false"))) {
            cc_op16(0x21,cc_is("true")?1:0); kind=K_BOOLEAN; cc_next();
        } else if(id<0&&cc_is("maxint")) { cc_op16(0x21,32767); cc_next();
        } else if(id<0&&cc_is("pi")) { cc_real_emit(REAL_PI); kind=K_REAL; cc_next();
        } else if(id<0&&(fn=cc_predefined_color())>=0) { cc_op16(0x21,(uint16_t)fn); cc_next();
        } else if(id<0) cc_fail(41);
        else if(CCS(id,role)==S_CONSTANT) {
            kind=CCS(id,type);
            if(cc_family(kind)==K_STRING) { cc_constant_literal(CCS(id,address)); cc_emit_literal(); }
            else if(cc_family(kind)==K_REAL) cc_real_emit(CCS(id,address));
#ifdef ZP_UNITS_EXPERIMENT
            else if(cc_family(kind)==K_LONGWORD)cc_unsigned_emit(cc_unsigned_literal(CCS(id,address)));
#endif
            else cc_op16(0x21,CCS(id,address));
            cc_next();
        } else if(CCS(id,role)==S_FUNCTION||CCS(id,role)==S_RESULT) { kind=cc_invoke((uint16_t)id); if(cc.token=='^') cc_fail(66); }
        else if(cc_variable(id)) {
            kind=CCS(id,type); cc_next();
            if(((cc_family(kind)==K_STRING||cc_family(kind)==K_ARRAY)&&cc.token=='[')||
               (cc_family(kind)==K_RECORD&&cc.token=='.')||
               (cc_family(kind)==K_POINTER&&cc.token=='^')) {
                cc_symbol_address((uint16_t)id); kind=cc_index(kind); cc_load_indirect(kind);
            } else cc_load_symbol((uint16_t)id);
        } else cc_fail(41);
    } else cc_fail(54);
    if(cc_family(kind)==K_REAL&&cc.temporaries==value_before) cc_real_hold();
#ifdef ZP_UNITS_EXPERIMENT
    if(cc_family(kind)==K_LONGWORD&&cc.temporaries==value_before)cc_unsigned_hold();
#endif
    --cc.depth;
    return kind;
}
static void bitwise(uint8_t op) {
    /* DE left, HL right. Boolean operands are already normalized to 0/1. */
    cc_emit(0x7d); cc_emit(op==0?0xa3:op==1?0xb3:0xab); cc_emit(0x6f);
    cc_emit(0x7c); cc_emit(op==0?0xa2:op==1?0xb2:0xaa); cc_emit(0x67);
}
static TypeId term(void) {
    uint16_t before=cc.temporaries,right_before,right_bytes;
    uint8_t op;TypeId right;TypeId base;TypeId kind=factor();
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
        else if(base!=K_INTEGER&&base!=K_WORD&&base!=K_LONGWORD&&(op!=3||base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=factor(); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc_unsigned(kind)||cc_unsigned(right)) {
            if(op==6){cc_fail(47);break;}
            kind=cc_unsigned_binary(op==0?2:op==1?3:op==2?4:op==3?5:op==4?8:9,kind,right,before,right_bytes);continue;
        }
#endif
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
static uint8_t text(TypeId kind) { return cc_family(kind)==K_STRING||cc_family(kind)==K_CHAR||cc_char_array(kind); }
static void hold_left(TypeId kind,uint16_t before) {
    if(cc_char_array(kind)) cc_text_value(kind);
    else if(cc_family(kind)==K_STRING&&cc.temporaries==before) {
        cc_string_snapshot(256);
    }
}
/* Right-hand string results may lie below the saved left operand. Reading
 * that word must not pop through live result bytes before the operation. */
static TypeId simple_expression(void) {
    uint16_t op,before=cc.temporaries,right_before,right_bytes,owned;
    TypeId right;TypeId base;TypeId kind=term();
    while(!cc.error->code&&(cc.token=='+'||cc.token=='-'||cc_is("or")||cc_is("xor"))) {
        op=cc.token=='+'||cc.token=='-'?cc.token:cc_is("or")?'|':'^';
        base=cc_family(kind);
        if(base==K_SET) { if(op!='+'&&op!='-') cc_fail(47); }
        else if(op=='+'&&text(kind)) hold_left(kind,before);
        else if((op=='+'||op=='-')?!cc_numeric(kind):(base!=K_INTEGER&&base!=K_WORD&&base!=K_LONGWORD&&base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=term(); if(op=='+'&&text(kind)) cc_text_value(right); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc_unsigned(kind)||cc_unsigned(right)) {
            kind=cc_unsigned_binary(op=='+'?0:op=='-'?1:op=='|'?6:7,kind,right,before,right_bytes);continue;
        }
#endif
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
static TypeId expression(void) {
    uint16_t before=cc.temporaries,right_before,right_bytes;
    TypeId kind=simple_expression();TypeId right;uint8_t jump;
    uint16_t op=cc_is("in")?'i':cc.token;
    if(op=='i') {
        if(!cc_ordinal(kind)) cc_fail(44);
        cc_next(); cc_guard(); cc_emit(0xe5); right_before=cc.temporaries;
        right=simple_expression(); right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
        if(cc_family(right)!=K_SET||(cc_set_base(right)!=TYPE_NONE&&cc_set_base(right)!=cc_family(kind))) cc_fail(44);
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
#ifdef ZP_UNITS_EXPERIMENT
    if(cc_unsigned(kind)||cc_unsigned(right))cc_unsigned_binary(10,kind,right,before,right_bytes);
    else
#endif
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
TypeId cc_expression(void) BACKEND { return expression(); }

/* Heap pointer destinations are selected exactly once before size evaluation.
 * The runtime commits a returned pointer only after allocation succeeds. */
static void block_statement(uint8_t fill) {
    uint16_t before=cc.temporaries;
    TypeId kind;
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
    TypeId kind;
    raw_index(); push_value(); cc_expect(T_ASSIGN,7);
    kind=expression();
    if(port) { if(cc_family(kind)!=K_INTEGER) cc_fail(23); }
    else cc_convert(K_BYTE,kind);
    cc_release(before); cc_emit(0xd1);
    if(port) { cc_emit(0x4b); cc_emit(0x42); cc_emit(0x7d); cc_emit(0xed); cc_emit(0x79); }
    else { cc_emit(0x7d); cc_emit(0x12); }
}
uint8_t cc_runtime_statement(void) BACKEND {
    uint8_t op;TypeId type=K_INTEGER;TypeId kind;
    int16_t id;
    uint16_t before=cc.temporaries,width=0;
    if(cc.token!=T_ID||cc_lookup()>=0) return 0;
    if(raw_kind()) { raw_store(raw_kind()==2); return 1; }
    if(cc_is("move")||cc_is("fillchar")) { block_statement(cc_is("fillchar")); return 1; }
    if(cc_is("stackptr")) {
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode)cc.console_dynamic=1;
#endif
        cc_next(); cc_expect(T_ASSIGN,7); integer_value(); cc_release(before);
        cc_call(RT_STACKSET); return 1;
    }
    op=cc_is("new")?0:cc_is("getmem")?1:cc_is("mark")?2:
       cc_is("dispose")?3:cc_is("freemem")?4:cc_is("release")?5:255;
    if(op==255) return 0;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode)cc.console_dynamic=1;
#endif
    cc_next(); cc_expect('(',4);
    if(op<=3) {
        id=cc.token==T_ID?cc_lookup():-1;
        if(!cc_variable(id)||CCS(id,control)||CCS(id,role)==S_RESULT) { cc_fail(44); return 1; }
        type=cc_selection((uint16_t)id);
        if(cc_family(type)!=K_POINTER) { cc_fail(44); return 1; }
        width=CCT(CCT(type,element),width);
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
    else {
        cc_call(op<=1?RT_ALLOC:op<=4?RT_FREE:RT_RELEASE);
    }
    if(op<=2) { cc_emit(0xd1); cc_store_indirect(K_INTEGER); }
    return 1;
}
