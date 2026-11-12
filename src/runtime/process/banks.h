#ifndef ZP_OVERLAY_BANKS_H
#define ZP_OVERLAY_BANKS_H
#include <stdint.h>

#define OVERLAY_RAM_PAGES 32u
#define OVERLAY_GROUPS 100u
#define OVERLAY_NO_PAGE 255u
#define OVERLAY_NO_MEMBER 65535u

/* Near metadata and caller stack stay outside the replaceable8000..BFFF
 * window. The immutable directory belongs to the main image and remains live
 * until close; logical member IDs are independent of physical page slots. */
typedef struct {
    const uint8_t *directory;
    uint16_t count,directory_bytes;
    uint8_t groups,drive,ready,error;
    char basename[12],path[128];
    uint16_t selected[OVERLAY_GROUPS],active[OVERLAY_GROUPS],parent[OVERLAY_GROUPS];
    uint32_t extent[OVERLAY_GROUPS];
    int8_t files[OVERLAY_GROUPS];
    uint8_t drives[OVERLAY_GROUPS],execution[OVERLAY_GROUPS];
    uint16_t cached_member[OVERLAY_RAM_PAGES];
    uint8_t cached_page[OVERLAY_RAM_PAGES],victim;
    uint8_t owned[OVERLAY_RAM_PAGES],owned_count;
    uint8_t buffer[128];
} OverlayBanks;

/* Private native directory ZBD1:16-byte header followed by16-byte records.
 * Header: magic[4], version:u16=1, count:u16, groups:u16, record_bytes:u16=16,
 * reserved:u32=0. Record: group:u16, parent_member:u16 (FFFF=resident),
 * code_bytes:u16, entry_offset:u16, file_offset:u32, Fletcher16:u16,
 * reserved:u16=0. IDs are record ordinals; parents precede their children.
 * A group has one parent; groups are dense0..99. Each group's .000..099 file
 * repeats the complete directory, replacing magic with ZBG1 and header word12
 * with its group ID, followed by only that group's dense member payloads.
 * All multi-byte fields are little endian. No CP/M binary format is consumed.
 * Fletcher16 protects accidental corruption, not cryptographic authenticity. */
void overlay_banks_init(OverlayBanks *banks);
/* Validate main-image metadata only: no disk access and no page allocation.
 * basename is a canonical relative compiler-bound path without extension,<=11 bytes.
 * Failure requires close before another bind, as do later load failures. */
uint8_t overlay_banks_bind(OverlayBanks *banks,const char *basename,
    const uint8_t *directory,uint16_t directory_bytes);
/* TP3 drive numbering:0=current,1=A,...,26=Z. Validate the mounted drive
 * before committing the selection. Existing opened groups retain their own
 * handle/drive. Paths use the current directory on the selected drive; no
 * chdir, executable-path inference or fallback lookup is performed. */
uint8_t overlay_banks_set_drive(OverlayBanks *banks,uint16_t drive);
/* First use opens the group on the selected drive and loads only this member.
 * Subsequent cache hits perform no file I/O/allocation. Immutable templates
 * may be recycled on physical RAM exhaustion; misses reread the ORIGINAL
 * opened group handle. Mutable group pages remain live until close.
 * Changing members resets code/constants from the immutable template and
 * invalidates descendant selections; active code may never be replaced.
 * Enter returns an owned physical execution page or255, never maps it. */
uint8_t overlay_banks_enter(OverlayBanks *banks,uint16_t member);
uint8_t overlay_banks_leave(OverlayBanks *banks,uint16_t member);
uint16_t overlay_banks_entry(const OverlayBanks *banks,uint16_t member);
/* Restore the original MMU mapping BEFORE close, including fatal unwinding.
 * The execution owner treats loading failures as fatal overlay errors even
 * under I-. Capture error before successful close resets the context.
 * Failed frees retain their owned handles for a subsequent cleanup attempt. */
uint8_t overlay_banks_close(OverlayBanks *banks);
#endif
