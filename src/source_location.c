#include "zp.h"

/* Permanent source-position service. Source and output pointers belong to
 * retained parent data/stack; no caller-bank pointer crosses this call. */
void source_location(const Source *s, uint16_t p, uint16_t *line, uint16_t *col) SOURCE_BLOCK_CALL {
    uint16_t i,end=s->length,row=1,column=1;
    uint8_t page=s->page;
    /* Source is unchanged during this read. Keep descriptor loads and output
     * stores outside the loop, particularly under the banked calling ABI. */
    if(p<end) end=p;
    for(i=0;i<end;++i) {
        if(buffer_get(page,i)=='\n') { ++row; column=1; }
        else ++column;
    }
    *line=row; *col=column;
}
