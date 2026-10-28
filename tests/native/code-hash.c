#include "harness.h"

/* Test-only integrity scanning shares an existing code bank; it need not
 * consume the resident space reserved for the fixed native ABI probe. */
void code_hash(uint8_t *out) SOURCE_BLOCK_CALL {
    Sha256 hash; uint8_t bank; uint16_t i;
    sha_init(&hash);
    for(bank=0;bank<code_required;++bank)
        for(i=0;i<code_sizes[bank];++i) sha_byte(&hash,far_get(code_pages[bank],i));
    sha_end(&hash,out);
}
