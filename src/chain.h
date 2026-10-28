#ifndef ZP_CHAIN_H
#define ZP_CHAIN_H
#include <stdint.h>
#ifdef __SDCC
#define CHAIN_CALL __sdcccall(1)
#else
#define CHAIN_CALL
#endif
/* Private loader state belongs to its retained module, outside replaced code.
 * A caller supplies validated runtime extents before lending the prefix bank. */
typedef struct {
    uint16_t origin,bytes,globals,end,heap_floor,heap_frontier,stack_pointer;
} ChainCaller;
typedef struct {
    uint8_t pages[2],ready;
    int8_t fd;
    uint16_t origin,bytes,globals,end,floor,frontier,trim_at,trim_value,error;
    uint8_t detail,id[32],map[32];
} ChainLoad;
void chain_init(ChainLoad *load);
/* Complete file authentication and heap validation, with no caller writes.
 * A failed cleanup retains its page ledger for a later discard/retry. */
uint8_t chain_prepare(ChainLoad *load,const char *path,const ChainCaller *caller);
uint8_t chain_discard(ChainLoad *load);
/* Internal commit: only after switching to a retained loader and a safe stack.
 * Installs payload/one trimmed free header; runtime control/header activation
 * belongs to the nonreturning handoff. Does not release its staging pages. */
void chain_install(ChainLoad *load);
/* Retained module services. A page copy maps8000 only for the bounded copy,
 * restoring the caller mapping before C resumes or any SDK call occurs. */
extern uint8_t chain_buffer[128],chain_store;
void chain_page_io(uint8_t page,uint16_t offset,uint16_t bytes) CHAIN_CALL;
uint16_t chain_user_word(uint16_t address);
void chain_user_store(uint16_t address,uint16_t value);
void chain_user_copy(uint16_t address,uint16_t bytes);
#endif
