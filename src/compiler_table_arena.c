#include "compiler_table_arena.h"
#if defined(__SDCC)
#pragma codeseg BANK9
#pragma constseg BANK9
#endif

CompilerTableArena cc_table_arena;

/* Build disposal also runs after failed phase restoration. Keep the release
 * implementation in its own permanent-bank translation unit: SDCC codeseg
 * changes inside one file do not reliably move the emitted function body. */
uint8_t cc_tables_end(void) FRONTEND {
    uint8_t p;
    while(cc_table_arena.page_count) {
        p=cc_table_arena.pages[cc_table_arena.page_count-1];
        if(page_lifetime(p)==cc_table_arena.lives[cc_table_arena.page_count-1]&&!page_drop(p))return 0;
        --cc_table_arena.page_count;
    }
    cc_table_arena.segment_count=0;cc_table_arena.arena_length=0;cc_table_arena.started=0;return 1;
}
