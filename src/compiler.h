#ifndef ZP_COMPILER_H
#define ZP_COMPILER_H
#include "zp.h"
#ifdef __SDCC
#define FRONTEND __banked
#else
#define FRONTEND
#endif
/* One synchronous compiler owns this workspace until it returns. Source
 * names reference its immutable snapshot; phase banks share only resident data. */
enum { T_EOF=256,T_ID,T_NUMBER,T_STRING,T_ASSIGN,T_RESERVED,T_NE,T_LE,T_GE,T_RANGE };
enum { K_INTEGER,K_STRING,K_BOOLEAN,K_CHAR,K_BYTE };
enum { S_VARIABLE,S_CONSTANT,S_TYPE,S_TYPED,S_PENDING };
/* Names reference the immutable canonical source. All 127 bytes remain
 * significant without spending 128 resident bytes per symbol. */
typedef struct { uint16_t name_start,address; uint8_t name_length,type,role,control,initial_type; } Symbol;
typedef struct { uint16_t width; int16_t low,high; uint8_t family,capacity; } Type;
enum { SYMBOL_LIMIT=512,TYPE_LIMIT=64 };
typedef struct {
    Symbol symbols[SYMBOL_LIMIT];
    Type types[TYPE_LIMIT];
    uint16_t symbol_count;
    uint8_t type_count,depth,range_check;
    const Source *input;
    Source snapshot;
    Build *image;
    Diagnostic *error;
    uint16_t scan,token,token_start,token_end,magnitude,literal_length;
    uint8_t literal[256],hexadecimal;
    char name[128];
    uint16_t emitted,entries,file_bytes;
} Compiler;
extern Compiler cc;

void cc_fail(uint8_t code) FRONTEND;
void cc_put(uint16_t p,uint8_t v) FRONTEND;
void cc_word(uint16_t p,uint16_t v) FRONTEND;
void cc_emit(uint8_t v) FRONTEND;
void cc_op16(uint8_t op,uint16_t value) FRONTEND;
uint16_t cc_forward(uint8_t op) FRONTEND;
void cc_patch(uint16_t at) FRONTEND;
void cc_guard(void) FRONTEND;
void cc_call(uint16_t address) FRONTEND;
void cc_next(void) FRONTEND;
void cc_expect(uint16_t t,uint8_t code) FRONTEND;
int16_t cc_lookup(void) FRONTEND;
uint8_t cc_new_type(uint8_t family,uint16_t width,int16_t low,int16_t high,uint8_t capacity) FRONTEND;
uint8_t cc_family(uint8_t type) FRONTEND;
uint8_t cc_compatible(uint8_t left,uint8_t right) FRONTEND;
uint8_t cc_variable(int16_t id) FRONTEND;
void cc_load_symbol(uint16_t id) FRONTEND;
void cc_store_symbol(uint16_t id) FRONTEND;
void cc_check_range(uint8_t type) FRONTEND;
void cc_constant_literal(uint16_t at) FRONTEND;
void cc_emit_literal(void) FRONTEND;
void cc_constant(uint8_t *type,uint16_t *value) FRONTEND;
int16_t cc_predefined_type(void) FRONTEND;
void cc_declarations(void) FRONTEND;
void cc_typed_storage(void) FRONTEND;
uint8_t cc_is(const char *s);
void cc_keyword(const char *s,uint8_t code);
#endif
