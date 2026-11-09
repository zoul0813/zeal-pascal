#ifndef ZP_TEST_HARNESS_H
#define ZP_TEST_HARNESS_H
#include "platform/banks/loader.h"
#include <zos_vfs.h>
#include <zos_sys.h>
extern volatile uint16_t test_passed,test_failed,test_stage,test_failures[64],test_index;
extern volatile uint8_t test_done;
extern Source s;
extern Build b;
extern Diagnostic d;
extern uint8_t console_scratch[256];
void test_machine(uint8_t *state) NATIVE;
void check(uint8_t ok);
uint8_t code_capture(uint8_t after) SOURCE_BLOCK_CALL;
uint8_t load(const char *text);
uint8_t seed(const char *path,const char *text);
void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position) SOURCE_BLOCK_CALL;
void execution(const char *text,uint8_t kind,uint8_t code);
void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position);
void checked_error(const char *text,uint8_t code,const char *where);
#ifdef ZP_SCREEN_LIBRARY
void checked_unit_error(const char *text,uint8_t code,uint16_t position);
#endif
uint8_t cached_code_pages(void) SOURCE_BLOCK_CALL;
#endif
