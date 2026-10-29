#include "compiler.h"

/* Shared type classification needs no caller-bank pointers. It belongs to
 * banked compiler code rather than the retained OS/transport prefix. */
uint8_t cc_char_array(uint8_t type) FRONTEND {
    return type<cc.type_count&&cc.types[type].family==K_ARRAY&&
        cc.types[cc.types[type].element].family==K_CHAR&&
        cc.types[cc.types[type].capacity].family==K_INTEGER&&cc.types[type].width<=255;
}
uint8_t cc_ordinal(uint8_t type) FRONTEND {
    return type<cc.type_count&&cc.types[type].family<K_ARRAY&&cc.types[type].family!=K_STRING&&cc.types[type].family!=K_REAL;
}
uint8_t cc_numeric(uint8_t type) FRONTEND {
    return type<cc.type_count&&(cc.types[type].family==K_INTEGER||cc.types[type].family==K_REAL);
}
uint8_t cc_aggregate(uint8_t type) FRONTEND {
    return type<cc.type_count&&(cc.types[type].family==K_ARRAY||cc.types[type].family==K_RECORD);
}
uint8_t cc_set_base(uint8_t type) FRONTEND {
    if(type==K_EMPTY_SET) return 255;
    if(type>=K_SET_EXPR&&type<K_EMPTY_SET) return type-K_SET_EXPR;
    return type<cc.type_count&&cc.types[type].family==K_SET?
        cc.types[cc.types[type].element].family:255;
}
