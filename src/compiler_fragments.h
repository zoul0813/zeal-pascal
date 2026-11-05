#ifndef ZP_COMPILER_FRAGMENTS_H
#define ZP_COMPILER_FRAGMENTS_H
#include "zp.h"

/* Private native linker proof, not the Pascal .ZPU interface/ABI format.
 * Each immutable source occupies one caller-owned PAGE_COMPILER page. The
 * caller retains descriptors/roots in near RAM until this synchronous call
 * returns. No borrowed pointer into a mapped window survives a far operation.
 *
 * All fields are little-endian. Header16: "ZPF1", identity:u16, fragments:u16,
 * relocations:u16, payload_bytes:u16, reserved:u32=0. Then fragment records8:
 * payload_offset, bytes, first_relocation, relocation_count (all u16).
 * Then relocation records8: patch_offset, target_identity, target_fragment,
 * target_offset (all u16). Then concatenated fragment bytes. Fragment and
 * relocation extents must partition their respective streams exactly. Word
 * patches are ascending and disjoint within each fragment. Target offsets
 * identify actual bytes, never one-past-end. IDs are private catalog identities,
 * not Pascal names or exported type identities. No C structs are serialized. */
#define CC_FRAGMENT_HEADER 16u
#define CC_FRAGMENT_RECORD 8u
#define CC_FRAGMENT_RELOCATION 8u
#define CC_FRAGMENT_OBJECTS 32u
#define CC_FRAGMENT_LIMIT (PAGE_SIZE/4u)
enum { CF_OK,CF_ARGUMENT,CF_OBJECT,CF_TARGET,CF_CAPACITY,CF_MEMORY,CF_CANCELLED,CF_OWNERSHIP };
typedef struct {uint8_t page;uint16_t bytes;uint32_t lifetime,version;} CompilerFragmentObject;
typedef struct {uint16_t identity,fragment;} CompilerFragmentRoot;
typedef struct {
    uint8_t pages[2],workspace,valid;
    uint32_t lifetimes[2],workspace_lifetime;
    uint16_t origin,bytes,entry,fragments;
} CompilerFragmentImage;

void cc_fragment_image_init(CompilerFragmentImage *image);
/* On any failure valid stays zero. Failed releases retain their owned page
 * handles so close can retry; never drop a page whose lifetime changed.
 * After any link failure other than CF_ARGUMENT, the caller must call close,
 * which may need retrying. CF_ARGUMENT can leave an existing valid image
 * untouched. */
uint8_t cc_fragment_image_close(CompilerFragmentImage *image);
/* Validate even unreachable source records before emission. Roots retain
 * transitive references, including recursive cycles. Independently place only
 * selected fragments in catalog/fragment order within8000..limit<=F400.
 * root0 determines entry. Output pages are allocated only for the retained
 * high-address span; untouched slots stay255 and all unoccupied bytes in
 * allocated pages are cleared. Source ownership and
 * bytes stay with the caller; no disk/library dependency remains in the image.
 * cc_poll is the intended synchronous compiler cancellation boundary;
 * the standalone fixture supplies a test implementation. */
uint8_t cc_fragment_link(const CompilerFragmentObject *objects,uint8_t count,
                        const CompilerFragmentRoot *roots,uint16_t root_count,
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image);
#endif
