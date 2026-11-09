#ifndef ZP_WORKER_H
#define ZP_WORKER_H
#include "common/zp.h"

/* Private RAM service, called only while a program job is active. The gate
 * uses the existing parent owner; no returned list grants allocation rights.
 * Caller reserves64 stack bytes below FC00. A=operation, DE=argument;
 * returns native error in A and value in DE, preserving BC/HL, IX/IY,
 * alternate registers and IFF. OPEN takes {flags:u8,path:u8[128]} in owned
 * child memory; the whole129-byte record must fit below FC00. */
#define WORKER_CONTROL 0xfd50u
#define WORKER_GATE 0xfda0u
enum { WORKER_ALLOC, WORKER_FREE, WORKER_OPEN, WORKER_CLOSE };
enum { WORKER_START, WORKER_FINISH, WORKER_CANCEL };
extern uint8_t worker_child_page,worker_child_prefix,worker_child_page2;
extern uint16_t worker_value;
void ram_worker_service(void) NATIVE;
uint8_t worker_control(uint8_t mode);
uint8_t worker_dispatch(uint8_t operation,uint16_t argument) NATIVE;
#endif
