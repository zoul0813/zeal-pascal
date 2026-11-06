# Caller argument allocation checkpoint

The shared-page eligibility estimate now includes live caller-owned arguments.
Earlier arguments remain counted while later or nested arguments are evaluated.
Callee cleanup restores the outer count, including suppressed SizeOf selectors.

Target verification passes 905 native assertions with zero failures. A 512-byte
argument frame uses two execution pages; a 516-byte frame and a nested call
crossing the threshold use three. Memory Run and saved execution both pass.
Fixtures check Real argument order, VAR access, nested results and suppression.
Failure branches use Integer division by zero rather than normal Halt.

`docs/program-pages.md` records the previous measurement checkpoint. Its 777
assertions and 239941-byte container are historical figures. This checkpoint
has 905 assertions and a 240094-byte production container. Small baseline
programs continue to own two execution pages. Compiler cache counts do not change.

The 512-byte threshold is an estimate of arguments, generated temporaries and
callee reservations; it is not an exact count of saved expression operands or
statement frames. Runtime guards check actual SP. Very deep expressions can
still overflow the shared reserve instead of being statically promoted to
three pages. Extending that estimate is separate work; this change closes the
omitted argument accounting gap.

Host sanitizers, the 13-case page measurement and complete 17-case optional Real
comparison passed. Reports and input hashes are retained in
`allocation-arguments-evidence.json`; the two-turn review is
`../reviews/allocation-arguments-v1.md`.
