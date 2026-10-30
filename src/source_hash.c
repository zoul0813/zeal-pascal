#include "zp.h"

void source_hash(const Source *s,uint8_t out[32]) SOURCE_BLOCK_CALL {
    Sha256 hash;
    uint16_t i,a=0,z,trim;
    sha_init(&hash);
    while(a<s->length) {
        z=a;
        while(z<s->length&&far_get(s->page,z)!='\n') ++z;
        trim=z;
        while(trim>a&&far_get(s->page,trim-1)==' ') --trim;
        for(i=a;i<trim;++i) sha_byte(&hash,far_get(s->page,i));
        if(z<s->length) sha_byte(&hash,'\n');
        a=z+1;
    }
    sha_end(&hash,out);
}
