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
 * return buffer. File associations live in the176-byte Pascal file block. */
enum {
    ZF_ASSIGN, ZF_RESET, ZF_REWRITE, ZF_CLOSE, ZF_READ, ZF_WRITE,
    ZF_SEEK, ZF_POSITION, ZF_SIZE, ZF_EOF, ZF_FLUSH, ZF_ERASE,
    ZF_BLOCKREAD, ZF_BLOCKREAD_PARTIAL, ZF_BLOCKWRITE, ZF_BLOCKWRITE_PARTIAL
};
enum { ZF_END_PROGRAM=19,ZF_END_EXECUTE=32 };
enum { ZF_TEXT_READ=20,ZF_TEXT_WRITE,ZF_TEXT_READLN,ZF_TEXT_WRITELN,
       ZF_EOLN,ZF_SEEKEOF,ZF_SEEKEOLN,ZF_RENAME,ZF_TEXT_BEGIN,ZF_STREAM_RETURN,ZF_TRANSFER_PATH,ZF_TRANSFER_EXECUTE=33 };
enum { ZF_STANDARD_INPUT=1,ZF_STANDARD_OUTPUT,ZF_STANDARD_KBD,ZF_STANDARD_CON,ZF_STANDARD_TRM,
       ZF_STANDARD_LST,ZF_STANDARD_AUX,ZF_STANDARD_USR };
enum { ZF_OUTPUT_OVERRIDES=0x4000,ZF_OUTPUT_NATIVE=0x8000,
       ZF_OUTPUT_LST=1,ZF_OUTPUT_AUX=2 };
enum { ZF_JOB_BYTES=38 };
/* Text input kinds share the existing generated input/format family numbers.
 * The high byte of count is String capacity. Numeric input uses this private
 * caller-owned context, parsed by the existing runtime Val after prefix return. */
enum { ZF_TEXT_INTEGER=0,ZF_TEXT_STRING=1,ZF_TEXT_CHAR=3,ZF_TEXT_BYTE=4,ZF_TEXT_REAL=5 };
enum { ZF_NUMBER_TARGET=0,ZF_NUMBER_CODE=2,ZF_NUMBER_VALUE=4,
       ZF_NUMBER_TOKEN=10,ZF_NUMBER_BYTES=41 };
typedef struct {
    uint16_t operation,object,descriptor,data,count,actual;
    uint16_t reserved,heap_floor,heap_frontier,static_end;
    uint16_t result,error,detail,pc,stack_pointer,code_origin;
    /* Immutable compiler table {count:u16, {object:u16,identity:u16}...}.
     * stream is a return action, executed only after restoring the prefix. */
    uint16_t standard_table,stream;
    /* Per-binding logical lookahead: low byte character, high byte ready (0/1).
     * Returned to the inline file block after prefix-side input, including
     * an unchecked input failure. This is data, never descriptor authority. */
    uint16_t lookahead;
} FileJob;
/* Terminal and Execute cleanup release native descriptors only. They never
 * flush Pascal files, inspect dead variables or retire source/heap lifetimes. */
typedef struct { uint16_t operation,error,detail; } FileCleanup;
void zf_dispatch(FileJob *job) ZFILE_CALL;
/* No auxiliary file-storage pages are allocated. */
void zf_cleanup(FileCleanup *job) ZFILE_CALL;

/* Only native descriptor operations cross the retained assembly gate. */
#define ZF_FILE_BYTES 176u
extern uint16_t zf_service_value;
uint8_t zf_service(uint8_t operation,uint16_t argument) ZFILE_CALL;
#endif
