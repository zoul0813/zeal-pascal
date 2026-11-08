#ifndef ZP_COMPILER_LAYOUT_H
#define ZP_COMPILER_LAYOUT_H
#include "compiler/internal.h"
uint8_t cc_layout_active(void) FRONTEND;
uint16_t cc_data_base(void) FRONTEND;
uint16_t cc_required_stack(void) FRONTEND;
uint16_t cc_runtime_address(uint16_t address) FRONTEND;
void cc_data_op(uint8_t opcode,uint16_t address) FRONTEND;
void cc_runtime_op(uint8_t opcode,uint16_t address) FRONTEND;
void cc_runtime_copy_operand(uint16_t destination,uint16_t original) FRONTEND;
void cc_runtime_copy_reference(uint16_t site,uint16_t address) FRONTEND;
uint8_t cc_layout_finish(void) ROUTINES;
#endif
