#ifndef ZP_LIBRARY_H
#define ZP_LIBRARY_H
#include "zp.h"
#define CODE_BANK_LIMIT 19
enum { CODE_IDE=0,CODE_STATEMENTS,CODE_SHARED,CODE_FRONTEND,CODE_ROUTINES,
       CODE_EXPRESSIONS,CODE_BUILD,CODE_RUNTIME,CODE_METADATA,CODE_FILE_COMPILER,
       CODE_FILE_RUNTIME,CODE_DEVICES,CODE_EDITOR,CODE_COMPILE,CODE_TRANSFER,CODE_UNITS,CODE_UNIT_IO,CODE_UNIT_META,CODE_LINKER };
/* BANK5 contains the phase loader itself and must remain loaded. The parser
 * expression/interrupt and file/I/O generators have no editor entry points. */
#ifdef ZP_OPTIONAL_REAL
#define CODE_DEFERRED(bank) ((bank)==CODE_EXPRESSIONS||(bank)==CODE_FILE_COMPILER||(bank)==CODE_RUNTIME||(bank)>=CODE_COMPILE)
uint8_t code_phase_real(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_real_release(void) SOURCE_BLOCK_CALL;
#else
#define CODE_DEFERRED(bank) ((bank)==CODE_EXPRESSIONS||(bank)==CODE_FILE_COMPILER||(bank)>=CODE_COMPILE)
#endif
#ifdef ZP_UNITS_EXPERIMENT
uint8_t code_phase_unit_link_begin(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_finish_begin(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_finish_end(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_link_end(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_io(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_meta(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_meta_release(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_unit_io_release(void) SOURCE_BLOCK_CALL;
#endif
extern char code_library_directory[128];
#define DEFAULT_LIBRARY "T:/lib/zpascal.zpl"
extern uint8_t code_pages[CODE_BANK_LIMIT],code_owned,boot_error;
extern uint16_t code_sizes[CODE_BANK_LIMIT];
extern const uint8_t code_required,code_version[4];
uint8_t get_bank(void) NATIVE;
void code_restore(uint8_t page) NATIVE;
uint8_t code_load(const char *path);
void code_release(void) NATIVE;
/* The version-checked loader lends its open original handle to the phase cache.
 * No phase page is executable after a failed reload. Child completion is
 * independent of a cache reload failure; the next compile retries it. */
extern int8_t code_phase_fd;
extern uint8_t code_phase_pending[CODE_BANK_LIMIT];
uint8_t code_read_exact(int8_t fd,uint16_t count);
uint8_t code_read_bank(int8_t fd,uint8_t page,uint16_t size);
void code_phase_bind(int8_t fd) SOURCE_BLOCK_CALL;
uint8_t code_phase_park(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_restore(void) SOURCE_BLOCK_CALL;
/* Restore only the warm set captured by the last synchronous park. */
void code_phase_return(void) SOURCE_BLOCK_CALL;
uint8_t code_phase_cleanup(void);
#endif
