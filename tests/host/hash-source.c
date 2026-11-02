#include "hash-evidence.h"

void test_source_hash(const Source *s,uint8_t out[32]) SOURCE_BLOCK_CALL {
    Sha256 hash;
    uint16_t i,spaces=0;
    uint8_t c;
    sha_init(&hash);
    for(i=0;i<s->length;++i) {
        c=buffer_get(s->page,i);
        if(c==' ') ++spaces;
        else {
            /* Only a later nonspace on this line makes pending spaces data. */
            if(c!='\n') while(spaces) { sha_byte(&hash,' '); --spaces; }
            spaces=0;
            sha_byte(&hash,c);
        }
    }
    sha_end(&hash,out);
}

void test_image_hash(const Build *b,uint8_t out[32]) {
    Sha256 hash;uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->length;++i)sha_byte(&hash,image_get(b,i));
    sha_end(&hash,out);
}
