#ifndef ZP_REAL_H
#define ZP_REAL_H
#include <stdint.h>
#if defined(__SDCC) && !defined(ZP_REAL_RUNTIME)
#define REAL_API __banked
#else
#define REAL_API
#endif
/* Private synchronous arithmetic context. Every operation snapshots its inputs;
 * the compiler/runtime copies the result before another operation can begin.
 * No syscall, user routine or callback executes within the arithmetic engine. */
enum { ZR_ADD,ZR_SUB,ZR_MUL,ZR_DIV,ZR_COMPARE,ZR_FROM_INT,ZR_TRUNC,ZR_ROUND,
       ZR_NEGATE,ZR_ABS,ZR_INT,ZR_FRAC,ZR_SQR,ZR_DECIMAL };
extern uint8_t zr_op,zr_error,zr_a[6],zr_b[6],zr_result[6];
extern int16_t zr_integer;
extern const uint8_t *zr_text;
extern uint16_t zr_length;
void zr_eval(void) REAL_API;
#endif
