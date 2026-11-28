#ifndef ZP_STORAGE_NAMES_H
#define ZP_STORAGE_NAMES_H
#include "common/zp.h"
uint8_t build_path(const Source *source,char *out,uint8_t chain) SOURCE_BLOCK_CALL;
uint8_t same_file_name(const char *a,const char *b) SOURCE_BLOCK_CALL;
uint8_t output_conflicts(const Source *source,const Build *build,const char *path) SOURCE_BLOCK_CALL;
#endif
