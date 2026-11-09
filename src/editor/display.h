#ifndef ZP_MAIN_DISPLAY_H
#define ZP_MAIN_DISPLAY_H
#include "editor/editor.h"

/* IDE-only formatting. The active editor and its Source stay in parent data;
 * no caller-bank string pointer crosses these permanent-bank calls. */
void main_draw_status(uint16_t line,uint8_t clear_tail) SOURCE_BLOCK_CALL;
#endif
