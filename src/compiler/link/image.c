#include "image.h"

/* The compiler's banked entry owns the switch; native_decode is a near call
 * shared with standalone loader modules. Keep both in image_compiler's bank. */
uint8_t native_header(const uint8_t *h,uint8_t chain,NativeImage *id) SOURCE_BLOCK_CALL {
    return native_decode(h,chain,id);
}
