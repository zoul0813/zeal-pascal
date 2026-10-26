#include "compiler.h"

Compiler cc;

uint8_t cc_char_array(uint8_t type) {
    return type<cc.type_count&&cc.types[type].family==K_ARRAY&&
        cc.types[cc.types[type].element].family==K_CHAR&&
        cc.types[cc.types[type].capacity].family==K_INTEGER&&cc.types[type].width<=255;
}
uint8_t cc_ordinal(uint8_t type) {
    return type<cc.type_count&&cc.types[type].family<K_ARRAY&&cc.types[type].family!=K_STRING&&cc.types[type].family!=K_REAL;
}
uint8_t cc_numeric(uint8_t type) {
    return type<cc.type_count&&(cc.types[type].family==K_INTEGER||cc.types[type].family==K_REAL);
}
uint8_t cc_aggregate(uint8_t type) {
    return type<cc.type_count&&(cc.types[type].family==K_ARRAY||cc.types[type].family==K_RECORD);
}
uint8_t cc_set_base(uint8_t type) {
    if(type==K_EMPTY_SET) return 255;
    if(type>=K_SET_EXPR&&type<K_EMPTY_SET) return type-K_SET_EXPR;
    return type<cc.type_count&&cc.types[type].family==K_SET?
        cc.types[cc.types[type].element].family:255;
}

/* These predicates stay resident: their strings belong to the calling bank. */
uint8_t cc_is(const char *s) { return (cc.token==T_ID||cc.token==T_RESERVED)&&!strcmp(cc.name,s); }

void cc_keyword(const char *s,uint8_t code) { if(!cc_is(s)) cc_fail(code); else cc_next(); }
