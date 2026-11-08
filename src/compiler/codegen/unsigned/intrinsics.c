#include "compiler/internal.h"

TypeId cc_unsigned_intrinsic(uint8_t op,TypeId type,uint16_t before) BACKEND {
    uint8_t wide=cc_family(type)==K_LONGWORD,i;
    if(op==F_ORD||op==F_ABS)return type;
    if(op==F_SUCC||op==F_PRED||op==F_SQR) {
        cc_emit(0x54);cc_emit(0x5d);
        if(op!=F_SQR)cc_op16(0x21,1);
        TypeId result=cc_unsigned_binary(op==F_SUCC?0:op==F_PRED?1:2,type,op==F_SQR?type:K_INTEGER,before,0);
        if(op!=F_SQR){cc_check_range(type);return type;}return result;
    }
    if(op==F_ODD) {
        if(wide)cc_emit(0x7e);else cc_emit(0x7d);
        cc_emit(0xe6);cc_emit(1);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);cc_release(before);return K_BOOLEAN;
    }
    if(op==F_CHR) {cc_unsigned_convert(K_CHAR,type,2);cc_emit(0x26);cc_emit(0);cc_release(before);return K_CHAR;}
    if(op==F_HI||op==F_LO) {
        if(wide){if(op==F_HI){cc_emit(0x23);cc_emit(0x23);}cc_load_indirect(cc_unsigned_type(0));}
        else {if(op==F_HI)cc_emit(0x6c);cc_emit(0x26);cc_emit(0);}
        cc_release(before);return wide?cc_unsigned_type(0):K_INTEGER;
    }
    if(op==F_SWAP) {
        if(!wide){cc_emit(0x7c);cc_emit(0x65);cc_emit(0x6f);return type;}
        for(i=0;i<4;++i){cc_emit(0x7e);cc_op16(0x32,RT_REAL_BUFFER+((i+2)&3));cc_emit(0x23);}
        cc_op16(0x21,RT_REAL_BUFFER);cc_release(before);cc_unsigned_hold();return type;
    }
    cc_fail(23);return K_INTEGER;
}

/* A Word index is compared as unsigned even when the array bounds use the
 * signed Integer family. A signed index cannot alias an upper-half Word. */
void cc_unsigned_index(TypeId index,TypeId actual,uint16_t capacity) BACKEND {
    uint16_t low,high;
    if(index==TYPE_NONE){cc_unsigned_bounds(0,capacity,0x90);return;}
    low=(uint16_t)CCT(index,low);high=(uint16_t)CCT(index,high);
    if(cc_family(index)==K_WORD) {
        if(cc_family(actual)==K_INTEGER&&CCT(actual,width)==2)cc_unsigned_bounds(0,32767,0x90);
    } else {
        if((int16_t)high<0){low=1;high=0;}
        else if((int16_t)low<0)low=0;
    }
    cc_unsigned_bounds(low,high,0x90);
}
