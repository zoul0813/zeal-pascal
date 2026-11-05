#include "compiler.h"

/* Pascal CRT targets the supported Zeal 80x40 text console, independently of
 * redirected Output and Pascal character callbacks. Helpers live in generated
 * code (>=8000), so page1 can briefly expose the board's character/attribute
 * planes. No syscall, callback or runtime-prefix call occurs while it is mapped.
 * The mapper, page1 and caller IFF are restored before every return. */
enum { CRT_CLEAR,CRT_EOL,CRT_XY,CRT_INSERT,CRT_DELETE,CRT_INIT,CRT_EXIT,
       CRT_LOW,CRT_NORMAL,CRT_HIGH,CRT_DELAY,CRT_FOREGROUND,CRT_BACKGROUND,CRT_WHERE_X,CRT_WHERE_Y };
static void crt_in(uint8_t port) {cc_emit(0xdb);cc_emit(port);}
static void crt_out(uint8_t port) {cc_emit(0xd3);cc_emit(port);}
static void crt_byte(uint8_t value,uint8_t port) {cc_emit(0x3e);cc_emit(value);crt_out(port);}
static void crt_enter(uint8_t vram) {
    uint16_t valid;
    crt_in(0x9c);cc_emit(0xb7);valid=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(0x20);cc_op16(0xc3,RT_IOFAIL);cc_patch(valid);
    cc_emit(0xed);cc_emit(0x57);cc_emit(0xf5);cc_emit(0xf3); /* LD A,I; PUSH AF; DI */
    crt_in(0x8e);cc_emit(0xf5);crt_byte(0,0x8e);
    if(vram) {cc_emit(0x3e);cc_emit(0x40);crt_in(0xf1);cc_emit(0xf5);crt_byte(0x40,0xf1);}
}
static void crt_restore(uint8_t vram) {
    uint16_t disabled;
    if(vram){cc_emit(0xf1);crt_out(0xf1);}
    cc_emit(0xf1);crt_out(0x8e);cc_emit(0xf1);
    disabled=cc_forward(0xe2); /* saved LD A,I parity is the caller IFF2 */
    cc_emit(0xfb);cc_patch(disabled);
}
static void crt_leave(uint8_t vram) {crt_restore(vram);cc_emit(0xc9);}
/* Give pending keyboard interrupts a window between rows. DE/HL/BC survive
 * restore/re-entry. NOP after EI allows an interrupt before DI in crt_enter. */
static void crt_yield(void) {
    crt_restore(1);cc_emit(0);
    /* The mode has already been validated; no failure/stack escape is allowed
     * here. The supported kernel's IRQ handlers do not change video mode. */
    cc_emit(0xed);cc_emit(0x57);cc_emit(0xf5);cc_emit(0xf3);
    crt_in(0x8e);cc_emit(0xf5);crt_byte(0,0x8e);
    cc_emit(0x3e);cc_emit(0x40);crt_in(0xf1);cc_emit(0xf5);crt_byte(0x40,0xf1);
}
/* E is logical row, D is retained. Return mapped character row HL. Physical
 * rows form a40-row ring, each80 bytes; attributes are at row+1000h. */
static uint16_t crt_row(void) {
    uint16_t entry,ready,normalize;
    if(cc.crt_row)return cc.crt_row;
    cc.crt_row=entry=cc.emitted;
    cc_emit(0x7b);normalize=cc.emitted; /* normalize E without changing DE */
    cc_emit(0xfe);cc_emit(40);ready=cc_forward(0xda);
    cc_emit(0xd6);cc_emit(40);cc_address(0xc3,normalize);cc_patch(ready);cc_emit(0x47);
    crt_in(0xa3);normalize=cc.emitted;
    cc_emit(0xfe);cc_emit(40);ready=cc_forward(0xda);
    cc_emit(0xd6);cc_emit(40);cc_address(0xc3,normalize);cc_patch(ready);
    cc_emit(0x80); /* ADD A,B; both normalized before adding */
    cc_emit(0xfe);cc_emit(40);ready=cc_forward(0xda);
    cc_emit(0xd6);cc_emit(40);cc_patch(ready);
    cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
    cc_emit(0x29);cc_emit(0x29);cc_emit(0x29);cc_emit(0x29);
    cc_emit(0x44);cc_emit(0x4d);cc_emit(0x29);cc_emit(0x29);cc_emit(0x09);
    cc_op16(0x01,0x4000);cc_emit(0x09);cc_emit(0xc9);return entry;
}
/* Fill an entire physical row: zero glyph is the console's blank character.
 * Fill attributes with the current rendition, not the previous row's color. */
static void crt_blank(void) {
    cc_emit(0xe5);cc_emit(0x54);cc_emit(0x5d);cc_emit(0x13);
    cc_emit(0x36);cc_emit(0);cc_op16(0x01,79);cc_emit(0xed);cc_emit(0xb0);
    cc_emit(0xe1);cc_op16(0x01,0x1000);cc_emit(0x09);
    cc_emit(0x54);cc_emit(0x5d);cc_emit(0x13);crt_in(0xa5);cc_emit(0x77);
    cc_op16(0x01,79);cc_emit(0xed);cc_emit(0xb0);
}
static uint16_t crt_engine(uint8_t op) {
    uint16_t skip,entry,row=0,loop,done,again,ready,state,invalid_row=0;
    uint8_t i,vram=op==CRT_CLEAR||op==CRT_EOL||op==CRT_INSERT||op==CRT_DELETE;
    if(cc.crt_entries[op])return cc.crt_entries[op];
    skip=cc_forward(0xc3);
    if(op==CRT_INIT||op==CRT_EXIT) {
        if(!cc.crt_state) {cc.crt_state=cc.emitted;for(i=0;i<9;++i)cc_emit(0);}
    }
    if(vram||op==CRT_EXIT)row=crt_row();
    entry=cc.emitted;cc.crt_entries[op]=entry;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    if(op==CRT_DELAY) {
        cc_emit(0xeb);cc_op16(0x2a,RT_PC);cc_emit(0xe5);
        cc_op16(0x21,18);cc_emit(0xcf); /* native msleep(DE) */
        cc_emit(0xe1);cc_op16(0x22,RT_PC);
        cc_emit(0xb7);cc_emit(0xc8);
        cc_emit(0x6f);cc_emit(0x26);cc_emit(0);cc_op16(0x22,0xfe7a);
        cc_emit(0x3e);cc_emit(0xf0);cc_op16(0xc3,RT_IOFAIL);
        cc_patch(skip);return entry;
    }
    crt_enter(vram);
    /* Raw Port writes must not let a malformed cursor row address other
     * board storage or wrap the insert/delete row counters through255. */
    if(vram&&op!=CRT_CLEAR) {
        crt_in(0xa1);cc_emit(0xfe);cc_emit(40);invalid_row=cc_forward(0xd2);
    }
    if(op==CRT_CLEAR) {
        cc_op16(0x11,0);loop=cc.emitted;
        cc_emit(0xd5);cc_address(0xcd,row);crt_blank();cc_emit(0xd1);
        crt_yield();cc_emit(0x1c);cc_emit(0x7b);cc_emit(0xfe);cc_emit(40);cc_address(0xc2,loop);
        cc_emit(0xaf);crt_out(0xa3);crt_out(0xa4);crt_out(0xa1);crt_out(0xa2);
    } else if(op==CRT_XY) {
        cc_emit(0x7d);cc_emit(0x3d);crt_out(0xa1);
        cc_emit(0x7b);cc_emit(0x3d);crt_out(0xa2);
    } else if(op==CRT_LOW||op==CRT_NORMAL||op==CRT_HIGH) {
        /* The original's HighVideo and NormVideo are aliases. Native low is
         * light-gray on black; normal/high is white on black. */
        crt_byte(op==CRT_LOW?7:15,0xa5);crt_byte(op==CRT_LOW?0x70:0xf0,0xa8);
    } else if(op==CRT_FOREGROUND||op==CRT_BACKGROUND) {
        /* Live hardware is authoritative: preserve the other nibble, including
         * changes made by native code/Port, and invert both for the cursor. */
        if(op==CRT_BACKGROUND) {
            cc_emit(0x7d);for(i=0;i<4;++i)cc_emit(0x07);cc_emit(0x4f);
        }
        crt_in(0xa5);cc_emit(0xe6);cc_emit(op==CRT_FOREGROUND?0xf0:0x0f);
        cc_emit(op==CRT_FOREGROUND?0xb5:0xb1);crt_out(0xa5);
        for(i=0;i<4;++i)cc_emit(0x07);crt_out(0xa8);
    } else if(op==CRT_WHERE_X||op==CRT_WHERE_Y) {
        /* DE addresses an invocation-owned result. IOCALL restores its own HL
         * on return, so queries cannot return their value in HL directly. */
        crt_in(op==CRT_WHERE_X?0xa2:0xa1);cc_emit(0x3c);cc_emit(0x12);
        cc_emit(0x13);cc_emit(0xaf);cc_emit(0x12);
    } else if(op==CRT_INIT||op==CRT_EXIT) {
        state=cc.crt_state;
        if(op==CRT_INIT) {
            for(i=0;i<8;++i){crt_in(0xa1+i);cc_address(0x32,state+1+i);}
            /* The kernel line editor also uses the board's single saved-
             * cursor slot. Keep our snapshot entirely in Pascal storage. */
            crt_in(0xa9);cc_emit(0xe6);cc_emit(0x38);cc_address(0x32,state);
            cc_emit(0x3e);cc_emit(1);cc_op16(0x32,cc_runtime_private(RP_CRT_ACTIVE));
            crt_byte(15,0xa5);crt_byte(0xf0,0xa8);
        } else {
            cc_op16(0x3a,cc_runtime_private(RP_CRT_ACTIVE));cc_emit(0xb7);done=cc_forward(0xca);
            /* Restore Y, scrolling and rendition first; X=80 represents a
             * pending wrap and cannot be written directly to cursor X. */
            for(i=0;i<8;++i)if(i!=1){cc_address(0x3a,state+1+i);crt_out(0xa1+i);}
            cc_address(0x3a,state);crt_out(0xa9);
            cc_address(0x3a,state+2);cc_emit(0xfe);cc_emit(80);again=cc_forward(0xd2);
            crt_out(0xa2);ready=cc_forward(0xc3);cc_patch(again);
            /* Recreate pending wrap by reprinting the existing last cell,
             * with its own attribute. Temporarily zero scroll X so the
             * hardware print addresses this exact physical column. */
            crt_byte(79,0xa2);crt_byte(0,0xa4);
            cc_emit(0x3e);cc_emit(0x40);crt_in(0xf1);cc_emit(0xf5);crt_byte(0x40,0xf1);
            crt_in(0xa1);cc_emit(0x5f);cc_address(0xcd,row);
            cc_op16(0x01,79);cc_emit(0x09);cc_emit(0x4e);
            cc_op16(0x11,0x1000);cc_emit(0x19);cc_emit(0x7e);crt_out(0xa5);
            cc_emit(0x79);crt_out(0xa0);
            cc_emit(0xf1);crt_out(0xf1);
            cc_address(0x3a,state+5);crt_out(0xa5);
            cc_address(0x3a,state+4);crt_out(0xa4);
            cc_patch(ready);cc_emit(0xaf);cc_op16(0x32,cc_runtime_private(RP_CRT_ACTIVE));cc_patch(done);
        }
    } else if(op==CRT_EOL) {
        crt_in(0xa1);cc_emit(0x5f);cc_address(0xcd,row); /* row base */
        cc_emit(0x54);cc_emit(0x5d); /* DE row base */
        crt_in(0xa2);cc_emit(0xfe);cc_emit(80);done=cc_forward(0xd2);
        cc_emit(0x4f);cc_emit(0x3e);cc_emit(80);cc_emit(0x91);cc_emit(0x47); /* B=count */
        crt_in(0xa4);again=cc.emitted;
        cc_emit(0xfe);cc_emit(80);ready=cc_forward(0xda);
        cc_emit(0xd6);cc_emit(80);cc_address(0xc3,again);cc_patch(ready);
        cc_emit(0x81);cc_emit(0xfe);cc_emit(80);ready=cc_forward(0xda);
        cc_emit(0xd6);cc_emit(80);cc_patch(ready);cc_emit(0x4f); /* C=column */
        loop=cc.emitted;
        cc_emit(0xc5);cc_emit(0xe5);cc_emit(0x06);cc_emit(0);cc_emit(0x09);
        cc_emit(0x36);cc_emit(0);cc_op16(0x01,0x1000);cc_emit(0x09);
        crt_in(0xa5);cc_emit(0x77);cc_emit(0xe1);cc_emit(0xc1);
        cc_emit(0x0c);cc_emit(0x79);cc_emit(0xfe);cc_emit(80);again=cc_forward(0xda);
        cc_emit(0x0e);cc_emit(0);cc_patch(again);cc_emit(0x05);cc_address(0xc2,loop);cc_patch(done);
    } else {
        crt_in(0xa1);cc_emit(0x57); /* D=cursor row */
        if(op==CRT_INSERT){cc_emit(0x1e);cc_emit(39);cc_emit(0x7b);}
        else cc_emit(0x7a);
        loop=cc.emitted;
        /* Insert copies bottom-up, delete copies top-down. D remains cursor
         * (insert) or current destination (delete), E is current destination. */
        if(op==CRT_INSERT) {cc_emit(0xba);done=cc_forward(0xca);}
        else {cc_emit(0xfe);cc_emit(39);done=cc_forward(0xca);cc_emit(0x5f);}
        cc_emit(0xd5); /* retained row counters */
        if(op==CRT_INSERT)cc_emit(0x1d);else cc_emit(0x1c);
        cc_address(0xcd,row);cc_emit(0xe5); /* source */
        if(op==CRT_INSERT)cc_emit(0x1c);else cc_emit(0x1d);
        cc_address(0xcd,row);cc_emit(0xeb);cc_emit(0xe1);
        cc_op16(0x01,80);cc_emit(0xed);cc_emit(0xb0);
        cc_op16(0x01,0x1000-80);cc_emit(0x09);cc_emit(0xeb);cc_emit(0x09);cc_emit(0xeb);
        cc_op16(0x01,80);cc_emit(0xed);cc_emit(0xb0);cc_emit(0xd1);
        crt_yield();
        if(op==CRT_INSERT){cc_emit(0x1d);cc_emit(0x7b);}
        else {cc_emit(0x14);cc_emit(0x7a);}
        cc_address(0xc3,loop);cc_patch(done);
        if(op==CRT_INSERT)cc_emit(0x5a);else {cc_emit(0x1e);cc_emit(39);}
        cc_address(0xcd,row);crt_blank();
    }
    if(invalid_row)cc_patch(invalid_row);
    crt_leave(vram);cc_patch(skip);return entry;
}
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
