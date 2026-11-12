#ifndef ZP_NATIVE_OVERLAYS_H
#define ZP_NATIVE_OVERLAYS_H
#include "compiler/internal.h"
#ifdef ZP_OVERLAYS
#define COMPILE_OVERLAY_RETRY 244u
uint8_t cc_overlays_begin(void) ROUTINES;
void cc_overlays_close(const Build *build) SOURCE_BLOCK_CALL;
extern uint8_t overlay_build_owned;
void cc_overlays_adopt(const Build *from,Build *to) SOURCE_BLOCK_CALL;
uint8_t cc_overlay_image_byte(const Build *build,uint16_t offset) FRONTEND;
uint16_t cc_overlay_origin(void) FRONTEND;
uint8_t cc_overlay_group(void) ROUTINES;
uint16_t cc_overlay_member(uint8_t group,uint16_t scope) ROUTINES;
uint8_t cc_overlay_switch(uint16_t bank) ROUTINES;
void cc_overlay_alias(uint16_t scope) ROUTINES;
void cc_overlay_wrappers(uint16_t bank) ROUTINES;
void cc_overlay_finish_member(uint16_t bank) ROUTINES;
uint8_t cc_overlay_finish(void) ROUTINES;
uint8_t cc_overlay_place(uint16_t origin,uint16_t data) ROUTINES;
uint8_t cc_overlay_publish(const Build *build,const char *path) SOURCE_BLOCK_CALL;
uint8_t cc_overlay_locate(const Build *build,uint16_t pc,uint8_t *file,uint16_t *position) SOURCE_BLOCK_CALL;
void cc_overlay_unit_call(uint8_t opcode,uint16_t scope) FRONTEND;
void cc_overlay_address(uint8_t opcode,uint16_t address) FRONTEND;
void cc_overlay_ovrdrive(void) FRONTEND;
void cc_overlay_put(uint16_t at,uint8_t value) FRONTEND;
uint8_t cc_overlay_get(uint16_t at) FRONTEND;
void cc_overlay_pc(uint16_t at) FRONTEND;
uint16_t cc_overlay_reset(uint16_t entry) FRONTEND;
void cc_overlay_chain(uint16_t operand,uint8_t root) FRONTEND;
uint8_t cc_overlay_record_read(uint32_t at,uint8_t *out,uint16_t size) FRONTEND;
void cc_overlay_record_put(uint32_t at,uint8_t value) FRONTEND;
#endif
#endif
