#include "compiler.h"

Compiler cc;

/* These predicates stay resident: their strings belong to the calling bank. */
uint8_t cc_is(const char *s) { return (cc.token==T_ID||cc.token==T_RESERVED)&&!strcmp(cc.name,s); }

void cc_keyword(const char *s,uint8_t code) { if(!cc_is(s)) cc_fail(code); else cc_next(); }
