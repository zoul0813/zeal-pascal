#ifndef ZP_LIBRARY_H
#define ZP_LIBRARY_H
#include "zp.h"
#define CODE_BANK_LIMIT 16
#define DEFAULT_LIBRARY "T:/lib/zpascal.zpl"
extern uint8_t code_pages[CODE_BANK_LIMIT],code_owned,boot_error;
extern uint16_t code_sizes[CODE_BANK_LIMIT];
extern const uint8_t code_required,code_digest[32];
uint8_t code_load(const char *path);
void code_release(void) NATIVE;
#endif
