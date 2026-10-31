#include "compiler_job_wire.h"

/* Decoded streams are owned/authenticated by the caller. A small parent-stack
 * buffer bounds both mapping time and stack use; no mapped pointer escapes. */
uint8_t job_hash_stream(const JobStream *s,uint16_t zero,uint16_t zero_bytes,
                        uint8_t digest[32]) SOURCE_BLOCK_CALL {
    Sha256 hash;
    uint8_t buffer[64],j;
    uint16_t i=0,n,at,remaining,length;
    if(s->length>3UL*PAGE_SIZE)return 0;
    length=(uint16_t)s->length;
    if(s->count!=(length?(length+PAGE_SIZE-1u)/PAGE_SIZE:0))return 0;
    sha_init(&hash);
    while(i<length) {
        n=sizeof(buffer);remaining=length-i;
        if(n>remaining)n=remaining;
        at=i%PAGE_SIZE;remaining=PAGE_SIZE-at;
        if(n>remaining)n=remaining;
        if(i>=zero&&i-zero<zero_bytes) {
            remaining=zero_bytes-(i-zero);
            if(n>remaining)n=remaining;
            memset(buffer,0,n);
        } else {
            /* Stop before the normalized window; never read its bytes. */
            if(zero_bytes&&i<zero&&n>zero-i)n=zero-i;
            if(!far_read(s->pages[i/PAGE_SIZE],at,buffer,n))return 0;
        }
        for(j=0;j<n;++j)sha_byte(&hash,buffer[j]);
        i+=n;
    }
    sha_end(&hash,digest);return 1;
}
