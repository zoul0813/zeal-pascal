#include "compiler.h"
#include "transfer_asset.h"
#ifdef __SDCC
#include "library.h"
extern const uint8_t transfer_asset_count,transfer_asset_rows[];
#else
extern const uint8_t transfer_start[];
#endif

uint8_t cc_transfer_asset_get(uint8_t kind,uint16_t offset) FRONTEND {
    if(kind)offset+=TRANSFER_MODULE_BYTES;
    if(kind>1)offset+=TRANSFER_GATE_BYTES;
    if(kind>2)offset+=TRANSFER_CALL_BYTES;

#ifdef __SDCC
    uint8_t i;uint8_t bank;
    uint16_t first=0,end,source;
    const uint8_t *row=transfer_asset_rows;
    for(i=0;i<transfer_asset_count;++i,row+=5) {
        end=row[0]|((uint16_t)row[1]<<8);
        if(offset<end) {
            source=(row[2]|((uint16_t)row[3]<<8))+offset-first;bank=row[4];
            return bank?far_get(code_pages[bank-1],source&0x3fffu):*((const uint8_t*)source);
        }
        first=end;
    }
    return 0;
#else
    return transfer_start[offset];
#endif
}
