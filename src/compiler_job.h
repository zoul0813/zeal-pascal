#ifndef ZP_COMPILER_JOB_H
#define ZP_COMPILER_JOB_H
#include "sources.h"
/* Diagnostic captured before synchronous compiler cleanup. */
#define COMPILER_UNIT_MODE 3u
typedef struct {Diagnostic diagnostic;char path[128];uint8_t interrupted;} CompileFailure;
uint8_t compile_request(const Source *root,Build *build,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options);
uint8_t compiler_result_ready(const Build *build) SOURCE_BLOCK_CALL;
uint8_t compiler_page_owned(const uint8_t mask[8],uint8_t page) SOURCE_BLOCK_CALL;
uint8_t compiler_pages_release(uint8_t owned[8],const uint8_t *keep) SOURCE_BLOCK_CALL;
uint8_t compiler_resources_ready(void) SOURCE_BLOCK_CALL;
extern uint8_t compiler_owned[8],compiler_allocating;
extern uint8_t compiler_parent_owned[8];
enum { COMPILER_ALLOC_NONE,COMPILER_ALLOC_WORKER,COMPILER_ALLOC_PARENT };
extern CompileFailure *compiler_invocation;
extern uint8_t compiler_memory_mode;
void compiler_capture_failure(void) SOURCE_BLOCK_CALL;
#endif
