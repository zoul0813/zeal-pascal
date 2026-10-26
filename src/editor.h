#ifndef ZP_EDITOR_H
#define ZP_EDITOR_H
#include "zp.h"
#ifdef __SDCC
#define EDITOR_CALL __banked
#else
#define EDITOR_CALL
#endif

enum { EDITOR_OK,EDITOR_DONE,EDITOR_REJECT,EDITOR_ABORT };
/* All positions are source offsets; column additionally preserves virtual
 * space. The state stays resident while editor code uses its own MMU bank. */
typedef struct {
    Source *source;
    uint16_t column,top,left,undo_start;
    uint8_t page,width,height,initialized,insert,indent,prefix,quote;
    uint8_t undo_valid,undo_length;
    uint8_t undo[128];
} Editor;
extern Editor editor;

void editor_forget(void) EDITOR_CALL;
void editor_begin(Source *source,uint8_t at_error,uint8_t width,uint8_t height) EDITOR_CALL;
uint8_t editor_key(uint8_t key) EDITOR_CALL;
#endif
