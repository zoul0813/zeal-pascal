#include "compiler.h"

/* The runtime wrapper owns one call's error landing and restores its exact
 * caller stack on I- failure. Expression callbacks finish before entering it;
 * a failed field must not skip evaluation of the following field. */
static void io_call(uint16_t entry,uint16_t reserve) {
    cc_reserve(reserve+16); cc_op16(0xcd,RT_IOCALL);
    cc_emit((uint8_t)entry); cc_emit((uint8_t)(entry>>8)); cc_emit(cc.io_check);
}
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
/* A complete padded field is at most255 bytes under the recovered width
 * rule. Reuse Str's copy/padding engine; keep the result invocation-owned
 * before entering a disk stream or evaluating the next argument. */
void cc_formatted_value(void) FRONTEND {
    uint16_t offset=field(0);
    cc_temporary(256); /* HL body survives; DE is the new String slot. */
    cc_reserve(4); cc_emit(0xe5); cc_emit(0xd5);
    cc_op16(0x21,offset+260); cc_emit(0x39);
    cc_emit(0x4e); cc_emit(0x23); cc_emit(0x46);
    cc_emit(0xd1); cc_emit(0xe1);
    cc_emit(0x3e); cc_emit(255); cc_call(RT_STR);
    cc_op16(0x21,0); cc_emit(0x39);
}
void cc_write(uint8_t newline) FRONTEND {
    cc.console_used=1;
    uint16_t before=cc.temporaries,offset;
    cc_next();
    if(cc.token=='(') {
        cc_next();
        if(cc_file_prefix()) { cc_file_transfer(1,newline); return; }
        do {
            offset=field(0);cc_stack_operand(offset);io_call(RT_FIELD,0);
            cc_release(before);
            if(cc.token!=',') break;
            cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
    } else if(!newline) cc_fail(4);
    if(newline) {
        cc_emit(0x3e);cc_emit(13);io_call(RT_CHAR,0);
        cc_emit(0x3e);cc_emit(10);io_call(RT_CHAR,0);
    }
}
void cc_str(void) FRONTEND {
    uint16_t before=cc.temporaries,body_before,offset;
    uint8_t target;
    cc_next();cc_expect('(',4);offset=field(1);cc_expect(',',3);
    body_before=cc.temporaries;target=cc_reference(41);
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
void cc_val(void) FRONTEND {
    uint16_t before=cc.temporaries,argument_before;
    uint8_t type,code_type;
    cc_next();cc_expect('(',4);cc_string_value();push_word();cc_expect(',',3);
    argument_before=cc.temporaries;type=cc_reference(41);
    if(cc_family(type)!=K_REAL&&
       (cc_family(type)!=K_INTEGER||cc.types[type].width!=2)) cc_fail(27);
    cc_release(argument_before);push_word();cc_expect(',',3);
    argument_before=cc.temporaries;code_type=cc_reference(41);
    if(cc_family(code_type)!=K_INTEGER||cc.types[code_type].width!=2) cc_fail(24);
    cc_release(argument_before);push_word();cc_expect(')',5);
    cc_op16(0x21,0);cc_emit(0x39);cc_emit(0x3e);
    cc_emit(cc_family(type)==K_REAL?ZR_VAL_REAL:ZR_VAL_INTEGER);
    cc_reserve(256);cc_op16(0xcd,RT_VAL);cc_release(before);
}
void cc_read(uint8_t newline) FRONTEND {
    cc.console_used=1;
    uint16_t before=cc.temporaries;
    uint8_t fresh=cc.input_fresh,type,kind;
    cc_next();
    if(cc.token=='(') {
        SourceCursor cursor;
        cc_source_save(&cursor);cc_next();
        if(cc_file_prefix()) { cc_file_transfer(0,newline); return; }
        cc_source_restore(&cursor);
    }
    cc_emit(0x3e);cc_emit(fresh|((cc.token!='('&&!newline)?2:0));
    io_call(RT_INPUT_BEGIN,64);
    if(cc.token=='(') {
        cc_next();
        do {
            type=cc_reference(41);kind=cc_family(type);
            if(kind!=K_INTEGER&&kind!=K_REAL&&kind!=K_CHAR&&kind!=K_STRING) cc_fail(66);
            cc_release(before);
            cc_emit(0x06);cc_emit(kind==K_INTEGER&&cc.types[type].width==1?K_BYTE:kind);
            cc_emit(0x0e);cc_emit(cc.types[type].capacity);
            io_call(RT_INPUT_VALUE,kind==K_INTEGER||kind==K_REAL?256:64);
            if(cc.token!=',') break;
            cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
    }
    if(newline) io_call(RT_INPUT_END,64);
}

/* Saved failures need a real reported PC for O,F. Memory failures already
 * return their authenticated context to the IDE. This terminal adapter uses
 * raw VID0 syscalls after file cleanup; no Pascal callback, pending I/O error
 * or broken output binding can recursively enter the error path. FC80..FCFF
 * is the inactive program job area, separate from the live return gates. */
void cc_error_finish(void) FRONTEND {
    static const char runtime[]="Runtime error 00, PC=0000\r\n";
    static const char io[]="I/O error 00, PC=0000\r\n";
    static const char video[]="#VID0";
    uint16_t normal=cc.emitted,other,name,nibble,byte,digit,choose,ready,finish;
    uint8_t i;
    if(cc.error->code)return;
    for(i=0;i<sizeof(runtime)-1;++i)cc_emit((uint8_t)runtime[i]);
    other=cc.emitted;for(i=0;i<sizeof(io)-1;++i)cc_emit((uint8_t)io[i]);
    name=cc.emitted;for(i=0;i<sizeof(video);++i)cc_emit((uint8_t)video[i]);
    nibble=cc.emitted;
    cc_emit(0xe6);cc_emit(15);cc_emit(0xfe);cc_emit(10);digit=cc_forward(0xda);
    cc_emit(0xc6);cc_emit(7);cc_patch(digit);cc_emit(0xc6);cc_emit('0');
    cc_emit(0x12);cc_emit(0x13);cc_emit(0xc9);
    byte=cc.emitted;cc_emit(0xf5);
    for(i=0;i<4;++i)cc_emit(0x0f);
    cc_address(0xcd,nibble);cc_emit(0xf1);cc_address(0xc3,nibble);
    cc.error_entry=cc.emitted;
    cc_op16(0x3a,cc_runtime_private(RP_ERROR_CODE));cc_op16(0x32,0xfcbf);
    cc_op16(0x3a,cc_runtime_private(RP_ERROR_NAMESPACE));cc_emit(0xfe);cc_emit(3);choose=cc_forward(0xca);
    cc_address(0x21,normal);cc_op16(0x11,0xfc80);cc_op16(0x01,sizeof(runtime)-1);cc_emit(0xed);cc_emit(0xb0);
    cc_op16(0x11,0xfc8e);cc_emit(0x3e);cc_emit(sizeof(runtime)-1);ready=cc_forward(0xc3);
    cc_patch(choose);
    cc_address(0x21,other);cc_op16(0x11,0xfc80);cc_op16(0x01,sizeof(io)-1);cc_emit(0xed);cc_emit(0xb0);
    cc_op16(0x11,0xfc8a);cc_emit(0x3e);cc_emit(sizeof(io)-1);cc_patch(ready);
    cc_op16(0x32,0xfcbe);cc_op16(0x3a,0xfcbf);cc_address(0xcd,byte);
    for(i=0;i<5;++i)cc_emit(0x13);
    cc_op16(0x2a,0xfc1e);cc_emit(0x7c);cc_address(0xcd,byte);cc_emit(0x7d);cc_address(0xcd,byte);
    cc_address(0x21,name);cc_op16(0x11,0xfcd0);cc_op16(0x01,sizeof(video));cc_emit(0xed);cc_emit(0xb0);
    cc_op16(0x01,0xfcd0);cc_op16(0x21,0x0102);cc_emit(0xcf);
    cc_emit(0xb7);finish=cc_forward(0xfa);
    cc_op16(0x32,0xfcbd);cc_emit(0x67);cc_emit(0x2e);cc_emit(1);
    cc_op16(0x11,0xfc80);cc_op16(0x3a,0xfcbe);cc_emit(0x4f);cc_emit(0x06);cc_emit(0);cc_emit(0xcf);
    cc_op16(0x3a,0xfcbd);cc_emit(0x67);cc_emit(0x2e);cc_emit(3);cc_emit(0xcf);
    cc_patch(finish);cc_op16(0x3a,0xfcbf);cc_emit(0x67);cc_emit(0x2e);cc_emit(15);cc_emit(0xcf);
}
