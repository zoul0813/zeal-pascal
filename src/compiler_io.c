#include "compiler.h"

/* A field owns its value before evaluating width/precision. Its three words
 * are value, width, places in push order, all counted as ordinary temporaries.
 * Preparation has no callbacks; the returned body is copied before output or
 * evaluation of a Str destination can invoke Pascal code again. */
static void push_word(void) {
    cc_reserve(2);cc_emit(0xe5);cc.temporaries+=2;
}
static void snapshot(uint16_t bytes) {
    cc_temporary(bytes);cc_emit(0x4e);cc_emit(0x06);cc_emit(0);cc_emit(0x03);
    cc_call(RT_MOVE);cc_op16(0x21,0);cc_emit(0x39);
}
static void integer_field(void) {
    uint16_t before=cc.temporaries;
    if(cc_family(cc_expression())!=K_INTEGER) cc_fail(23);
    cc_release(before);
}
static uint16_t field(uint8_t numeric) {
    uint16_t before=cc.temporaries,bytes;
    uint8_t kind=cc_family(cc_text_value(cc_expression()));
    if(numeric) {if(kind!=K_INTEGER&&kind!=K_REAL) cc_fail(26);}
    else if(kind==K_ARRAY||kind==K_RECORD) cc_fail(62);
    else if(kind==K_SET) cc_fail(66);
    else if(kind>K_CHAR&&kind!=K_REAL) cc_fail(44);
    if(kind==K_STRING&&cc.temporaries==before) snapshot(256);
    push_word();
    if(cc.token==':') {cc_next();integer_field();}
    else cc_op16(0x21,kind==K_REAL?18:0);
    push_word();
    if(kind==K_REAL&&cc.token==':') {cc_next();integer_field();}
    else cc_op16(0x21,0xffff);
    push_word();
    cc_op16(0x21,0);cc_emit(0x39);cc_emit(0x3e);cc_emit(kind);
    cc_reserve(kind==K_REAL?256:0);cc_op16(0xcd,RT_FORMAT);
    bytes=kind==K_STRING?0:kind==K_REAL?66:kind==K_BOOLEAN?6:kind==K_CHAR?2:7;
    if(bytes) snapshot(bytes);
    return bytes+2; /* width word relative to the current SP */
}
void cc_write(uint8_t newline) FRONTEND {
    uint16_t before=cc.temporaries,offset;
    cc_next();
    if(cc.token=='(') {
        cc_next();
        do {
            offset=field(0);cc_stack_operand(offset);cc_call(RT_FIELD);
            cc_release(before);
            if(cc.token!=',') break;
            cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
    } else if(!newline) cc_fail(4);
    if(newline) {
        cc_emit(0x3e);cc_emit(13);cc_call(RT_CHAR);
        cc_emit(0x3e);cc_emit(10);cc_call(RT_CHAR);
    }
}
void cc_str(void) FRONTEND {
    uint16_t before=cc.temporaries,body_before,offset;
    uint8_t target;
    cc_next();cc_expect('(',4);offset=field(1);cc_expect(',',3);
    body_before=cc.temporaries;target=cc_reference(34);
    if(cc_family(target)!=K_STRING) cc_fail(34);
    cc_release(body_before);cc_expect(')',5);
    /* Keep the selected destination while loading width and body from their
     * owned stack slots. Destination-index calls cannot steal preparation. */
    cc_reserve(2);cc_emit(0xe5);
    cc_op16(0x21,offset+2);cc_emit(0x39);
    cc_emit(0x4e);cc_emit(0x23);cc_emit(0x46);
    cc_op16(0x21,2);cc_emit(0x39);cc_emit(0xd1);
    cc_emit(0x3e);cc_emit(cc.types[target].capacity);cc_call(RT_STR);
    cc_release(before);
}
