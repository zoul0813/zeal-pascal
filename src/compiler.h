#ifndef ZP_COMPILER_H
#define ZP_COMPILER_H
#include "zp.h"
#include "real.h"
#include "sources.h"
#include "chain_abi.h"
#include <stddef.h>
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
 * names reference its suspended inputs; phase banks share only resident data. */
enum { T_EOF=256,T_ID,T_NUMBER,T_STRING,T_ASSIGN,T_RESERVED,T_NE,T_LE,T_GE,T_RANGE,T_REAL };
enum { K_INTEGER,K_STRING,K_BOOLEAN,K_CHAR,K_BYTE,K_REAL };
/* Aggregate family tags are separate from ordinary ordinal type identities. */
enum { K_ARRAY=0x8000u,K_RECORD,K_SET,K_POINTER,K_FILE };
enum { FILE_TEXT,FILE_TYPED,FILE_UNTYPED };
/* Set expression kinds encode the ordinal family without allocating a type
 * descriptor for every constructor. The empty kind has no fixed base family. */
enum { K_SET_EXPR=0x8000u,K_EMPTY_SET=0xfffcu,K_ANY_POINTER,K_NIL,TYPE_NONE };
typedef uint16_t TypeId;
enum { S_VARIABLE,S_CONSTANT,S_TYPE,S_TYPED,S_PENDING,S_VALUE,S_REFERENCE,S_RESULT,S_PROCEDURE,S_FUNCTION,S_SYSTEM,S_ADDRESS,S_ALIAS,S_LABEL,S_STANDARD,S_HOOK,S_FIELD,S_VARIANT,S_VLABEL };
enum { HOOK_STATUS,HOOK_CON_OUT,HOOK_LST_OUT,HOOK_AUX_OUT,HOOK_USR_OUT,HOOK_COUNT };
enum { R_PARENT=0,R_FLAGS=2,R_SYMBOL=4,R_ENTRY=6,R_CALLS=8,R_PARAMETERS=10,R_COUNT=12,R_LOCALS=14,R_EXITS=16,
       R_DESCRIPTOR=18,R_THUNK=20,R_LOCAL_GUARD=22,R_LOCAL_ALLOC=24 };
#define ROUTINE_BYTES 26u
#define RELOCATION_BYTES 4u
enum { RF_FORWARD=1,RF_RECURSIVE=2,RF_DEFINED=4,RF_EXTERNAL=8 };
/* Published saved IX/return/arguments and lexical/lifetime slots stay fixed.
 * The private words are caller cursor[-6] and owned-local lower bound[-8]. */
#define FRAME_PRIVATE_BYTES 8u
/* Names reference the suspended input text. All 127 bytes remain
 * significant without spending 128 resident bytes per symbol. */
typedef struct { uint16_t name_start,address; uint8_t name_length;TypeId type;uint8_t role;uint8_t control;TypeId initial_type;TypeId owner; } Symbol;
typedef struct { uint16_t width; int16_t low,high; TypeId family;TypeId capacity;TypeId element; } Type;
/* Signed symbol lookup reserves -1; high type bits encode expression kinds.
 * Physical metadata allocation normally exhausts well before these encodings. */
enum { SYMBOL_LIMIT=32767u,TYPE_LIMIT=0x7ffcu };
/* Preferred code addresses never wrap through zero. The final image reserves
 * the specified2 KiB initial stack below FC00; globals share this static span. */
#define CC_CODE_BASE 0x8000u
#define CC_ACTIVATION_BYTES CHAIN_ACTIVATION_BYTES
#define CC_STATIC_LIMIT 0xf400u
typedef uint32_t SourcePosition; /* file ID in high word, source offset in low */
struct CompilerGroups;
typedef struct {
    const Source *root;
    uint8_t file,error_file,external_error,memory;
    uint16_t refs,parent_scan,input_base;
    SourcePosition highest_reference;
    uint8_t parent_flags;uint8_t parent_limit;
    const CompileOptions *options;
    struct CompilerGroups *groups; /* Invocation-owned; NULL without overlays. */
#ifdef ZP_UNITS_EXPERIMENT
    uint8_t unit_page,unit_mode,unit_cbreak,unit_real;uint16_t unit_qualifier;
    uint16_t unit_owner,unit_start;
#endif
} CompilerSources;
typedef struct {
    SourcePosition first,last;
    uint16_t scan,token,parent_scan;
    uint8_t file;uint8_t flags;uint8_t limit;uint8_t parent_flags;uint8_t parent_limit;
#ifdef ZP_UNITS_EXPERIMENT
    uint16_t unit_qualifier;
#endif
} SourceCursor;
typedef struct {
    uint16_t symbol_count;
    uint16_t type_count,depth;uint8_t range_check;
    Source *input;
    CompilerSources *sources;
    Build *image;
    Diagnostic *error;
    uint8_t polling;uint8_t interrupted;uint8_t scan_poll;
    uint16_t scan,token,magnitude,literal_length;
    SourcePosition token_start,token_end;
    uint8_t literal[256];uint8_t hexadecimal;uint8_t number_overflow;
    char name[128];
    uint16_t emitted,entries,relocations;
    uint8_t stage_page;uint8_t primary_stage;uint8_t placing;uint16_t scope;uint8_t absolute_mode;uint8_t var_strict;uint16_t nesting;uint8_t suppress;uint8_t index_speed;
    uint16_t routine_count,temporaries;
    uint8_t with_depth;uint8_t with_limit;uint8_t with_bound;TypeId with_types[9];uint8_t for_depth;uint8_t input_fresh;uint8_t io_check;uint8_t user_interrupt;uint8_t console_control;
    uint16_t field_offset;
    uint16_t random_entry,random_state;
    TypeId text_type;
    uint16_t parameter_entry;
    uint16_t file_entry;
    uint16_t standard_table_operand,file_stream_operand;
    uint16_t keyboard_entry,keyboard_poll,keyboard_status,keyboard_state,interrupt_entry;
    uint16_t keyboard_probe,keyboard_enqueue,keyboard_console_probe;
    uint8_t console_used;uint8_t console_dynamic;
    uint16_t crt_entries[15],crt_state,crt_row;
    uint16_t hook_variables[HOOK_COUNT],hook_defaults[HOOK_COUNT];
    uint16_t hook_output_calls,hook_mask_call;
    uint16_t hook_character,hook_echo,hook_open;
    uint16_t error_entry;
    uint16_t transfer_entry; /* call fixup head until transfer finish, then entry */
    uint8_t full_memory; /* Raw/heap capability requires independent windows. */
    uint16_t page_entry; /* Optional shared-page startup address. */
    uint8_t shared_stack;
    uint16_t unpack_entry; /* shared module decoder fixup head, then entry */
#ifdef ZP_OPTIONAL_REAL
    uint8_t real_needed;
#endif
} Compiler;
extern Compiler cc;
/* Field tags contain the byte offset and16-bit-width flag. Private host and
 * target record padding need not match: neither is an installed format. */
#define CC_S_FIELD(f) ((uint8_t)((offsetof(Symbol,f)<<1)|(sizeof(((Symbol*)0)->f)==2)))
#define CC_T_FIELD(f) ((uint8_t)((offsetof(Type,f)<<1)|(sizeof(((Type*)0)->f)==2)))
#define CCS(id,f) cc_symbol_value((id),CC_S_FIELD(f))
#define CCS_SET(id,f,v) cc_symbol_set((id),CC_S_FIELD(f),(uint16_t)(v))
#define CCT_SET(id,f,v) cc_type_set((id),CC_T_FIELD(f),(uint16_t)(v))
#define CC_TYPE_width(id) cc_type_value((id),CC_T_FIELD(width))
#define CC_TYPE_low(id) ((int16_t)cc_type_value((id),CC_T_FIELD(low)))
#define CC_TYPE_high(id) ((int16_t)cc_type_value((id),CC_T_FIELD(high)))
#define CC_TYPE_family(id) cc_type_value((id),CC_T_FIELD(family))
#define CC_TYPE_capacity(id) cc_type_value((id),CC_T_FIELD(capacity))
#define CC_TYPE_element(id) cc_type_value((id),CC_T_FIELD(element))
#define CCT(id,f) CC_TYPE_##f(id)
uint8_t cc_symbol_read(uint16_t id,Symbol *out) FRONTEND;
uint8_t cc_reference_read(uint16_t id,uint8_t *out,uint8_t count) FRONTEND;
uint16_t cc_symbol_value(uint16_t id,uint8_t field) FRONTEND;
void cc_symbol_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND;
uint16_t cc_type_value(uint16_t id,uint8_t field) FRONTEND;
void cc_type_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND;
void cc_symbol_clear(uint16_t id) FRONTEND;
void cc_symbol_copy(uint16_t to,uint16_t from) FRONTEND;
uint16_t cc_routine_value(uint16_t id,uint8_t field) FRONTEND;
void cc_routine_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND;
void cc_routine_clear(uint16_t id) FRONTEND;
uint8_t cc_reloc_get(uint16_t offset) FRONTEND;
void cc_reloc_put(uint16_t offset,uint8_t value) FRONTEND;
uint8_t cc_reference_get(uint16_t id,uint8_t field) FRONTEND;
void cc_reference_put(uint16_t id,uint8_t field,uint8_t value) FRONTEND;
uint8_t cc_references_end(void) FRONTEND;
uint8_t cc_tables_begin(void) FRONTEND;
uint8_t cc_stack_check(void) FRONTEND;
uint8_t cc_tables_end(void) FRONTEND;
uint8_t cc_poll(void);
void cc_activation_begin(void) FRONTEND;
void cc_activation_place(void) FRONTEND;
void cc_startup_finish(void) FRONTEND;
void cc_require_full_memory(void) FRONTEND;
uint8_t cc_seal(const Source *root,const CompileOptions *options) FRONTEND;
void build_discard_image(Build *build) SOURCE_BLOCK_CALL;

void cc_fail(uint8_t code) FRONTEND;
SourcePosition cc_source_position(void) ROUTINES;
uint8_t cc_source_byte(SourcePosition position) ROUTINES;
uint16_t cc_source_reference(SourcePosition position) ROUTINES;
SourcePosition cc_reference_position(uint16_t reference) ROUTINES;
void cc_reference_seek(uint16_t reference) ROUTINES;
void cc_source_seek(SourcePosition position) ROUTINES;
void cc_source_save(SourceCursor *cursor) ROUTINES;
void cc_source_restore(const SourceCursor *cursor) ROUTINES;
uint8_t cc_source_resume(void) ROUTINES;
void cc_source_include(uint8_t comment) ROUTINES;
uint8_t cc_sources_prepare(const Source *root) ROUTINES;
uint8_t cc_sources_finish(void) ROUTINES;
void cc_sources_error(void) ROUTINES;
uint8_t cc_sources_ready(const Source *root,const Build *build) ROUTINES;
void cc_put(uint16_t p,uint8_t v);
void cc_global_put(uint16_t p,uint8_t v) FRONTEND;
uint8_t cc_code_get(uint16_t address);
void cc_stage_put(uint16_t offset,uint8_t value) FRONTEND;
uint8_t cc_stage_get(uint16_t offset) FRONTEND;
void cc_word(uint16_t p,uint16_t v);
void cc_emit(uint8_t v);
void cc_op16(uint8_t op,uint16_t value);
void cc_address(uint8_t op,uint16_t value) FRONTEND;
void cc_relocate(uint16_t operand) FRONTEND;
void cc_relocate_value(uint16_t operand,uint16_t coefficient,uint8_t byte) FRONTEND;
uint8_t cc_place_code(void) FRONTEND;
uint8_t cc_image_page(uint8_t index) FRONTEND;
void cc_symbol_address(uint16_t id) FRONTEND;
void cc_destination(uint16_t id) FRONTEND;
uint16_t cc_forward(uint8_t op) FRONTEND;
void cc_patch(uint16_t at) FRONTEND;
uint16_t cc_link(uint8_t opcode,uint16_t head) FRONTEND;
void cc_patch_chain(uint16_t head,uint16_t value) FRONTEND;
void cc_guard(void) FRONTEND;
void cc_capture_guard(uint16_t bytes,uint8_t character) FRONTEND;
void cc_preserve_registers(uint8_t restore) FRONTEND;
uint16_t cc_original_entry(uint16_t entry,uint8_t staged) FRONTEND;
void cc_finish(uint16_t entry) FRONTEND;
void cc_reserve(uint16_t bytes) FRONTEND;
void cc_temporary(uint16_t bytes) FRONTEND;
void cc_string_snapshot(uint16_t bytes) FRONTEND;
void cc_drop_stack(uint16_t bytes) FRONTEND;
void cc_release(uint16_t before) FRONTEND;
void cc_stack_operand(uint16_t right_bytes) FRONTEND;
void cc_load_indirect(TypeId type) FRONTEND;
void cc_store_indirect(TypeId type) FRONTEND;
void cc_call(uint16_t address) FRONTEND;
void cc_next(void) FRONTEND;
void cc_control_literal(void) FRONTEND;
#if defined(ZP_OPTIONAL_REAL) || defined(ZP_UNITS_EXPERIMENT)
void cc_require_real(void) FRONTEND;
#else
#define cc_require_real() ((void)0)
#endif
#ifdef ZP_OPTIONAL_REAL
uint8_t cc_integer_read(uint16_t at,uint8_t *out,uint16_t count) SOURCE_BLOCK_CALL;
uint8_t cc_integer_get(uint16_t at) SOURCE_BLOCK_CALL;
uint8_t cc_real_runtime_finish(void) SOURCE_BLOCK_CALL;
#endif
uint16_t cc_runtime_size(void);
uint8_t cc_runtime_get(uint16_t offset);
uint8_t cc_runtime_read(uint16_t offset,uint8_t *out,uint16_t count);
uint8_t cc_runtime_copy(uint8_t page);
/* Build-bound private input addresses; never an installed/public runtime ABI. */
enum { RP_INPUT_READY,RP_INPUT_ADVANCE,RP_LINE_NEXT,RP_LINE_REMAINING,RP_LINE_EOF,RP_LINE_VALID,
       RP_INPUT_KEY,RP_INPUT_RAW_KEY,RP_INPUT_MODE,RP_INPUT_CLEANUP,RP_INPUT_FAIL,RP_IO_ERROR,RP_ECHO_FD,
       RP_INPUT_SOURCE,RP_INPUT_OPEN,RP_INPUT_ECHO,RP_INPUT_CLOSE,RP_USER_BREAK,
       RP_INPUT_MODE_VALUE,RP_INPUT_BREAK,RP_INPUT_CONSOLE_AREA,
       RP_NORMAL_ERROR,RP_ERROR_CODE,RP_ERROR_NAMESPACE,RP_CRT_ACTIVE,RP_PROGRAM_ENTER,RP_COUNT };
uint16_t cc_runtime_private(uint8_t entry);
/* Profile follows the private table in the same immutable BANK8 asset. */
#define cc_runtime_profile_get(offset) cc_runtime_get(cc_runtime_size()+2u*RP_COUNT+(offset))
void cc_keypressed(void) FRONTEND;
void cc_logical_stream(void) FRONTEND;
void cc_keyboard_engine(void) FRONTEND;
void cc_keyboard_finish(void) FRONTEND;
void cc_interrupt_finish(void) FRONTEND;
void cc_file_engine_finish(void) FRONTEND;
void cc_user_interrupt(void) FRONTEND;
void cc_console_finish(void) FRONTEND;
uint8_t cc_crt_statement(void) FRONTEND;
void cc_crt_query(uint8_t vertical) FRONTEND;
int16_t cc_predefined_color(void) FRONTEND;
void cc_error_finish(void) FRONTEND;
void cc_params_finish(void) FRONTEND;
void cc_random_finish(void) FRONTEND;
void cc_hook_variable(uint16_t id) ROUTINES;
void cc_hook_output(void) ROUTINES;
void cc_hooks_finish(void) ROUTINES;
void cc_hooks_place(uint16_t delta) ROUTINES;
uint16_t cc_file_default_output(void) FRONTEND;
uint16_t cc_file_asset_size(uint8_t gate);
uint8_t cc_file_asset_get(uint8_t gate,uint16_t offset);
uint16_t cc_transfer_asset_size(uint8_t kind) FRONTEND;
uint8_t cc_transfer_asset_get(uint8_t kind,uint16_t offset) FRONTEND;
void cc_transfer_statement(uint16_t base) FRONTEND;
void cc_transfer_finish(void) FRONTEND;
void cc_unpack_reference(uint16_t operand) FRONTEND;
void cc_unpack_finish(void) FRONTEND;
void cc_expect(uint16_t t,uint8_t code) FRONTEND;
int16_t cc_lookup(void) FRONTEND;
#ifdef ZP_UNITS_EXPERIMENT
int16_t cc_lookup_decl(void) FRONTEND;
#else
#define cc_lookup_decl cc_lookup
#endif
void cc_system_init(void) FRONTEND;
TypeId cc_standard_reference(uint8_t identity) FRONTEND;
TypeId cc_new_type(TypeId family,uint16_t width,int16_t low,int16_t high,TypeId capacity) FRONTEND;
TypeId cc_family(TypeId type) FRONTEND;
uint8_t cc_compatible(TypeId left,TypeId right) FRONTEND;
uint8_t cc_variable(int16_t id) FRONTEND;
void cc_load_symbol(uint16_t id) FRONTEND;
void cc_store_symbol(uint16_t id) FRONTEND;
void cc_check_range(TypeId type) FRONTEND;
void cc_constant_literal(uint16_t at) FRONTEND;
void cc_emit_literal(void) FRONTEND;
void cc_constant(TypeId *type,uint16_t *value) FRONTEND;
uint16_t cc_new_symbol(uint8_t role) FRONTEND;
void cc_result_symbol(uint16_t routine) FRONTEND;
void cc_storage(uint16_t id,uint16_t width) FRONTEND;
void cc_bind_storage(uint16_t id) FRONTEND;
uint16_t cc_rget(uint16_t scope,uint8_t field) FRONTEND;
void cc_rput(uint16_t scope,uint8_t field,uint16_t value) FRONTEND;
void cc_frame(uint16_t scope) FRONTEND;
void cc_recursion_save(void) ROUTINES;
void cc_recursion_restore(void) ROUTINES;
uint8_t cc_frame_required(uint16_t scope) FRONTEND;
void cc_slot(uint16_t id) FRONTEND;
void cc_convert(TypeId target,TypeId source) FRONTEND;
uint8_t cc_char_array(TypeId type) FRONTEND;
uint8_t cc_ordinal(TypeId type) FRONTEND;
uint8_t cc_numeric(TypeId type) FRONTEND;
uint8_t cc_aggregate(TypeId type) FRONTEND;
TypeId cc_set_base(TypeId type) FRONTEND;
void cc_set_load(TypeId type) FRONTEND;
void cc_set_store(TypeId type) FRONTEND;
int16_t cc_field_lookup(TypeId type) FRONTEND;
uint16_t cc_field_path(TypeId type,uint16_t field) FRONTEND;
void cc_with_address(uint16_t field) FRONTEND;
void cc_with_unwind(void) FRONTEND;
TypeId cc_text_value(TypeId type) FRONTEND;
int16_t cc_named_type(void) FRONTEND;
int16_t cc_predefined_type(void) FRONTEND;
void cc_declarations(void) FRONTEND;
void cc_typed_storage(uint16_t first) FRONTEND;
SourcePosition cc_block(void) ROUTINES;
TypeId cc_invoke(uint16_t id) ROUTINES;
void cc_routine_address(uint16_t id) ROUTINES;
void cc_entry_reference(uint8_t opcode,uint16_t scope) ROUTINES;
void cc_exit(void) ROUTINES;
TypeId cc_expression(void) BACKEND;
TypeId cc_index(TypeId type) BACKEND;
TypeId cc_selection(uint16_t id) BACKEND;
TypeId cc_aggregate_actual(TypeId type) BACKEND;
void cc_string_statement(uint8_t insert) BACKEND;
void cc_string_value(void) BACKEND;
enum { CC_STR_COPY,CC_STR_POS,CC_STR_CONCAT };
TypeId cc_string_function(uint8_t op) BACKEND;
SourcePosition cc_statement(void) BACKEND;
void cc_inline(void) FRONTEND;
void cc_labels(void) BACKEND;
void cc_labels_finish(void) BACKEND;
uint8_t cc_runtime_statement(void) BACKEND;
TypeId cc_reference(uint8_t code) BACKEND;
void cc_mapping(uint16_t begin,uint16_t end,SourcePosition first,SourcePosition last) BACKEND;
uint8_t cc_is(const char *s);
void cc_keyword(const char *s,uint8_t code);
#define REAL_PI 0x7fffu
void cc_real_literal(uint16_t reference) FRONTEND;
void cc_real_constant(TypeId type,uint16_t value) FRONTEND;
void cc_real_emit(uint16_t reference) FRONTEND;
void cc_real_hold(void) FRONTEND;
void cc_real_unary(uint8_t op,TypeId type,uint16_t before) FRONTEND;
void cc_real_binary(uint8_t op,TypeId left,TypeId right,uint16_t before,uint16_t right_bytes) FRONTEND;
void cc_write(uint8_t newline) FRONTEND;
void cc_str(void) FRONTEND;
void cc_val(void) FRONTEND;
void cc_read(uint8_t newline) FRONTEND;
void cc_random(uint8_t bounded) FRONTEND;
void cc_randomize(void) FRONTEND;
void cc_params(uint8_t string_result) FRONTEND;
uint8_t cc_file_statement(void) FRONTEND;
int16_t cc_file_function(void) FRONTEND;
uint8_t cc_file_prefix(void) FRONTEND;
void cc_file_transfer(uint8_t writing,uint8_t newline) FRONTEND;
void cc_formatted_value(void) FRONTEND;
void cc_file_finish(void) FRONTEND;
void cc_file_stream_finish(void) FRONTEND;
#endif
