#ifndef ZP_PATH_H
#define ZP_PATH_H
#include <stdint.h>
#if defined(__SDCC) && defined(ZP_IDE_PATHS)
#define PATH_CALL __banked
#else
#define PATH_CALL
#endif

/* Canonical native file identity, without filesystem access or an implicit
 * extension. Directory supplies the native current directory for relative
 * names. Input and output may alias; failure leaves no usable identity. */
uint8_t path_resolve(const char *name,const char *directory,char *out) PATH_CALL;
/* Directory identity also accepts root, trailing separators and final dot/
 * dot-dot. Canonical output has no trailing slash except a drive root. */
uint8_t path_directory(const char *name,const char *directory,char *out) PATH_CALL;
uint8_t path_program(const char *root,char *out,uint8_t chain) PATH_CALL;
#endif
