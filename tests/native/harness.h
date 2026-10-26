#ifndef ZP_TEST_HARNESS_H
#define ZP_TEST_HARNESS_H
#include "library.h"
#include <zos_vfs.h>
#include <zos_sys.h>
extern volatile uint16_t test_passed,test_failed,test_stage,test_failures[64],test_index;
extern volatile uint8_t test_done,test_hash[32];
extern Source s;
extern Build b;
extern Diagnostic d;
extern uint8_t console_scratch[256];
void test_machine(uint8_t *state) NATIVE;
void check(uint8_t ok);
void code_hash(uint8_t *out);
uint8_t load(const char *text);
uint8_t seed(const char *path,const char *text);
void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position);
void execution(const char *text,uint8_t kind,uint8_t code);
void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position);
void checked_error(const char *text,uint8_t code,const char *where);
#endif
