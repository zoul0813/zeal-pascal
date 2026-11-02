#ifndef ZP_CHAIN_H
#define ZP_CHAIN_H
#include <stdint.h>
#include "chain_abi.h"
#ifdef __SDCC
#define CHAIN_CALL __sdcccall(1)
#else
#define CHAIN_CALL
#endif
/* Private loader state belongs to its retained module, outside replaced code.
 * A caller supplies validated runtime extents before lending the prefix bank. */
typedef struct {
    uint16_t origin,bytes,globals,end,heap_floor,heap_frontier,stack_pointer;
    uint8_t profile[CHAIN_PROFILE_BYTES];
} ChainCaller;
#ifdef ZP_CHAIN_ONLY
#define IMAGE_LOAD_PAGES 2
#else
#define IMAGE_LOAD_PAGES 3
enum { IMAGE_EXECUTABLE,IMAGE_CHAIN,IMAGE_ROLLBACK };
#endif
/* One ownership ledger serves version-checked CHN payloads and ordinary native
 * executable images. ZP_CHAIN_ONLY is a component-test variant; the product handles both. */
typedef struct {
    uint8_t pages[IMAGE_LOAD_PAGES],ready;
    int8_t fd;
    uint16_t origin,bytes,globals,end,floor,frontier,trim_at,trim_value,error;
    uint8_t detail;
#ifndef ZP_CHAIN_ONLY
    uint8_t kind;
    uint16_t length;
    uint16_t entry;
    uint8_t handoff;
    uint8_t originals[3]; /* borrowed caller pages, never freed by this ledger */
#endif
} ImageLoad;
void chain_init(ImageLoad *load);
/* Complete file extent and heap validation, with no caller writes.
 * A failed cleanup retains its page ledger for a later discard/retry. */
uint8_t chain_prepare(ImageLoad *load,const char *path,const ChainCaller *caller);
#ifndef ZP_CHAIN_ONLY
/* Fully stage an ordinary native executable without changing caller RAM.
 * A valid matching Pascal startup may retire old pages early; all other
 * bytes use the OS executable contract without private header requirements.
 * This validates/stages only; OS override and rollback are separate owners. */
uint8_t execute_prepare(ImageLoad *load,const char *path);
/* After validation, reuse staging pages for a complete caller rollback copy.
 * Both calls must run in retained code on an isolated stack: no original or
 * backup page may contain the current code or stack. Capture consumes the
 * staged target bytes; the OS subsequently loads the checked path itself.
 * Failed capture validation/allocation leaves the staged image untouched.
 * Restore consumes readiness only on success and preserves the OS error. */
uint8_t execute_snapshot(ImageLoad *load,const uint8_t originals[3]);
uint8_t execute_restore(ImageLoad *load);
uint8_t execute_code_page(void) CHAIN_CALL;
uint8_t execute_stack_page(void) CHAIN_CALL;
/* Internal saved-program override. The retained prefix owns load/code/data,
 * including at least1024 bytes below its aliased FC00 stack top. The supplied
 * originals are the OS program's three allocation pages, not the retained
 * mappings. Success never returns. Failure restores the complete caller and
 * returns zero with load->error/detail, retaining backup owners for discard.
 * Additional owners are copied into the retained handoff, never freed on
 * failed override. Their complete nonaliasing page plan is checked before
 * snapshot. Caller input/file cleanup remains a separate owner. */
uint8_t execute_override(ImageLoad *load,const char *path,const uint8_t originals[3],
                         const uint8_t *owners,uint8_t owner_count);
void execute_switch(void);
void execute_attempt(void);
uint8_t execute_os_override(const char *path) CHAIN_CALL;
void execute_abandon(void);
#endif
uint8_t chain_discard(ImageLoad *load);
/* Internal commit: only after switching to a retained loader and a safe stack.
 * Installs payload/one trimmed free header; runtime control/header activation
 * belongs to the nonreturning handoff. Does not release its staging pages. */
void chain_install(ImageLoad *load);
/* Retained module services. A page copy maps8000 only for the bounded copy,
 * restoring the caller mapping before C resumes or any SDK call occurs. */
extern uint8_t chain_buffer[128],chain_store;
void chain_page_io(uint8_t page,uint16_t offset,uint16_t bytes) CHAIN_CALL;
uint16_t chain_user_word(uint16_t address);
void chain_user_store(uint16_t address,uint16_t value);
void chain_user_copy(uint16_t address,uint16_t bytes);
#endif
