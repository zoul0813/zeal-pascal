#ifndef ZP_FILE_RUNTIME_H
#define ZP_FILE_RUNTIME_H
#include <stdint.h>
#ifdef __SDCC
#define ZFILE_CALL __sdcccall(1)
#else
#define ZFILE_CALL
#endif

/* Private callable module at4000. All words are native little-endian u16;
 * operands/results belong to the invoking program, never a shared prefix
 * return buffer. The module owns its registry independently of Mark/Release. */
enum {
    ZF_ASSIGN, ZF_RESET, ZF_REWRITE, ZF_CLOSE, ZF_READ, ZF_WRITE,
    ZF_SEEK, ZF_POSITION, ZF_SIZE, ZF_EOF, ZF_FLUSH, ZF_ERASE,
    ZF_BLOCKREAD, ZF_BLOCKREAD_PARTIAL, ZF_BLOCKWRITE, ZF_BLOCKWRITE_PARTIAL
};
enum { ZF_END_FRAME=16, ZF_END_OBJECT, ZF_END_RANGE, ZF_END_PROGRAM };
enum { ZF_TEXT_READ=20,ZF_TEXT_WRITE,ZF_TEXT_READLN,ZF_TEXT_WRITELN,
       ZF_EOLN,ZF_SEEKEOF,ZF_SEEKEOLN,ZF_RENAME,ZF_TEXT_BEGIN };
enum { ZF_STANDARD_INPUT=1,ZF_STANDARD_OUTPUT };
enum { ZF_JOB_BYTES=36 };
/* Text input kinds share the existing generated input/format family numbers.
 * The high byte of count is String capacity. Numeric input uses this private
 * caller-owned context, parsed by the existing runtime Val after prefix return. */
enum { ZF_TEXT_INTEGER=0,ZF_TEXT_STRING=1,ZF_TEXT_CHAR=3,ZF_TEXT_BYTE=4,ZF_TEXT_REAL=5 };
enum { ZF_NUMBER_TARGET=0,ZF_NUMBER_CODE=2,ZF_NUMBER_VALUE=4,
       ZF_NUMBER_TOKEN=10,ZF_NUMBER_BYTES=41 };
typedef struct {
    uint16_t operation,object,descriptor,data,count,actual;
    uint16_t frame,heap_floor,heap_frontier,static_end;
    uint16_t result,error,detail,pc,stack_pointer,code_origin;
    /* Immutable compiler table {count:u16, {object:u16,identity:u16}...}.
     * stream is a return action, executed only after restoring the prefix. */
    uint16_t standard_table,stream;
} FileJob;
typedef struct { uint16_t operation,owner,end,error,detail; } FileCleanup;
void zf_dispatch(FileJob *job) ZFILE_CALL;
void zf_cleanup(FileCleanup *job) ZFILE_CALL;

/* Assembly services execute from the module's retained prefix. Registry
 * copies temporarily map their owned page at8000, restoring it before any
 * SDK call, parent service, or return to generated code. */
#define ZF_RECORD_BYTES 160u
extern uint8_t zf_record[ZF_RECORD_BYTES],zf_store;
extern uint16_t zf_service_value;
void zf_record_io(uint8_t page,uint16_t offset) ZFILE_CALL;
void zf_page_clear(uint8_t page) ZFILE_CALL;
uint8_t zf_service(uint8_t operation,uint16_t argument) ZFILE_CALL;
#endif
