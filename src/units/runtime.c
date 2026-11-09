#include "units/internal.h"

/* Runtime operands are names, not embedded implementation addresses in Unit
 * source. Prefix addresses become Runtime16 references and follow final layout;
 * high workspace addresses are platform ABI slots outside the movable prefix. */
uint16_t cc_native_runtime(void) ROUTINES {
    static const char *const names[]={"iostatus","iofail","pc","iocall","range","guard","nativeerror",
        "crtactive","echofd","inputmodevalue","inputmode","inputkey","inputfail","ioerror"};
    static const uint16_t values[]={RT_IOSTATUS,RT_IOFAIL,RT_PC,RT_IOCALL,RT_RANGE,RT_GUARD,0xfe7a};
    static const uint8_t entries[]={RP_CRT_ACTIVE,RP_ECHO_FD,RP_INPUT_MODE_VALUE,RP_INPUT_MODE,
        RP_INPUT_KEY,RP_INPUT_FAIL,RP_IO_ERROR};
    uint8_t i;
    if(!cc.sources->unit_mode||!cc.scope||!(cc_rget(cc.scope,R_FLAGS)&RF_NATIVE)) {
        cc_fail(UNIT_UNSUPPORTED);return 0;
    }
    cc_next();cc_expect('(',4);
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i)if(cc_is(names[i]))break;
    if(i==sizeof(names)/sizeof(names[0])){cc_fail(74);return 0;}
    cc_next();if(cc.token!=')'){cc_fail(5);return 0;}
    return i<7?values[i]:cc_runtime_private(entries[i-7]);
}
