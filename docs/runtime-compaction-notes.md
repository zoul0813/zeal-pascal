# Runtime compaction checkpoint

Global addresses are resolved during the final static link. The current integer
runtime ends at `$579A`, and the full Real runtime ends at `$7FFF`; these are
measurements, not fixed global addresses. A change to the retained runtime can
move globals. The program header records their resolved base, and compiler and
unit references are relocated relative to it. `$8000` remains an internal
staging address, not the final address of ordinary program globals.

The compiler-bank lending discussed in the review is code-cache management
during linking. Generated file I/O uses the program’s normal mapping and stack.

Tests that inspect a program must read its header or use `Addr`. The page-crossing
fixture now computes the crossing from `Addr(Pad)`; the allocation-boundary
fixture reads the base from a previously linked header. Synthetic decoder/file
fixtures may deliberately supply a particular header layout.

The [measurements and test evidence](runtime-compaction-evidence.json) record
10,342 recovered bytes, one/two/three-page execution, and the unchanged-source
qualification. Snake is 35,699 bytes, with 10,381 static bytes free. Memory and
Com-file compilation produced identical files. It remains one self-contained
COM; no asset file or ZPU is needed while playing.

The full ZVB run retained two obsolete page-alignment failures and an observer
count that included three earlier hardware executions. The evidence preserves
that original report. The corrected page fixture passed its targeted RAM/saved
rerun, and the journal authenticates all 15 client executions plus those three
hardware executions. Other clients were unchanged. The updated observer counts
client stages explicitly.

## Retained limits

- Chain/Execute preserve the existing full borrowed-runtime contract. CHN static
  bounds now also account for known stack demand; a large frame near End can
  therefore be rejected more conservatively.
- Public I/O hook variables retain the existing full-runtime contract. The
  `ConOutPtr`/`ConStPtr` callback probe passes RAM, detached and cold execution.
  Ordinary compact console adapters are separately covered.
- The options screen previews the most recently compiled layout. Recompile the
  current program before relying on those estimates; final compilation checks
  its actual layout and stack demand.
- A unit cannot currently take the address of a zero-width implementation
  global at the end of its data. Empty record types and program-level one-past
  globals remain supported.
- The existing CPU-stack window remains `$C800`–`$FC00`, with runtime guards.
  The standalone screen-test fixture retains its documented pre-existing
  resident overflow; shipped CRT is covered by other native tests.

[Opus review and dispositions](../reviews/runtime-compaction-v1.md) cover the
production checkpoint `287e183`. The additional bank-residency proof checks the
linked product's actual map, objects and emitted calls. Later changes are
verification fixtures and documentation, not production code. Emulator checks
do not establish physical-board, NMI or audio-waveform acceptance.
