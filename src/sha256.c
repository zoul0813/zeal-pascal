#include "zp.h"

static const uint32_t k[64] = {
0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL};
/* Every SHA rotation has a fixed count. Keep it visible to SDCC so it can
 * select byte moves and fixed shifts instead of a 32-bit variable shift loop. */
#ifdef __SDCC
uint32_t sha_rr2(uint32_t x) NATIVE;
uint32_t sha_rr6(uint32_t x) NATIVE;
uint32_t sha_rr7(uint32_t x) NATIVE;
uint32_t sha_rr11(uint32_t x) NATIVE;
uint32_t sha_rr13(uint32_t x) NATIVE;
uint32_t sha_rr17(uint32_t x) NATIVE;
uint32_t sha_rr18(uint32_t x) NATIVE;
uint32_t sha_rr19(uint32_t x) NATIVE;
uint32_t sha_rr22(uint32_t x) NATIVE;
uint32_t sha_rr25(uint32_t x) NATIVE;
#define rr(x,n) sha_rr##n(x)
#else
#define rr(x,n) (((x)>>(n))|((x)<<(32-(n))))
#endif
static void transform(Sha256 *s) {
    uint32_t t,u,x,y;
    uint32_t *w=s->schedule,*v=s->working;
    uint8_t i,j;
    for (i=0;i<16;++i) {
        j=i*4;
        w[i]=((uint32_t)s->block[j]<<24)|((uint32_t)s->block[j+1]<<16)|
             ((uint32_t)s->block[j+2]<<8)|s->block[j+3];
    }
    memcpy(v,s->h,32);
    for(i=0;i<64;++i) {
        j=i&15;
        if(i>=16) {
            x=w[(i+1)&15]; y=w[(i+14)&15];
            w[j]+=(rr(x,7)^rr(x,18)^(x>>3))+w[(i+9)&15]+(rr(y,17)^rr(y,19)^(y>>10));
        }
        t=v[7]+(rr(v[4],6)^rr(v[4],11)^rr(v[4],25))+((v[4]&v[5])^(~v[4]&v[6]))+k[i]+w[j];
        u=(rr(v[0],2)^rr(v[0],13)^rr(v[0],22))+((v[0]&v[1])^(v[0]&v[2])^(v[1]&v[2]));
        for(j=7;j>0;--j) v[j]=v[j-1];
        v[4]+=t; v[0]=t+u;
    }
    for(i=0;i<8;++i) s->h[i]+=v[i];
}
void sha_init(Sha256 *s) {
    static const uint32_t init[8]={0x6a09e667UL,0xbb67ae85UL,0x3c6ef372UL,0xa54ff53aUL,
        0x510e527fUL,0x9b05688cUL,0x1f83d9abUL,0x5be0cd19UL};
    memcpy(s->h,init,sizeof(init)); s->bytes=0; s->used=0;
}
void sha_byte(Sha256 *s,uint8_t b) {
    s->block[s->used++]=b; ++s->bytes;
    if(s->used==64) { transform(s); s->used=0; }
}
void sha_end(Sha256 *s,uint8_t out[32]) {
    uint32_t bits=s->bytes*8;
    uint8_t i;
    sha_byte(s,128);
    while(s->used!=56) sha_byte(s,0);
    for(i=0;i<4;++i) sha_byte(s,0);
    for(i=0;i<4;++i) sha_byte(s,(uint8_t)(bits>>(24-i*8)));
    for(i=0;i<32;++i) out[i]=(uint8_t)(s->h[i/4]>>(24-(i&3)*8));
}
void source_hash(const Source *s,uint8_t out[32]) {
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
