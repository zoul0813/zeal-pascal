#ifndef ZP_PATH_H
#define ZP_PATH_H
#include <stdint.h>

/* Canonical native file identity, without filesystem access or an implicit
 * extension. Directory supplies the native current directory for relative
 * names. Input and output may alias; failure leaves no usable identity. */
uint8_t path_resolve(const char *name,const char *directory,char *out);
#endif
