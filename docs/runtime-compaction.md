# Compact runtime implementation

Implementation starts at checkpoint399b2d5 on explore/pascal-units. This is an
approved native Zeal change, not a claim of completed TP3 recovery. Example and
library work is preserved; no additional Snake packing is planned.

## Required result

- Statically link only retained optional runtime/file helpers into each COM.
- Keep all program-owned addresses symbolic until final link. Preserve only
  platform-defined addresses and explicit Pascal absolute-address contracts.
- Replace the fixed16KB prefix with a compact common core. Optional Real lives
  outside that core; non-Real programs reserve no Real-sized hole.
- Remove the compressed file-service overlay and its dedicated page/stack.
  Preserve Pascal file formats, semantics, ownership checks and errors.
- Support one/two/three physical execution pages where proven safe, including
  stack/control relocation, alias-aware bounds and actual kernel ownership.
- Add executable-block directive `{$RUNTIME REAL}` for opaque native code.
  Inline/External alone must not imply Real. Unused unit fragments stay unused.
- Preserve Memory Run, source/error locations, unit initialization and existing
  Chain/Execute semantics. No runtime ZPU loader or old-artifact readers.

## Stages

1. Hash-bound file/layout/native-page baseline.
2. Relocatable runtime objects and symbolic helper references.
3. Compact common core, optional Real and dynamic program placement.
4. Selective file helpers using normal program mappings and ZOS buffers.
5. Lifecycle/transfers and physical execution-page allocation.
6. Full relevant qualification, Opus reviews, rebuilt stock units and demos.

The cold compiler budget remains10 pages. Record warm/peak compiler RAM, image
bytes, retained helpers, usable static capacity and execution ownership
separately. Demonstrate at least8KB additional usable static capacity and
one-page execution of eligible minimal/Integer/leaf-unit cases. Do not claim
RAM savings solely from compressed files or counting zero bytes.

## Current work

Baseline capture and build-time object-reader verification pass.
Production layout and generated programs are unchanged. Runtime objects will extend
the unit linker's dependency/relocation model; the build-time converter, not
the native Pascal parser, reads SDCC-specific records. Word and low/high-byte
address relocations must match independently linked toolchain output.

The existing core/file implementation remains the behavior oracle while
replacement paths are qualified. Passing experiments do not enable production
defaults before integrated checks pass. Commit known working stages separately.


The baseline passes 253 native checks, including module-window mappings and
release before parent cleanup. Non-Real prefix ownership is 5,900 bytes plus
10,484 unowned bytes; the largest unowned span is the 10,287-byte Real hole.
The final 193 bytes include 138 live uninitialized bytes and 55 unowned bytes.
See runtime-baseline-evidence.json for hashed inputs and physical observations.

The converter passes 11 unit tests and six exact linked-image comparisons,
including independently selected SDK dependencies and all initialized bytes.
Signed/wrapping addends are deliberately rejected until qualified. At the foundation checkpoint the native fragment format accepted only word
references. Stage 2 extends that format, while Pascal final placement still
needs a typed reference model before runtime integration can use it.
No byte reference is coerced into a word patch.


Stage 2 at checkpoint c2af3c6: ZPF1 fragment records retain their eight-byte size, using
the bounded fragment-index word's upper bits for word16/low8/high8 kinds. The
standalone fragment linker resolves the full target before extracting bytes.
The Pascal unit importer still accepts only word references, explicitly rejecting
byte kinds until its final-placement ledger is updated. The production compiler
ledger stays four bytes; a blanket eight-byte expansion was rejected because
it increases workspace and exceeds two compiler code-bank limits. Its redesign
belongs with integrated runtime placement and capacity qualification.


Typed fragment qualification passes 1,013 host assertions, 952 native assertions,
66 IDE unit checks and 13 native importer rejection checks. The importer checks
use the same valid Calc unit as a control and alter only one reference kind
with a recomputed artifact checksum; low/high variants fail without publication
or retained transient banks. The original ledger still passes 1,824 host checks.
Cold startup remains10 code pages; observed warm IDE checkpoints remain15.
Dynamic compiler peaks are not measured by this checkpoint.

The original bank placement is preserved. Compiler bank16 occupies16,382 of
16,384 bytes, so further code growth requires layout work. The resident remains
16,031 bytes; zpascal.zpl is277,582 bytes (244 more than the baseline). This
checkpoint has not removed the Real hole, file overlay or runtime page padding.
See runtime-typed-links-evidence.json for exact inputs and observations.

## Production final-placement integration

The next checkpoint accepts word/low/high native unit references. A sparse
eight-byte symbolic stream complements the existing four-byte coefficient
ledger, so byte extraction happens after final placement and carries correctly.
Only retained unit byte references currently create these records; ordinary
global accesses and Inline operands retain their existing representation until
compact data placement is implemented. DATA and coefficient-adjustment records
are exercised on the host, but have no production emitter yet. This avoids
adding no-op metadata and linker bank reads to every ordinary compilation.

Generic linking occupies transient BANK19; compiler tables share deferred
BANK16, with arena disposal in its own permanent BANK9 source file so closing a build
never needs a successful deferred-bank restore. Fragment source capture remains
in BANK11, and hot source access remains resident. The overlay journal is
unchanged: symbolic records are created only during final unit linking, after
parser groups close. Any future emitter active during parsing must first
define rewind/journal handling for those records.

This is still stage 2. It does not reclaim the Real hole, move globals below
$8000, remove file overlays, or enable one-page execution. Those remain stages
3–5; existing runtime bytes continue to be the behavior oracle.

The [integration evidence](runtime-placement-evidence.json) records 16 native
byte-placement checks, 80 sanitized placement/arena checks, 1,824 original
ledger checks, 1,761 native Real/dependency assertions, 642 initialization/Chain
assertions, 66 final positive IDE checks, 51 cancellation/retry checks and 17
malformed-unit checks. All selected checks pass. Independently executed saved
CRC, DMA, Graphics and Sound clients pass without compiler/source/unit files;
the larger ZVB observer hit its fixed breakpoint-loop bound and is recorded as
inconclusive rather than a successful framework run.

Cold startup still uses ten compiler code pages. The launcher is 16,035 bytes;
the compiler library is 278,802 bytes. Program runtime/RAM savings remain zero
at this checkpoint. Six map checks confirm actual permanent/transient symbol
placement, including cleanup's bank tag and actual address.

The standalone CRT fixture remains outside its resident page: an isolated ZDE
rebuild of c2af3c6 proves a pre-existing 307-byte overflow (plus BANK3/BANK12
overflows). This stage resolves the two code-bank overflows, while the resident
overflow is 311 bytes. The default ALL build was already failing at the prior
checkpoint; this is not a qualified full-suite build. Full macro-on language
corpora, peak compiler RAM and BANK19-specific free-failure injection remain
qualification work. Two Opus turns found no remaining code defect; final IDE
and baseline evidence resolves the follow-up's pre-commit requirements.
