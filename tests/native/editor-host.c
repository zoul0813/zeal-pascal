#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static uint8_t bytes[3][PAGE_SIZE],owned[3];
volatile uint32_t page_versions[64];
static unsigned assertions;
uint8_t page_new(uint8_t role) {
    uint8_t i;
    for(i=0;i<3;++i) if(!owned[i]) { owned[i]=role; return 32+i; }
    return 255;
}
uint8_t page_drop(uint8_t p) { assert(p>=32&&p<35&&owned[p-32]); owned[p-32]=0; return 1; }
uint8_t pages_used(void) { return (owned[0]!=0)+(owned[1]!=0)+(owned[2]!=0); }
void page_initialized(uint8_t p,uint16_t n) { assert(p>=32&&p<35&&owned[p-32]&&n<=PAGE_SIZE); }
uint8_t far_get(uint8_t p,uint16_t at) { assert(p>=32&&p<35&&owned[p-32]&&at<PAGE_SIZE); return bytes[p-32][at]; }
void far_set(uint8_t p,uint16_t at,uint8_t b) { assert(p>=32&&p<35&&owned[p-32]&&at<PAGE_SIZE); bytes[p-32][at]=b; ++page_versions[p]; }
void check(uint8_t ok) { ++assertions; if(!ok) { fprintf(stderr,"editor assertion %u failed: cursor=%u column=%u top=%u\n",assertions,editor.source->cursor,editor.column,editor.top); abort(); } }
void editor_checks(Source *s);
int main(void) {
    Source s;
    check(source_new(&s,"H:/EDITOR.PAS"));
    editor_checks(&s); source_close(&s); check(!pages_used());
    printf("PASS: %u editor buffer, command, viewport and failure assertions\n",assertions);
    return 0;
}
