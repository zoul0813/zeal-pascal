#ifndef ZP_COMPILER_DISK_H
#define ZP_COMPILER_DISK_H
#include "common/zp.h"
extern uint8_t compiler_disk_requested,compiler_disk_placed;
uint8_t cc_disk_begin(Build *b,const Source *root,uint8_t chain);
uint8_t cc_disk_active(const Build *b);
uint8_t cc_disk_ready(const Build *b);
uint8_t cc_disk_chain(const Build *b);
const char *cc_disk_path(void);
void cc_disk_place(Build *b,uint16_t origin,uint16_t bytes);
void cc_disk_seal(Build *b);
void cc_disk_adopt(const Build *from,Build *to);
void cc_disk_discard(const Build *b);
void cc_disk_final_put(uint16_t at,uint8_t value);
void cc_disk_global_put(uint16_t at,uint8_t value);
void cc_disk_put(uint16_t at,uint8_t value);
uint8_t cc_disk_get(uint16_t at);
uint8_t cc_disk_image(const Build *b,uint16_t at);
uint8_t cc_disk_prepare_publish(const Build *b);
uint8_t cc_disk_publish(const Build *b,const char *path) SOURCE_BLOCK_CALL;
#endif
