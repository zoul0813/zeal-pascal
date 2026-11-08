#include "compiler_layout.h"
#ifndef ZP_UNITS_EXPERIMENT

#include "../libraries/screen-engine.inc"
static void crt_call(uint16_t entry) {
    cc_reserve(64);cc_op16(0xcd,RT_IOCALL);
    cc_relocate(cc.emitted);cc_emit((uint8_t)entry);cc_emit((uint8_t)(entry>>8));cc_emit(cc.io_check);
}
void cc_crt_query(uint8_t vertical) FRONTEND {
    uint16_t entry;
    if(cc.error->code||cc.suppress)return;
    entry=crt_engine(vertical?CRT_WHERE_Y:CRT_WHERE_X);
    cc_temporary(2);
    cc_emit(0xaf);cc_emit(0x12);cc_emit(0x13);cc_emit(0x12);cc_emit(0x1b);
    crt_call(entry);cc_emit(0xe1);cc.temporaries-=2;
}
static void crt_integer(void) {
    uint16_t before=cc.temporaries;
    if(cc_family(cc_expression())!=K_INTEGER)cc_fail(23);
    cc_release(before);
}
uint8_t cc_crt_statement(void) FRONTEND {
    static const char *const names[]={"clrscr","clreol","gotoxy","insline","delline",
        "crtinit","crtexit","lowvideo","normvideo","highvideo","delay","textcolor","textbackground"};
    uint8_t op;uint16_t entry,before=cc.temporaries;
    if(cc.token!=T_ID||cc_lookup()>=0)return 0;
    for(op=0;op<sizeof(names)/sizeof(names[0]);++op)if(cc_is(names[op]))break;
    if(op==sizeof(names)/sizeof(names[0]))return 0;
    if(op==CRT_HIGH)op=CRT_NORMAL; /* original aliases share their helper */
    cc_next();
    if(op==CRT_XY||op==CRT_DELAY||op==CRT_FOREGROUND||op==CRT_BACKGROUND) {
        cc_expect('(',4);crt_integer();
        cc_op16(0x11,op==CRT_XY?1:0);
        cc_op16(0x01,op==CRT_DELAY?32767:op==CRT_XY?80:15);cc_call(RT_RANGE);
        if(op==CRT_XY) {
            cc_reserve(2);cc_emit(0xe5);cc.temporaries+=2;
            cc_expect(',',3);crt_integer();
            cc_op16(0x11,1);cc_op16(0x01,40);cc_call(RT_RANGE);
            cc_emit(0xd1);cc.temporaries-=2;
        }
        cc_expect(')',5);
    }
    if(!cc.error->code&&!cc.suppress) {
        entry=crt_engine(op);crt_call(entry);
    }
    cc_release(before);return 1;
}

#endif
