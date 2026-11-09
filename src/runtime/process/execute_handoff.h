#ifndef ZP_EXECUTE_HANDOFF_H
#define ZP_EXECUTE_HANDOFF_H
#include <stdint.h>
/* Saved-program transition context, never RAM worker context or Pascal data.
 * The OS's valid native image extent ends before F400, leaving this intact.
 * Byte XOR of all72 bytes is zero. Unused page slots participate in the XOR
 * but are not owners. Page0 is the retained module, then all other old owners.
 * Startup excludes all mapped pages before a checked module-page retirement:
 * an unowned candidate is stale cold-start state. An owned candidate belongs
 * to the ending program. The full record/page plan is checked before any other
 * release. Startup never executes retained old code. */
#define EXECUTE_HANDOFF_ADDRESS 0xfc00u
#define EXECUTE_HANDOFF_BYTES 72u
#define EXECUTE_HANDOFF_COUNT 4u
#define EXECUTE_HANDOFF_ORIGINALS 5u
#define EXECUTE_HANDOFF_LENGTH 8u
#define EXECUTE_HANDOFF_ID 10u
#define EXECUTE_HANDOFF_PAGES 42u
#define EXECUTE_HANDOFF_MAX_PAGES 29u
extern const uint8_t execute_entry_start[];
extern const uint16_t execute_entry_length;
#endif
