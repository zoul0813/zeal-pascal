#ifndef ZP_COMPILER_H
#define ZP_COMPILER_H
#include "zp.h"
#ifdef __SDCC
#define FRONTEND __banked
#define ROUTINES __banked
#define BACKEND __banked
#else
#define FRONTEND
#define ROUTINES
#define BACKEND
#endif
/* One synchronous compiler owns this workspace until it returns. Source
 * names reference its immutable snapshot; phase banks share only resident data. */
enum { T_EOF=256,T_ID,T_NUMBER,T_STRING,T_ASSIGN,T_RESERVED,T_NE,T_LE,T_GE,T_RANGE };
enum { K_INTEGER,K_STRING,K_BOOLEAN,K_CHAR,K_BYTE };
/* Aggregate families cannot overlap the 5..63 enumeration identity range. */
enum { K_ARRAY=64,K_RECORD,K_SET };
/* Set expression kinds encode the ordinal family without allocating a type
 * descriptor for every constructor. The empty kind has no fixed base family. */
enum { K_SET_EXPR=128,K_EMPTY_SET=192 };
enum { S_VARIABLE,S_CONSTANT,S_TYPE,S_TYPED,S_PENDING,S_VALUE,S_REFERENCE,S_RESULT,S_PROCEDURE,S_FUNCTION,S_FIELD,S_VARIANT,S_VLABEL };
enum { R_PARENT=0,R_FLAGS=1,R_SYMBOL=2,R_ENTRY=4,R_CALLS=6,R_PARAMETERS=8,R_COUNT=10,R_LOCALS=12,R_EXITS=14 };
enum { RF_FORWARD=1,RF_RECURSIVE=2,RF_DEFINED=4,RF_EXTERNAL=8 };
/* Names reference the immutable canonical source. All 127 bytes remain
 * significant without spending 128 resident bytes per symbol. */
typedef struct { uint16_t name_start,address; uint8_t name_length,type,role,control,initial_type,owner; } Symbol;
typedef struct { uint16_t width; int16_t low,high; uint8_t family,capacity,element; } Type;
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
    uint16_t emitted,entries,file_bytes,relocations;
    uint8_t workspace,scope,absolute_mode,var_strict,nesting,suppress;
    uint16_t routine_count,temporaries;
    uint8_t with_depth,with_limit,with_bound,with_types[9];
    uint16_t field_offset;
} Compiler;
extern Compiler cc;

void cc_fail(uint8_t code) FRONTEND;
void cc_put(uint16_t p,uint8_t v) FRONTEND;
void cc_word(uint16_t p,uint16_t v) FRONTEND;
void cc_emit(uint8_t v) FRONTEND;
void cc_op16(uint8_t op,uint16_t value) FRONTEND;
void cc_address(uint8_t op,uint16_t value) FRONTEND;
void cc_relocate(uint16_t operand) FRONTEND;
uint8_t cc_place_code(void) FRONTEND;
void cc_symbol_address(uint16_t id) FRONTEND;
void cc_destination(uint16_t id) FRONTEND;
uint16_t cc_forward(uint8_t op) FRONTEND;
void cc_patch(uint16_t at) FRONTEND;
uint16_t cc_link(uint8_t opcode,uint16_t head) FRONTEND;
void cc_patch_chain(uint16_t head,uint16_t value) FRONTEND;
void cc_guard(void) FRONTEND;
void cc_reserve(uint16_t bytes) FRONTEND;
void cc_temporary(uint16_t bytes) FRONTEND;
void cc_drop_stack(uint16_t bytes) FRONTEND;
void cc_release(uint16_t before) FRONTEND;
void cc_stack_operand(uint16_t right_bytes) FRONTEND;
void cc_load_indirect(uint8_t type) FRONTEND;
void cc_store_indirect(uint8_t type) FRONTEND;
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
uint16_t cc_new_symbol(uint8_t role) FRONTEND;
void cc_result_symbol(uint16_t routine) FRONTEND;
void cc_storage(uint16_t id,uint16_t width) FRONTEND;
uint16_t cc_rget(uint8_t scope,uint8_t field) FRONTEND;
void cc_rput(uint8_t scope,uint8_t field,uint16_t value) FRONTEND;
void cc_frame(uint8_t scope) FRONTEND;
void cc_slot(uint16_t id) FRONTEND;
void cc_convert(uint8_t target,uint8_t source) FRONTEND;
uint8_t cc_char_array(uint8_t type);
uint8_t cc_ordinal(uint8_t type);
uint8_t cc_aggregate(uint8_t type);
uint8_t cc_set_base(uint8_t type);
void cc_set_load(uint8_t type) FRONTEND;
void cc_set_store(uint8_t type) FRONTEND;
int16_t cc_field_lookup(uint8_t type) FRONTEND;
uint16_t cc_field_path(uint8_t type,uint16_t field) FRONTEND;
void cc_with_address(uint16_t field) FRONTEND;
void cc_with_unwind(void) FRONTEND;
uint8_t cc_text_value(uint8_t type) FRONTEND;
int16_t cc_named_type(void) FRONTEND;
int16_t cc_predefined_type(void) FRONTEND;
void cc_declarations(void) FRONTEND;
void cc_typed_storage(uint16_t first) FRONTEND;
void cc_block(void) ROUTINES;
uint8_t cc_invoke(uint16_t id) ROUTINES;
void cc_routine_address(uint16_t id) ROUTINES;
void cc_exit(void) ROUTINES;
uint8_t cc_expression(void) BACKEND;
uint8_t cc_index(uint8_t type) BACKEND;
uint8_t cc_selection(uint16_t id) BACKEND;
uint8_t cc_aggregate_actual(uint8_t type) BACKEND;
void cc_string_statement(uint8_t insert) BACKEND;
void cc_statement(void) BACKEND;
void cc_mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) BACKEND;
uint8_t cc_is(const char *s);
void cc_keyword(const char *s,uint8_t code);
#endif
