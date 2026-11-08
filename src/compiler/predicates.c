#include "compiler/internal.h"

/* Shared type classification needs no caller-bank pointers. It belongs to
 * banked compiler code rather than the retained OS/transport prefix. */
uint8_t cc_char_array(TypeId type) FRONTEND {
    return type<cc.type_count&&CCT(type,family)==K_ARRAY&&
        CCT(CCT(type,element),family)==K_CHAR&&
        CCT(CCT(type,capacity),family)==K_INTEGER&&CCT(type,width)<=255;
}
uint8_t cc_ordinal(TypeId type) FRONTEND {
    return type<cc.type_count&&((CCT(type,family)<K_ARRAY&&CCT(type,family)!=K_STRING&&CCT(type,family)!=K_REAL)||CCT(type,family)==K_WORD);
}
uint8_t cc_numeric(TypeId type) FRONTEND {
    return type<cc.type_count&&(CCT(type,family)==K_INTEGER||CCT(type,family)==K_REAL||CCT(type,family)==K_WORD||CCT(type,family)==K_LONGWORD);
}
uint8_t cc_aggregate(TypeId type) FRONTEND {
    return type<cc.type_count&&(CCT(type,family)==K_ARRAY||CCT(type,family)==K_RECORD);
}
TypeId cc_set_base(TypeId type) FRONTEND {
    if(type==K_EMPTY_SET) return TYPE_NONE;
    if(type>=K_SET_EXPR&&type<K_EMPTY_SET) return type-K_SET_EXPR;
    return type<cc.type_count&&CCT(type,family)==K_SET?
        CCT(CCT(type,element),family):TYPE_NONE;
}
