#include "harness.h"
/* Export actual code bytes. Only the host compares or hashes these captures. */
uint8_t code_capture(uint8_t after) SOURCE_BLOCK_CALL {
    char path[]="H:/CODE0.BIN";
    uint8_t bank,bytes[128],ok=1,error;uint16_t at,n,i,done,written;
    int8_t fd;
    path[7]=after?'1':'0';fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    for(bank=0;bank<code_required&&ok;++bank) {
        if(code_pages[bank]==255){ok=0;break;}
        for(at=0;at<code_sizes[bank]&&ok;at+=n) {
            n=code_sizes[bank]-at;if(n>128)n=128;
            for(i=0;i<n;++i)bytes[i]=far_get(code_pages[bank],at+i);
            for(done=0;done<n;done+=written) {
                written=n-done;error=write(fd,bytes+done,&written);
                if(error||!written||written>n-done){ok=0;break;}
            }
        }
    }
    error=close(fd);return ok&&!error;
}
