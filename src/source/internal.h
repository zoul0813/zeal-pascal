#ifndef ZP_SOURCE_INTERNAL_H
#define ZP_SOURCE_INTERNAL_H
/* Shared byte/line invariant for near edits and banked block composition. */
static uint8_t line_byte(uint8_t c,uint16_t *line) {
    if(c==26||c=='\r') return 0;
    if(c=='\n') *line=0;
    else if(++*line>127) return 0;
    return 1;
}
#endif
