#ifndef ZP_LIBRARY_H
#define ZP_LIBRARY_H
#include "zp.h"
#define CODE_BANK_LIMIT 16
#define CODE_PHASE_FIRST 13u /* Zero-based: banks14..16 are compiler phases. */
#define DEFAULT_LIBRARY "T:/lib/zpascal.zpl"
extern uint8_t code_pages[CODE_BANK_LIMIT],code_owned,boot_error;
extern uint16_t code_sizes[CODE_BANK_LIMIT];
extern const uint8_t code_required,code_version[4];
uint8_t code_load(const char *path);
void code_release(void) NATIVE;
/* The version-checked loader lends its open original handle to the phase cache.
 * No phase page is executable after a failed reload. Child completion is
 * independent of a cache reload failure; the next compile retries it. */
extern int8_t code_phase_fd;
extern uint8_t code_phase_pending[CODE_BANK_LIMIT-CODE_PHASE_FIRST];
uint8_t code_read_exact(int8_t fd,uint16_t count);
void code_phase_bind(int8_t fd) SOURCE_BLOCK_CALL;
uint8_t code_phase_park(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_restore(void) SOURCE_BLOCK_CALL;
void code_phase_return(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_cleanup(void);
#endif
