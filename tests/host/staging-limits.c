#include "compiler/internal.h"
#include <assert.h>
#include <stdio.h>
uint16_t cc_required_stack(void) {return 2048;}
int main(void) {
    CompileLimits limits; CompileOptions options;
    /* A compact image can fit code beyond the old 29 KiB staging ceiling. */
    assert(compile_limits_at(0x5103,960,30000,0,&limits));
    assert(limits.minimum_start==0x54c3);
    assert(compile_limits_at(0x5103,960,32767,0,&limits));
    /* A full prefix cannot fit that same stream plus its physical stack. */
    assert(!compile_limits_at(0x8000,0,32767,0,&limits));
    assert(!compile_limits_at(0x5103,0xa000,30000,0,&limits));
    memset(&options,0,sizeof(options));options.start=0xf000;options.end=0xfbff;
    assert(compile_limits_at(0x5103,960,1000,&options,&limits));
    assert(!compile_limits_at(0x5103,960,1100,&options,&limits));
    options.start=0x5000;
    assert(!compile_limits_at(0x5103,960,30000,&options,&limits));
    options.start=0x54c3;options.end=0xffff;
    assert(!compile_limits_at(0x5103,960,30000,&options,&limits));
    puts("9 physical placement assertions passed");return 0;
}
