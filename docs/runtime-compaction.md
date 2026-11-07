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
Production layout and generated programs are unchanged. Runtime objects reuse
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
Signed/wrapping addends are deliberately rejected until qualified. The native
fragment format still accepts only word references: typed byte references must
be implemented and validated before runtime objects can use that format.
No byte reference is emitted or coerced into a word patch by this checkpoint.
