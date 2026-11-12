#include "compiler/groups/native.h"
extern const uint8_t overlay_asset_start[];
uint8_t cc_overlay_asset_get(uint16_t at) FRONTEND {return overlay_asset_start[at];}
