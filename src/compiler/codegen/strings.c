#include "compiler/internal.h"

/* Short-string expression ownership, bounded snapshots and mutations share
 * the existing parkable compiler phase. No target runtime page is added. */
static uint16_t string_value(void) {
    uint16_t before=cc.temporaries;
    TypeId kind;TypeId type;
    uint16_t bytes;
    if(cc.token==')'||cc.token==','||cc.token==T_EOF) { cc_fail(41); return 0; }
    type=cc_text_value(cc_expression()); kind=cc_family(type);
    /* Preserve TP3's permissive V-/raw string lengths without clipping them
     * to a declared capacity. Every String/Copy slot can hold 255 bytes. */
    bytes=kind==K_CHAR?2:256;
    if(kind==K_CHAR) {
        cc_temporary(2); cc_emit(0x3e); cc_emit(1); cc_emit(0x12);
        cc_emit(0x13); cc_emit(0x7d); cc_emit(0x12); cc_op16(0x21,0); cc_emit(0x39);
    } else if(kind!=K_STRING) cc_fail(33);
    else if(cc.temporaries==before) cc_string_snapshot(bytes);
    return bytes;
}
static void integer_value(void) { if(cc_family(cc_expression())!=K_INTEGER) cc_fail(23); }
void cc_string_value(void) BACKEND { string_value(); }
static void push_value(void) { cc_guard(); cc_emit(0xe5); }
TypeId cc_string_function(uint8_t op) BACKEND {
    uint16_t before=cc.temporaries,right_before,right_bytes,owned,bytes;
    bytes=string_value();
    if(op==CC_STR_CONCAT) {
        while(cc.token==','&&!cc.error->code) {
            push_value(); right_before=cc.temporaries; cc_next(); string_value();
            right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes);
            cc_op16(0x01,cc.temporaries-before+(right_bytes?2:0)); cc_emit(0x3e); cc_emit(0);
            cc_call(RT_CONCAT); cc.temporaries=before+256;
        }
        cc_expect(')',5); return K_STRING;
    }
    push_value(); cc_expect(',',3);
    if(op==CC_STR_POS) {
        right_before=cc.temporaries; string_value(); cc_expect(')',5);
        right_bytes=cc.temporaries-right_before; cc_stack_operand(right_bytes); cc_call(RT_POS);
        if(right_bytes) cc_drop_stack(2);
        cc_release(before); return K_INTEGER;
    }
    integer_value(); push_value(); cc_expect(',',3); integer_value(); push_value(); cc_expect(')',5);
    owned=cc.temporaries-before; cc_temporary(bytes);
    cc_op16(0x21,bytes); cc_emit(0x39); cc_op16(0x11,owned+6); cc_call(RT_SUBSTRING);
    cc.temporaries=before+bytes; return K_STRING;
}
static TypeId string_target(void) {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    TypeId type;
    if(id>=0&&CCS(id,role)==S_RESULT) { cc_fail(41); return K_STRING; }
    if(!cc_variable(id)||CCS(id,control)) { cc_fail(34); return K_STRING; }
    type=cc_selection((uint16_t)id);
    if(cc_family(type)!=K_STRING) cc_fail(34);
    return type;
}
void cc_string_statement(uint8_t insert) BACKEND {
    uint16_t before=cc.temporaries;
    TypeId type=K_STRING;
    cc_next(); cc_expect('(',4);
    if(insert) { string_value(); push_value(); cc_expect(',',3); }
    type=string_target(); push_value(); cc_expect(',',3);
    integer_value();
    if(insert) {
        cc_emit(0x44); cc_emit(0x4d); cc_emit(0xd1); cc_emit(0xe1);
        cc_emit(0x3e); cc_emit(CCT(type,capacity));
    } else {
        push_value(); cc_expect(',',3); integer_value();
        cc_emit(0xeb); cc_emit(0xc1); cc_emit(0xe1);
    }
    cc_expect(')',5); cc_call(insert?RT_INSERT:RT_DELETE); cc_release(before);
}
