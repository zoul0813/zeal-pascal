#ifndef ZP_BANK_MODULE_H
#define ZP_BANK_MODULE_H
#include "runtime/process/banks.h"
/* Dispatch and manager state live in their owned page1 bank. Only this job
 * and the borrowed immutable directory remain in page3 during dispatch. */
typedef struct {
    uint16_t reserved_context;
    const uint8_t *directory;
    const char *basename;
    uint16_t directory_bytes,operation,value;
    uint8_t page,changed,error,reserved;
} OverlayJob;
#endif
