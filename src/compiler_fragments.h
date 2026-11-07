#ifndef ZP_COMPILER_FRAGMENTS_H
#define ZP_COMPILER_FRAGMENTS_H
#include "zp.h"
#if defined(__SDCC) && defined(ZP_UNITS_EXPERIMENT)
#define CC_FRAGMENT_CALL __banked
#else
#define CC_FRAGMENT_CALL
#endif

/* Private native linker proof, not the Pascal .ZPU interface/ABI format.
 * Each immutable source is a bounded slice of caller-owned PAGE_COMPILER
 * pages; offset permits a compiled-unit container to precede its fragment data.
 * Optional binding gives an invocation identity. References to that object's
 * stored identity are local and follow its binding; other IDs stay explicit. The
 * caller retains descriptors/roots in near RAM until this synchronous call
 * returns. No borrowed pointer into a mapped window survives a far operation.
 *
 * All fields are little-endian. Header16: "ZPF1", identity:u16, fragments:u16,
 * relocations:u16, payload_bytes:u16, reserved:u32=0. Then fragment records8:
 * payload_offset, bytes, first_relocation, relocation_count (all u16).
 * Then relocation records8: patch_offset, target_identity, target_fragment,
 * target_offset (all u16). Bits14..15 of target_fragment encode word16=0,
 * low8=1, high8=2; bits12..13 are reserved and must be zero. The low12 bits
 * select the fragment. Address arithmetic precedes byte extraction. Then concatenated fragment bytes. Fragment and
 * relocation extents must partition their respective streams exactly. Patches are ascending and disjoint within each fragment. Target offsets
 * identify actual bytes, never one-past-end. IDs are private catalog identities,
 * not Pascal names or exported type identities. No C structs are serialized. */
#define CC_FRAGMENT_HEADER 16u
#define CC_FRAGMENT_RECORD 8u
#define CC_FRAGMENT_RELOCATION 8u
#define CF_FRAGMENT_MASK 0x0fffu
#define CF_REFERENCE_RESERVED 0x3000u
#define CF_REFERENCE_SHIFT 14u
enum { CF_WORD16,CF_LOW8,CF_HIGH8 };
#define CC_FRAGMENT_OBJECTS 32u
#define CC_FRAGMENT_LIMIT (PAGE_SIZE/4u)
enum { CF_OK,CF_ARGUMENT,CF_OBJECT,CF_TARGET,CF_CAPACITY,CF_MEMORY,CF_CANCELLED,CF_OWNERSHIP };
typedef struct {uint8_t page;uint16_t bytes;uint32_t lifetime,version;uint16_t offset,binding,bindings;uint8_t binding_count;
    uint16_t stored_bytes; /* Zero: full source; otherwise metadata-only bytes. */
    uint8_t continuations,following[3];
    uint32_t following_lifetimes[3],following_versions[3];
} CompilerFragmentObject;
/* Zero continuations preserves a raw one-page source. Paged sources capture
 * their complete buffer chain before publication. Absolute reads require a
 * validated extent; source_read checks its bounded span and ownership. */
uint8_t cc_fragment_source_capture(CompilerFragmentObject *object) CC_FRAGMENT_CALL;
uint8_t cc_fragment_source_live(const CompilerFragmentObject *object) NATIVE;
uint8_t cc_fragment_source_children_live(const CompilerFragmentObject *object) CC_FRAGMENT_CALL;
uint8_t cc_fragment_source_overlap(const CompilerFragmentObject *a,const CompilerFragmentObject *b) CC_FRAGMENT_CALL;
uint8_t cc_fragment_source_byte(const CompilerFragmentObject *object,uint16_t at) NATIVE;
uint8_t cc_fragment_source_read(const CompilerFragmentObject *object,uint16_t at,uint8_t *out,uint16_t count) CC_FRAGMENT_CALL;
typedef struct {uint16_t identity,fragment;} CompilerFragmentRoot;
typedef struct {
    uint8_t pages[2],workspace,valid;
    uint32_t lifetimes[2],workspace_lifetime;
    uint16_t origin,bytes,entry,fragments;
} CompilerFragmentImage;

/* Optional immutable placement catalog, separately owned while a compiler
 * copies code, relocations and maps into its final image. The convenience
 * linker releases this catalog as before; only the explicit layout API keeps
 * it. No source descriptor or caller-stack pointer is retained. */
typedef struct {
    uint8_t page,objects;uint16_t fragments;
    uint32_t lifetime,version;
    uint16_t identities[CC_FRAGMENT_OBJECTS],bases[CC_FRAGMENT_OBJECTS+1];
} CompilerFragmentLayout;
uint8_t cc_fragment_validate(const CompilerFragmentObject *objects,uint8_t count) CC_FRAGMENT_CALL;
/* Envelope/site checks before an external identity namespace is available. */
uint8_t cc_fragment_validate_source(const CompilerFragmentObject *object) CC_FRAGMENT_CALL;
void cc_fragment_layout_init(CompilerFragmentLayout *layout) CC_FRAGMENT_CALL;
uint8_t cc_fragment_layout_close(CompilerFragmentLayout *layout) CC_FRAGMENT_CALL;
uint8_t cc_fragment_layout_get(const CompilerFragmentLayout *layout,uint16_t identity,
                               uint16_t fragment,uint16_t *address) CC_FRAGMENT_CALL;

void cc_fragment_image_init(CompilerFragmentImage *image) CC_FRAGMENT_CALL;
/* On any failure valid stays zero. Failed releases retain their owned page
 * handles so close can retry; never drop a page whose lifetime changed.
 * After any link failure other than CF_ARGUMENT, the caller must call close,
 * which may need retrying. CF_ARGUMENT can leave an existing valid image
 * untouched. */
uint8_t cc_fragment_image_close(CompilerFragmentImage *image) CC_FRAGMENT_CALL;
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
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image) CC_FRAGMENT_CALL;
uint8_t cc_fragment_link_with_layout(const CompilerFragmentObject *objects,uint8_t count,
                        const CompilerFragmentRoot *roots,uint16_t root_count,
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image,
                        CompilerFragmentLayout *layout) CC_FRAGMENT_CALL;
/* Validate, select and place without allocating/copying output pages. The
 * caller owns emission. Close both result owners exactly as for full linking. */
uint8_t cc_fragment_plan(const CompilerFragmentObject *objects,uint8_t count,
    const CompilerFragmentRoot *roots,uint16_t root_count,uint16_t origin,uint16_t limit,
    CompilerFragmentImage *image,CompilerFragmentLayout *layout) CC_FRAGMENT_CALL;
#endif
