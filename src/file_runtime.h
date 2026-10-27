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
typedef struct {
    uint16_t operation,object,descriptor,data,count,actual;
    uint16_t frame,heap_floor,heap_frontier,static_end;
    uint16_t result,error,detail,pc,stack_pointer,code_origin;
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
