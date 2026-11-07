#ifndef ZP_COMPILER_UNSIGNED_INTERNAL_H
#define ZP_COMPILER_UNSIGNED_INTERNAL_H
#include "compiler.h"
uint8_t cc_unsigned_operand(TypeId type,uint8_t right,uint8_t sign) BACKEND;
void cc_unsigned_negative_literal(uint8_t negative) BACKEND;
uint8_t cc_unsigned_initializer(TypeId type,TypeId *actual,uint16_t *value,uint8_t emit) BACKEND;
#endif
