#ifndef ZP_COMPILER_LATE_H
#define ZP_COMPILER_LATE_H
#include "compiler.h"

/* Deferred addresses keep the existing single preferred code space and
 * scalar relocation coefficient. This journal adds preferred symbol values;
 * ordinary relocation applies the final delta separately. No tagged addresses
 * or target-qualified relocation records enter the emitter. */
enum { LATE_ROOT,LATE_INITIALIZER,LATE_BRIDGE,LATE_ROUTINE };
enum { LATE_RANDOM,LATE_RANDOM_STATE,LATE_PARAMETERS,LATE_FILE,
       LATE_KEYBOARD,LATE_KEY_POLL,LATE_KEY_STATUS,LATE_KEY_STATE,
       LATE_KEY_PROBE,LATE_KEY_ENQUEUE,LATE_CONSOLE_PROBE,LATE_INTERRUPT,
       LATE_OUTPUT,LATE_TRANSFER,LATE_UNPACK,LATE_ROOT_COUNT };
#define LATE_NONE 65535u
#define LATE_REFERENCE_BYTES 8u
typedef struct {
    uint32_t lifetime,version;
    uint16_t symbols,references;
    uint8_t page,failed,sealed;
} CompilerLate;
void cc_late_init(CompilerLate *late) ROUTINES;
uint8_t cc_late_discard(CompilerLate *late) ROUTINES;
/* Owner0 is resident; other owners identify overlay member routine scopes.
 * Symbol indices stay stable until discard, even after member refs retire. */
uint16_t cc_late_symbol(CompilerLate *late,uint8_t kind,uint8_t id,uint8_t owner) ROUTINES;
uint8_t cc_late_bind(CompilerLate *late,uint16_t symbol,uint8_t owner,uint16_t value) ROUTINES;
uint8_t cc_late_value(CompilerLate *late,uint16_t symbol,uint16_t *value) ROUTINES;
/* The caller emits its constant addend and maintains the ordinary relocation
 * ledger. Multiple symbols at one operand add; byte terms are modulo256. */
uint8_t cc_late_reference(CompilerLate *late,uint16_t operand,uint16_t symbol,uint16_t coefficient,uint8_t byte) ROUTINES;
/* Pop a prepend chain's member suffix into the journal, zeroing only those
 * member operands. Invalid chains/capacity failures leave head/code/count
 * unchanged. The caller adopts parent_head only after success. */
uint8_t cc_late_chain(CompilerLate *late,uint16_t first,uint16_t head,uint16_t symbol,uint16_t *parent_head) ROUTINES;
uint8_t cc_late_range(CompilerLate *late,uint16_t first,uint16_t origin,uint16_t end) ROUTINES;
uint8_t cc_late_byte(CompilerLate *late,uint16_t reference,uint8_t offset) ROUTINES;
/* Called only after verified spool commit; never after a partial append. */
uint8_t cc_late_retire(CompilerLate *late,uint16_t first) ROUTINES;
/* Validate every remaining resident reference before patching any. Apply once,
 * after all deferred owners have emitted; symbol lookup remains available for
 * the immutable spooled members' linker after this sealing step. */
uint8_t cc_late_finish(CompilerLate *late) ROUTINES;
/* Root-owned factories defer their definition while an overlay member emits.
 * A known root address takes the ordinary emission path, unchanged. */
uint8_t cc_root_member(void) ROUTINES;
void cc_root_request(uint8_t kind,uint8_t id) ROUTINES;
uint8_t cc_root_pending(uint8_t kind,uint8_t id) ROUTINES;
void cc_root_bind(uint8_t kind,uint8_t id,uint16_t value) ROUTINES;
void cc_root_operand(uint8_t kind,uint8_t id,uint16_t value,uint16_t addend) ROUTINES;
void cc_root_address(uint8_t opcode,uint8_t kind,uint8_t id,uint16_t value,uint16_t addend) ROUTINES;
#endif
