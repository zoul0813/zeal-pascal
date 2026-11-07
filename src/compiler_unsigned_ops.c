#include "compiler_unsigned_internal.h"

TypeId cc_unsigned_binary(uint8_t op,TypeId left,TypeId right,uint16_t before,uint16_t right_bytes) BACKEND {
    uint8_t wide=(cc_family(left)==K_LONGWORD||(op!=8&&op!=9&&cc_family(right)==K_LONGWORD));
    if(!cc_integral(left)||!cc_integral(right)){cc_fail(47);return K_INTEGER;}
    if(!wide&&cc_family(right)!=K_LONGWORD&&op!=3&&op!=4) {
        if(op==0)cc_emit(0x19);
        else if(op==1){cc_emit(0xeb);cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);}
        else if(op==2)cc_call(RT_MUL);
        else if(op==8||op==9)cc_call(op==8?RT_SHL:RT_SHR);
        else if(op==10) {
            cc_reserve(2);cc_emit(0xd5);
            cc_emit(0x7a);cc_emit(0xee);cc_emit(128);cc_emit(0x57);
            cc_emit(0x7c);cc_emit(0xee);cc_emit(128);cc_emit(0x67);
            cc_call(RT_COMPARE);cc_emit(0xd1);
        } else {
            cc_emit(0x7d);cc_emit(op==5?0xa3:op==6?0xb3:0xab);cc_emit(0x6f);
            cc_emit(0x7c);cc_emit(op==5?0xa2:op==6?0xb2:0xaa);cc_emit(0x67);
        }
        if(right_bytes)cc_drop_stack(2);cc_release(before);
        return op==10?K_INTEGER:cc_unsigned_type(0);
    }
    cc_unsigned_call(op|cc_unsigned_operand(left,0,wide)|cc_unsigned_operand(right,1,wide));
    if(right_bytes)cc_drop_stack(2);cc_release(before);
    if(op==10)return K_INTEGER;
    if(wide)cc_unsigned_hold();else cc_load_indirect(cc_unsigned_type(0));
    return cc_unsigned_type(wide);
}

/* Case dispatch has a 16-bit selector slot; reject wider or sign-changing
 * labels before their source references can be mistaken for scalar values. */
void cc_unsigned_case_label(TypeId selector,TypeId type,uint16_t value) BACKEND {
    if(cc_family(type)==K_LONGWORD||
       (cc_family(selector)==K_INTEGER&&cc_family(type)==K_WORD&&value>32767u)||
       (cc_family(selector)==K_WORD&&cc_family(type)==K_INTEGER&&CCT(type,width)==2&&(int16_t)value<0))cc_fail(46);
}

void cc_unsigned_negative_literal(uint8_t negative) BACKEND {
    if(negative&&cc.token==T_NUMBER&&!cc.hexadecimal&&cc.unsigned_magnitude>32768UL)cc_fail(56);
}

/* Scalar constant validation shares the unsigned representation rules while
 * the ordinary initializer still owns parsing and replaying aggregate data. */
uint8_t cc_unsigned_initializer(TypeId type,TypeId *actual,uint16_t *value,uint8_t emit) BACKEND {
    uint32_t number;uint8_t i;
    if(cc_family(*actual)==K_LONGWORD&&cc_family(type)!=K_LONGWORD) {
        number=cc_unsigned_literal(*value);if(number>65535UL)cc_fail(44);
        *value=(uint16_t)number;*actual=cc_unsigned_type(0);
    }
    if(cc_family(type)==K_INTEGER&&cc_family(*actual)==K_WORD&&*value>32767u)cc_fail(44);
    if(cc_unsigned(type)&&cc_family(*actual)==K_INTEGER&&CCT(*actual,width)==2&&(int16_t)*value<0)cc_fail(44);
    if(cc_family(type)!=K_LONGWORD)return 0;
    number=cc_family(*actual)==K_LONGWORD?cc_unsigned_literal(*value):*value;
    if(emit)for(i=0;i<4;++i)cc_emit((uint8_t)(number>>(8*i)));
    return 1;
}

void cc_unsigned_range(TypeId type) BACKEND {
    cc_unsigned_bounds((uint16_t)CCT(type,low),(uint16_t)CCT(type,high),0x91);
}
void cc_unsigned_bounds(uint16_t low,uint16_t high,uint8_t error) BACKEND {
    uint16_t bad,done;
    if(!cc.range_check||(!low&&high==65535u))return;
    cc_guard();cc_emit(0xe5);cc_op16(0x11,low);cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);cc_emit(0xe1);bad=cc_forward(0xda);
    cc_emit(0xe5);cc_op16(0x11,high);cc_emit(0xeb);cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);cc_emit(0xe1);
    done=cc_forward(0xd2);cc_patch(bad);cc_emit(0x3e);cc_emit(error);cc_emit(0x06);cc_emit(2);cc_op16(0xc3,RT_ERROR);cc_patch(done);
}
