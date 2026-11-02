#ifndef ZP_COMPILER_DISK_H
#define ZP_COMPILER_DISK_H
#include "zp.h"
extern uint8_t compiler_disk_requested,compiler_disk_placed;
uint8_t cc_disk_begin(Build *b,const Source *root,uint8_t chain) SOURCE_BLOCK_CALL;
uint8_t cc_disk_active(const Build *b) SOURCE_BLOCK_CALL;
uint8_t cc_disk_ready(const Build *b) SOURCE_BLOCK_CALL;
uint8_t cc_disk_chain(const Build *b) SOURCE_BLOCK_CALL;
void cc_disk_place(Build *b,uint16_t origin,uint16_t bytes) SOURCE_BLOCK_CALL;
void cc_disk_seal(Build *b) SOURCE_BLOCK_CALL;
void cc_disk_adopt(const Build *from,Build *to) SOURCE_BLOCK_CALL;
void cc_disk_discard(const Build *b) SOURCE_BLOCK_CALL;
void cc_disk_final_put(uint16_t at,uint8_t value) SOURCE_BLOCK_CALL;
void cc_disk_global_put(uint16_t at,uint8_t value) SOURCE_BLOCK_CALL;
void cc_disk_put(uint16_t at,uint8_t value) SOURCE_BLOCK_CALL;
uint8_t cc_disk_get(uint16_t at) SOURCE_BLOCK_CALL;
uint8_t cc_disk_image(const Build *b,uint16_t at) SOURCE_BLOCK_CALL;
uint8_t cc_disk_publish(const Build *b,const char *path) SOURCE_BLOCK_CALL;
#endif
