#ifndef ZP_EDITOR_H
#define ZP_EDITOR_H
#include "common/zp.h"
#ifdef __SDCC
#define EDITOR_CALL __banked
#else
#define EDITOR_CALL
#endif

enum { EDITOR_OK,EDITOR_DONE,EDITOR_REJECT,EDITOR_ABORT,EDITOR_READ_BLOCK,EDITOR_WRITE_BLOCK,
       EDITOR_FIND,EDITOR_REPLACE,EDITOR_REPEAT };
enum { SEARCH_BACK=1,SEARCH_GLOBAL=2,SEARCH_UPPER=4,SEARCH_WORD=8,SEARCH_NO_CONFIRM=16 };
enum { SEARCH_MORE,SEARCH_MATCH,SEARCH_DONE,SEARCH_MISSING,SEARCH_REJECT };
enum { FIELD_MORE,FIELD_DONE,FIELD_ABORT,FIELD_CANCEL,FIELD_REJECT };
enum { REPAINT_NONE,REPAINT_LINE,REPAINT_TAIL };
typedef struct {
    uint8_t pattern[30],replacement[30];
    uint8_t length,replacement_length,flags,replacing,valid;
    uint16_t count;
} EditorSearch;
typedef struct {
    EditorSearch search;
    Source *source;
    uint32_t revision;
    uint16_t remaining;
    uint16_t point,match,visited;
    uint8_t page,pending,exhausted,active;
} EditorSearchJob;
typedef struct { uint8_t text[30],length,cursor,quote,insert; } EditorField;
/* All positions are source offsets; column additionally preserves virtual
 * space. The state stays resident while editor code uses its own MMU bank. */
typedef struct {
    Source *source;
    uint16_t column,top,left,undo_start;
    uint8_t page,width,height,initialized,insert,indent,prefix,quote;
    uint8_t undo_valid,undo_length;
    uint8_t undo[128];
    uint16_t block_begin,block_end;
    uint8_t block_marks,block_visible;
    EditorSearch search;
    /* Damage from the last editor_key transaction. Only replace() certifies
     * local damage; other mutations require the IDE's full repaint fallback. */
    uint16_t repaint_from;
    uint8_t repaint;
} Editor;
extern Editor editor;

void editor_forget(void) EDITOR_CALL;
void editor_begin(Source *source,uint8_t at_error,uint8_t width,uint8_t height) EDITOR_CALL;
uint8_t editor_key(uint8_t key) EDITOR_CALL;
uint8_t editor_insert_block(const Source *block) EDITOR_CALL;
/* Banked calls take retained stack/data byte arrays, never a caller-bank
 * literal. Configuration and jobs own copies after these calls return. */
uint8_t editor_search_configure(const uint8_t *pattern,uint8_t length,const uint8_t *replacement,
    uint8_t replacement_length,const uint8_t *options,uint8_t options_length,uint8_t replacing) EDITOR_CALL;
uint8_t editor_search_begin(EditorSearchJob *job) EDITOR_CALL;
uint8_t editor_search_step(EditorSearchJob *job) EDITOR_CALL;
uint8_t editor_search_accept(EditorSearchJob *job,uint8_t yes) EDITOR_CALL;
void editor_field_begin(EditorField *field) EDITOR_CALL;
uint8_t editor_field_key(EditorField *field,uint8_t key,const uint8_t *recall,uint8_t recall_length) EDITOR_CALL;
#endif
