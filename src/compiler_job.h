#ifndef ZP_COMPILER_JOB_H
#define ZP_COMPILER_JOB_H
#include "compiler_job_wire.h"
/* Parent-facing presentation copied only from a validated wire reply. */
typedef struct {Diagnostic diagnostic;char path[128];uint8_t hash[32],has_hash,interrupted;} CompileFailure;
uint8_t compile_request(const Source *root,const Sources *buffers,Build *build,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options,const char *work_path);
uint8_t job_map_source(const Build *build,uint16_t at,Source *source,uint8_t hash[32]) SOURCE_BLOCK_CALL;
uint8_t job_validate_success(const uint8_t *job,const uint8_t *context,Build *build,uint16_t *files) SOURCE_BLOCK_CALL;
uint8_t compiler_pages_release(uint8_t owned[8],const uint8_t *keep) SOURCE_BLOCK_CALL;
uint8_t compiler_resources_ready(void) SOURCE_BLOCK_CALL;
extern uint8_t compiler_owned[8],compiler_allocating;
extern uint8_t compiler_parent_owned[8];
enum { COMPILER_ALLOC_NONE,COMPILER_ALLOC_WORKER,COMPILER_ALLOC_PARENT };
extern uint8_t *compiler_job_active;
extern uint8_t compiler_job_memory;
void compiler_job_failure(void) SOURCE_BLOCK_CALL;
#endif
