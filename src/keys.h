#ifndef ZP_KEYS_H
#define ZP_KEYS_H
#include <stdint.h>
#ifdef __SDCC
#define KEYCALL __sdcccall(1)
#else
#define KEYCALL
#endif
/* Stateful raw PS/2 conversion. Zero means no press; printable/control bytes
 * and special-key codes are returned to the consumer. The Pascal runtime maps
 * physical Enter/Escape/Delete to CR/ESC/DEL; Ctrl-J remains LF. Each linked
 * image owns its own modifiers and release state. */
uint8_t key_event(uint8_t raw) KEYCALL;
#endif
