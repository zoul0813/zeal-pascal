#include "compiler.h"
#include "transfer_asset.h"
extern const uint8_t transfer_start[];

uint8_t cc_transfer_asset_get(uint8_t kind,uint16_t offset) FRONTEND {
    if(kind)offset+=TRANSFER_MODULE_BYTES;
    if(kind>1)offset+=TRANSFER_GATE_BYTES;
    if(kind>2)offset+=TRANSFER_CALL_BYTES;

    return transfer_start[offset];

}
