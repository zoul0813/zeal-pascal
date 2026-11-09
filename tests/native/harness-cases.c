#include "harness.h"

/* CRT case helpers and their source literals share the BANK1 supervisor.
 * Other harnesses retain their existing resident layout. Keep check and seed
 * in harness.c: execution evidence calls them from other banks. */
#if defined(__SDCC) && defined(ZP_SCREEN_LIBRARY)
#pragma codeseg BANK1
#pragma constseg BANK1
#endif

uint8_t load(const char *text) {
#ifdef ZP_SCREEN_LIBRARY
    static const char prefix[]="uses Crt;\n";
    uint8_t ok=source_import(&s,(const uint8_t*)prefix,sizeof(prefix)-1,&d);
    return ok&&source_replace(&s,s.length,s.length,(const uint8_t*)text,(uint16_t)strlen(text));
#else
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
#endif
}
void execution(const char *text,uint8_t kind,uint8_t code) {
    uint8_t ok=load(text);
    check(ok); if(ok) execute_loaded(kind,code,6);
}
void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position) {
    uint8_t ok=load_source(&s,path,&d);
    check(ok); if(ok) execute_loaded(kind,code,expected_position);
}
void checked_error(const char *text,uint8_t code,const char *where) {
    uint16_t position=0,length=(uint16_t)strlen(text),needle=(uint16_t)strlen(where);
    /* Reuse the already-linked byte comparator rather than pulling a separate
     * resident strstr implementation into small native test supervisors. */
    if(needle>length){check(0);return;}
    while(position<=length-needle&&memcmp(text+position,where,needle))++position;
    if(position>length-needle){check(0);return;}
#ifdef ZP_SCREEN_LIBRARY
    position+=sizeof("uses Crt;\n")-1;
#endif
    uint8_t ok=load(text);
    check(ok); if(ok) execute_loaded(3,code,position);
}
#ifdef ZP_SCREEN_LIBRARY
/* Pascal CRT validates its arguments in the Unit body. Its fault belongs to
 * that source, unlike the native Delay bridge's caller-attributed faults. */
void checked_unit_error(const char *text,uint8_t code,uint16_t position) {
    uint8_t ok=load(text);
    check(ok);if(ok)execute_loaded(0x83,code,position);
}
#endif
