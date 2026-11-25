#include "harness.h"
/* Resident: load_source captures a path in the caller bank before switching. */
void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position) {
    uint8_t ok=load_source(&s,path,&d);
    check(ok); if(ok) execute_loaded(kind,code,expected_position);
}
