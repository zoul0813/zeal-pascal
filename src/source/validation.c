#include "source/validation.h"

/* One complete-text proof, recorded only after a successful replacement.
 * Any external store, changed extent or reuse of the page makes this miss. */
static struct {
    uint8_t page;
    uint16_t length;
    uint32_t lifetime,version;
} known;

void source_validation_window(const Source *s,uint16_t first,uint16_t last,
                              uint16_t *begin,uint16_t *end) SOURCE_BLOCK_CALL {
    uint32_t lifetime=page_lifetime(s->page);
    *begin=0; *end=s->length;
    if(lifetime&&lifetime==known.lifetime&&s->page==known.page&&
       s->length==known.length&&page_versions[s->page]==known.version) {
        /* The unchanged outer lines are already valid. Include every old
         * byte that may join the replacement, stopping at line boundaries. */
        while(first&&buffer_get(s->page,first-1)!='\n') --first;
        while(last<s->length&&buffer_get(s->page,last)!='\n') ++last;
        *begin=first; *end=last;
    }
}

void source_validation_record(const Source *s) SOURCE_BLOCK_CALL {
    known.page=s->page; known.length=s->length;
    known.lifetime=page_lifetime(s->page);
    if(known.lifetime) known.version=page_versions[s->page];
}
