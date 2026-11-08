#include "units/internal.h"

uint8_t cc_unit_fragment(uint16_t preferred,uint16_t *fragment,uint16_t *offset) ROUTINES {
    uint16_t at=cc_unit_word(UH_FRAGMENTS),id=0,first,last;
    /* Fragment intervals are appended in emission order. Follow their index,
     * never scan thousands of unrelated global/reference catalog records. */
    while(at) {
        first=cc_unit_word(at+2);last=cc_unit_word(at+4);
        if(preferred>=first&&preferred<last){*fragment=id;*offset=preferred-first;return 1;}
        if(preferred<first)break;
        at=cc_unit_word(at+10);++id;
    }
    cc_fail(UNIT_FORMAT);return 0;
}
