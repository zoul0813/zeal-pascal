#include "units/internal.h"
#include "compiler/codegen/unsigned/internal.h"

/* Unsigned families are lazy descriptors, like Text: programs that never use
 * them retain their historical primitive IDs and allocate no extra metadata. */
TypeId cc_unsigned_type(uint8_t wide) BACKEND {
    TypeId *cached=wide?&cc.longword_type:&cc.word_type;
    if(!*cached)*cached=cc_new_type(wide?K_LONGWORD:K_WORD,wide?4:2,0,-1,0);
    return *cached;
}
uint8_t cc_unsigned(TypeId type) BACKEND {
    TypeId f=cc_family(type);return f==K_WORD||f==K_LONGWORD;
}
uint8_t cc_integral(TypeId type) BACKEND {
    return cc_family(type)==K_INTEGER||cc_unsigned(type);
}
uint32_t cc_unsigned_literal(uint16_t reference) BACKEND {
    SourcePosition at=cc_reference_position(reference&0x7fffu);uint32_t value=0;uint8_t c,base=10,i;
    if(at&0x80000000UL) {
        for(i=0;i<4;++i)value|=(uint32_t)cc_source_byte(at+i)<<(8*i);
    } else {
        if(cc_source_byte(at)=='$'){base=16;++at;}
        for(;;) {
            c=cc_source_byte(at++);if(c>='A'&&c<='F')c+=32;
            if(c>='0'&&c<='9')c-='0';else if(base==16&&c>='a'&&c<='f')c=c-'a'+10;else break;
            value=value*base+c;
        }
    }
    return reference&0x8000u?0UL-value:value;
}
void cc_unsigned_emit(uint32_t value) BACKEND {
    uint16_t at=cc.emitted+3;uint8_t i;
    cc_address(0xc3,at+4);
    for(i=0;i<4;++i)cc_emit((uint8_t)(value>>(8*i)));
    cc_address(0x21,at);
}
void cc_unsigned_hold(void) BACKEND {
    cc_temporary(4);cc_op16(0x01,4);cc_call(RT_MOVE);cc_op16(0x21,0);cc_emit(0x39);
}
uint8_t cc_unsigned_operand(TypeId type,uint8_t right,uint8_t sign) BACKEND {
    uint8_t flags=0;
    if(cc_family(type)!=K_LONGWORD) {
        flags=right?0x80:0x40;
        if(sign&&cc_family(type)==K_INTEGER&&CCT(type,width)==2)flags|=right?0x20:0x10;
    }
    return flags;
}
void cc_unsigned_not(TypeId type,uint16_t before) BACKEND {
    cc_unsigned_call(11|0x40|cc_unsigned_operand(type,1,0));cc_release(before);cc_unsigned_hold();
}
static void narrow(uint8_t checked) {
    if(checked)cc_unsigned_call(13|0x40);
    else {cc_emit(0x5e);cc_emit(0x23);cc_emit(0x56);cc_emit(0xeb);}
}
uint8_t cc_unsigned_convert(TypeId target,TypeId source,uint8_t cast) BACKEND {
    TypeId tf=cc_family(target),sf=cc_family(source);uint8_t checked=cast!=1;
    if(cast&&tf==K_WORD&&sf==K_POINTER)return 1;
    if(!cc_unsigned(target)&&!cc_unsigned(source))return 0;
    if((!cc_integral(target)&&!(cast&&tf==K_CHAR))||(!cc_integral(source)&&!(cast&&sf==K_CHAR))){cc_fail(44);return 1;}
    if(tf==K_LONGWORD&&sf!=K_LONGWORD) {
        if(checked&&cc.range_check&&sf==K_INTEGER&&CCT(source,width)==2) {
            cc_op16(0x11,0);cc_op16(0x01,32767);cc_call(RT_RANGE);
        }
        cc_unsigned_call(12|0x40|cc_unsigned_operand(source,1,1));cc_unsigned_hold();
    } else if(tf!=K_LONGWORD&&sf==K_LONGWORD)narrow(checked&&cc.range_check);
    else if(checked&&cc.range_check&&tf==K_WORD&&sf==K_INTEGER&&CCT(source,width)==2) {
        cc_op16(0x11,0);cc_op16(0x01,32767);cc_call(RT_RANGE);
    }
    if(checked&&cc.range_check&&tf==K_INTEGER&&CCT(target,width)==2&&(sf==K_WORD||sf==K_LONGWORD)) {
        cc_op16(0x11,0);cc_op16(0x01,32767);cc_call(RT_RANGE);
    }
    if(checked)cc_check_range(target);
    return 1;
}
void cc_unsigned_format(TypeId type) BACKEND {
    cc_temporary(12);cc_emit(0xd5); /* value HL survives reservation; DE is output */
    cc_emit(0xc1);cc_unsigned_call(14|0x40|cc_unsigned_operand(type,1,0));
}
