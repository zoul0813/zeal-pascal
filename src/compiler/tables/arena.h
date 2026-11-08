#ifndef ZP_COMPILER_TABLE_ARENA_H
#define ZP_COMPILER_TABLE_ARENA_H
#include "compiler/internal.h"

/* Retained ownership is shared with cleanup in a permanent bank. No table
 * record or deferred-bank helper is needed to dispose of an abandoned build. */
typedef struct {
    uint8_t pages[32],page_count,segment_count,started;
    uint32_t lives[32],arena_length;
} CompilerTableArena;
extern CompilerTableArena cc_table_arena;
#endif
