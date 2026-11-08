#ifndef ZP_UNIT_OUTPUT_H
#define ZP_UNIT_OUTPUT_H
#include "compiler/internal.h"

/* The unit request owns the returned descriptor. This module owns only the
 * reserved scratch pathname until discard succeeds, including after a retry. */
int8_t unit_output_begin(const Source *root) ROUTINES;
uint8_t unit_output_publish(const Source *root,char *published) ROUTINES;
uint8_t unit_output_discard(void) ROUTINES;
#endif
