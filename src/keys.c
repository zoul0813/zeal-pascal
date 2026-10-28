#include "keys.h"
#include <string.h>
#include <zos_keyboard.h>

static uint8_t control,shift,caps,released;
#ifndef ZP_PASCAL_KEYS
void key_reset(void) KEYCALL { control=shift=caps=released=0; }
#endif
uint8_t key_event(uint8_t c) KEYCALL {
    uint8_t mask,up,alpha;
    if(c==KB_RELEASED) { released=1;return 0; }
    up=released;released=0;
    mask=c==KB_LEFT_CTRL||c==KB_LEFT_SHIFT?1:2;
    if(c==KB_LEFT_CTRL||c==KB_RIGHT_CTRL) {
        if(up) control&=~mask;else control|=mask;
    } else if(c==KB_LEFT_SHIFT||c==KB_RIGHT_SHIFT) {
        if(up) shift&=~mask;else shift|=mask;
    } else if(c==KB_CAPS_LOCK&&!up) caps=!caps;
    else if(!up) {
        if(c>=32&&c<127) {
            alpha=(uint8_t)(c-'a')<26;
            if(control&&alpha) return c-'a'+1;
            if(alpha) { if((shift!=0)^caps) c-=32; }
            else if(shift) {
                const char *plain="`1234567890-=[]\\;',./";
                const char *upper="~!@#$%^&*()_+{}|:\"<>?";
                const char *p=strchr(plain,c);
                if(p) c=upper[p-plain];
            }
            return c;
        }
#ifdef ZP_PASCAL_KEYS
        /* Pascal's physical Enter is CR. Ctrl-J returns LF through the
         * control-letter branch above; the IDE keeps its own LF convention. */
        if(c=='\n') return 13;
        if(c==KB_ESC) return 27;
        if(c==KB_DELETE) return 127;
#else
        if(c=='\n') return c;
#endif
        if(c=='\b'||c=='\t'||c==127||c>=128) return c;
    }
    return 0;
}
