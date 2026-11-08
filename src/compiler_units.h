#ifndef ZP_COMPILER_UNITS_H
#define ZP_COMPILER_UNITS_H
#include "compiler.h"
#include "compiler_fragments.h"

/* Experimental first end-to-end unit path. Nothing here is linked by the
 * legacy/compact targets. Artifacts bind the exact normal runtime profile;
 * public interfaces carry routines, constants and canonical type metadata.
 * A caller owns the fresh output descriptor, including close/publication. */
enum { UNIT_FORMAT=248,UNIT_UNSUPPORTED,UNIT_MISSING,UNIT_STALE,UNIT_AMBIGUOUS,UNIT_INTERFACE };
enum { UR_GLOBAL=1,UR_PC,UR_FRAGMENT,UR_EXPORT,UR_MODULE,UR_SEAL,UR_IMPORT,UR_CODE,UR_STATE,UR_EXTERNAL,UR_TYPE,UR_INITIALIZER,UR_INIT_CALL,UR_ARTIFACT,UR_FILE,UR_HASH,UR_PATH,UR_RUNTIME,UR_NATIVE };
/* Every imported module has a stream record and eight hash words after its
 * seal. Three hash words fit in UR_FILE; five follow the UR_HASH tag. */
#define UNIT_FILE_AT(module) ((module)+2u*UNIT_RECORD_BYTES)
#define UNIT_HASH_AT(module,word) (UNIT_FILE_AT(module)+6u+2u*(word)+((word)>=3u?2u:0u))
#define cc_unit_prefix(module) cc_unit_word(UNIT_FILE_AT(module)+4u)
uint16_t cc_unit_path_store(const char *path) SOURCE_BLOCK_CALL;
int8_t cc_unit_path_open(uint16_t module) SOURCE_BLOCK_CALL;
uint8_t cc_unit_file_bind(int8_t fd) SOURCE_BLOCK_CALL;
int8_t cc_unit_file(uint8_t slot) SOURCE_BLOCK_CALL;
uint8_t cc_unit_files_close(void) SOURCE_BLOCK_CALL;
uint8_t cc_unit_validate_object(uint16_t module,const char *name,uint32_t stream_hash) ROUTINES;
uint8_t cc_unit_emit_page(uint16_t module,uint8_t index,const CompilerFragmentLayout *layout) ROUTINES;
#define RF_UNIT_IMPORT 16u
#define UNIT_RECORD_BYTES 12u
#define UNIT_RECORD_BASE 256u
#define UH_USED 0u
#define UH_RECORDS 2u
#define UH_MODULES 4u
#define UH_NAME_LENGTH 6u
#define UH_REFERENCES 8u
#define UH_REFERENCE_TAIL 10u
#define UH_REFERENCE_COUNT 12u
#define UH_FRAGMENTS 14u
#define UH_FRAGMENT_TAIL 16u
#define UH_NAME 32u
#define ZPU_HEADER_BYTES 48u
#define ZPU_ARTIFACT_LIMIT (SOURCE_LIMIT-2u*CC_FRAGMENT_OBJECTS)
#define UNIT_CYCLE 247u
#define UM_VISIBLE 1u
#define UM_LOADING 2u
#define UM_READY 4u
#define UM_TYPES 8u
#define UM_INITIALIZED 16u
/* Unreleased format: evolve this contract directly until the ABI1 release. */
#define ZPU_ABI 1u
#define ZPU_REAL 1u
#define ZPU_FULL_MEMORY 2u
#define ZPU_HARDWARE 4u /* Raw memory/I/O: retain conservative console binding. */
#define ZPU_CALLER_IO 1u /* Export flag: pass caller {$I} in A. */
int8_t cc_units_output(void) ROUTINES;
uint32_t unit_source_fingerprint(const Source *source) SOURCE_BLOCK_CALL;
uint8_t cc_units_hold(uint8_t page,uint32_t lifetime,uint8_t role) ROUTINES;
uint16_t cc_unit_owned_page(void) ROUTINES;
uint16_t cc_unit_artifact_page(void) ROUTINES;
uint8_t cc_unit_module_ready(uint16_t module) ROUTINES;
uint8_t cc_unit_module_byte(uint16_t module,uint16_t at) ROUTINES;
uint16_t cc_unit_module_word(uint16_t module,uint16_t at) ROUTINES;
void cc_unit_descriptor(uint16_t module,CompilerFragmentObject *object) ROUTINES;
uint8_t cc_unit_fragment(uint16_t preferred,uint16_t *fragment,uint16_t *offset) ROUTINES;
uint8_t compile_unit(const Source *source,Build *scratch,Diagnostic *diagnostic,int8_t output,uint8_t interruptible);
uint16_t cc_units_load(const char *name,const char *path) ROUTINES;
uint16_t cc_units_find(const char *name) ROUTINES;
uint16_t cc_units_state(uint16_t module) ROUTINES;
uint16_t cc_units_graph(const char *name,uint8_t visible) ROUTINES;
uint16_t cc_unit_export(uint16_t module,const char *name) ROUTINES;
uint16_t cc_units_graph_load(const char *name,uint8_t visible) ROUTINES;
uint8_t cc_units_graph_validate(void) ROUTINES;
uint16_t cc_unit_target(uint16_t module,uint16_t identity) ROUTINES;
uint8_t cc_units_requested(void) ROUTINES;
uint8_t cc_units_begin(void) ROUTINES;
uint8_t cc_units_close(void) ROUTINES;
void cc_units_handoff_cleanup(void) ROUTINES;
void cc_units_header(void) ROUTINES;
void cc_units_interface(void) ROUTINES;
void cc_units_imports(void) ROUTINES;
void cc_units_initializers(void) ROUTINES;
int16_t cc_units_lookup(void) ROUTINES;
void cc_units_call(uint8_t opcode,uint16_t scope) ROUTINES;
void cc_units_note(uint16_t kind,uint16_t site,uint16_t value) ROUTINES;
void cc_units_owner(uint16_t owner) ROUTINES;
void cc_units_export(uint16_t symbol,uint16_t scope) ROUTINES;
void cc_units_global(uint8_t opcode,uint16_t address) ROUTINES;
uint8_t cc_units_write(void) ROUTINES;
typedef struct {uint16_t next,relocation,record,site,value;uint8_t kind;} UnitReference;
uint8_t cc_unit_relocations_prepare(void) ROUTINES;
void cc_unit_reference_begin(UnitReference *cursor) ROUTINES;
void cc_unit_reference_next(UnitReference *cursor) ROUTINES;
uint8_t cc_units_write_object(void) ROUTINES;
uint16_t cc_units_load_object(const char *name,const char *path,const char *fallback) ROUTINES;
uint8_t cc_units_link(void) ROUTINES;
uint8_t cc_units_link_object(void) ROUTINES;
uint32_t cc_unit_types_bytes(void) ROUTINES;
void cc_unit_types_write(uint8_t page,uint16_t *at) ROUTINES;
uint8_t cc_unit_types_ready(uint16_t module) ROUTINES;
uint8_t cc_units_types_ready(uint16_t module) ROUTINES;
uint32_t cc_unit_exports_bytes(void) ROUTINES;
void cc_unit_exports_write(uint8_t page,uint16_t *at) ROUTINES;
int16_t cc_unit_import_symbol(uint16_t module,uint16_t export) ROUTINES;
uint16_t cc_unit_record(uint16_t kind,uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint16_t e) ROUTINES;
uint16_t cc_unit_word(uint16_t at) ROUTINES;
void cc_unit_put(uint16_t at,uint16_t value) ROUTINES;
uint8_t cc_unit_byte(uint16_t at) ROUTINES;
void cc_unit_set(uint16_t at,uint8_t value) ROUTINES;
uint8_t cc_file_native_bind(const CompilerFragmentLayout *layout,uint16_t end) ROUTINES;
#endif
