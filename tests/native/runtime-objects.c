/* Linker oracle only: initialized/uninitialized data, C calls and address
 * arithmetic must survive independent placements without target execution. */
#include <stdint.h>
uint8_t runtime_workspace[257];
uint16_t runtime_seed = 0x1234;
const uint8_t runtime_values[4] = { 17, 42, 128, 255 };
uint16_t runtime_sum(uint8_t index) {
    return (uint16_t)(uintptr_t)(runtime_workspace + index) + runtime_values[index & 3];
}
uint16_t runtime_probe(uint8_t index) {
    uint8_t *p = runtime_workspace + index;
    *p = runtime_values[index & 3];
    return runtime_sum(index) + (uint16_t)(uintptr_t)p + runtime_seed;
}
