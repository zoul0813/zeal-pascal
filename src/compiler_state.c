#include "compiler.h"

Compiler cc;
uint8_t compile_interrupt_reason(void) { return cc.interrupted; }
uint8_t cc_poll(void) {
    /* The synchronous job alone enables polling. Build authentication also
     * uses hash helpers after the job's stack-owned diagnostic has expired. */
    if(!cc.polling)return 1;
    if(cc.error->code)return 0;
    cc.interrupted=key_interrupt();
    if(!cc.interrupted)return 1;
    cc.error->code=COMPILE_INTERRUPTED;return 0;
}

/* These predicates stay resident: their strings belong to the calling bank. */
uint8_t cc_is(const char *s) { return (cc.token==T_ID||cc.token==T_RESERVED)&&!strcmp(cc.name,s); }

void cc_keyword(const char *s,uint8_t code) { if(!cc_is(s)) cc_fail(code); else cc_next(); }
