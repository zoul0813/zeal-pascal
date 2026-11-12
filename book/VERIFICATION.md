# Pilot verification and morning check

The original pilot evidence below is historical. The current OPFS integration and updated checks are recorded at the end of this document; they supersede the earlier T: session-only behavior and manual-start instructions.

Implemented in branch `codex/next-work`, under `book/`. No commit, push, GitHub Pages deployment, or third-party source modification was performed.

## What passed

- Hugo 0.167.0 builds the site without warnings, both at a localhost root and under the GitHub Pages `/zeal-pascal/` project path.
- All generated internal asset/navigation links resolve in the production output. Reveal sections contain their generated Pascal listings rather than omitted raw HTML.
- The local preview returned HTTP 200 for the lesson.
- Three Node tests cover preservation of existing files and fresh-copy isolation, failed starter downloads, and backup validation (binary round-trip, traversal, duplicate paths, file/directory conflicts, invalid data).
- JavaScript syntax checks pass.
- All 60 staged assets match `static/provenance.json`. Printed listings, served Pascal files, and the reproducible source ZIP share the same source files.
- The exact `HELLO.PAS`, `START.PAS`, and `SOLUTION.PAS` compile on the installed unmodified native Zeal emulator using the staged Pascal binaries and the final compatible boot ROM. Fresh native COM envelopes and extents were checked, including the current single-page flag.
- Native execution of the exact compiled starter and solution accepted ten physical keyboard command lines each. Observed room states after `look, n, e, n, inspect, w, s, s, xyz` were `1, 2, 3, 3, 3, 2, 1, 1, 1`; both also accepted `quit`. These checks establish movement and input behavior, not browser display behavior or every output message.

Retained native evidence (local, ignored build output):

- `../build/book-native/20261111T044210.901452Z-90387/report.json`: successful compilation of the exact published sources.
- `../build/book-runtime/20261111T044243.438727Z-90764/report.json`: successful runtime commands and room-state observations, with per-program input receipts and console journals.
- The runtime check used compiler outputs from the earlier successful compilation at `../build/book-native/20261111T044013.454241Z-88439/hostfs/`. That run's generic legacy host validator rejected the new valid single-page flag; the compiler had produced all three outputs. The final verifier handles the current header. The source and staged compiler bytes did not change between those runs.

## Compatibility finding

The original community TF image is retained byte-for-byte. Its bundled `/sys/zshell.img` did not launch this compiler in two bounded native attempts; both remained in the shell and timed out. The existing ZDE `os_with_romdisk.img` did launch the IDE and compile the examples. The site therefore boots that compatible ZShell ROM while mounting the original community TF card. This is supported emulator ROM configuration, not a patched kernel or application.

Final boot ROM SHA-256: `87de2b8ac3d71514f90984231da02af4f0ad054a730bda18b6617e89b090d38d`.

The native WASM bundle and matching HostFS wrapper were copied unchanged from the installed ZDE microbe deployment (the same native emulator family used by ZShell). Hashes bind the exact shipped binaries; the precise source revision of that prebuilt WASM bundle was not independently established. The bundle has the required HostFS and native-exit exports. Browser execution remains to be checked.

## What remains unverified

The computer-use browser host returned **“The browser access host is unavailable.”** No alternate browser automation was used. Actual browser startup, rendered appearance, keyboard behavior, OPFS persistence through the emulator, and backup/import controls therefore still need a browser pass. Native checks do not establish these behaviors.

The project-required restricted Opus creative and code reviews were attempted. Both failed before review with API status 429, `usage_limit_reached`; the reported reset is November 12 at 9am America/New_York. No review findings, follow-up, or completed milestone verdict exist. Local diagnostics are in `.review-workflow/` in this worktree. Those reviews remain outstanding.

No physical-hardware qualification, mobile/touch programming support, or GitHub Pages deployment is claimed. T: edits are session-only; the persistent reader workspace is H: through the upstream HostFS backend and browser OPFS. The installed emulator's image store uses IndexedDB, not localStorage; no replacement localStorage filesystem was built.

## Try it in the morning

From this worktree:

```sh
hugo server --source book --bind 127.0.0.1 --port 1313 --baseURL http://localhost:1313/
```

Open <http://localhost:1313/lessons/the-silent-lab/> in desktop Chrome or Edge.

1. Confirm the reading layout works before opening the emulator. Open/hide it from the header and from the lesson callout; expand/reduce it.
2. Start Zeal. At ZShell run `H:/bin/zpascal.bin H:/lib/zpascal.zpl`; press Y at the messages prompt.
3. Use W to open `H:/book/lab/HELLO.PAS`. E enters the editor; Ctrl-K then D returns to the menu. R should print the greeting.
4. Open `H:/book/lab/START.PAS`, then use A to save your copy as `H:/work/LAB.PAS`. Run it and follow the lesson's movement table.
5. Change a room description, save with S, and reload the page. Start the emulator and Pascal again, then open `H:/work/LAB.PAS`; confirm the change remains. Unsaved editor RAM should not be treated as persistent.
6. Download a workspace backup after saving. Make a fresh lesson copy and confirm earlier files remain. Import the backup and open its displayed new path; it should not replace existing work.
7. Try “Load all book code.” It should add the pilot source collection without replacing edits. There is only one lesson in this edition.
8. Verify F8 releases focus to the book and Ctrl-K reaches the IDE rather than browser search. Verify normal page Tab navigation works outside the canvas.
9. Open a second workbench tab; it should refuse concurrent workspace access. Closing the first tab should allow startup in the second.
10. Reveal the hints and solution, compare the expected behavior, and note any pacing, display, or input issues.

The preview server used during implementation is stopped at handoff. The manual workflow can build a Pages artifact after the changes are committed/pushed; publication is a separate action.

## OPFS workbench integration — November 11

The book now includes the unchanged WASM binary, generated JS, and four matching libnative helpers from the clean NativeEmulator `feature/wasm-opfs` checkout at `ea901bb296071e0b6019741eef1299c85de22826`. The source checkout was not modified. Bundle requests carry its commit as a cache-version parameter. The community TF image, compatible boot ROM, and Pascal compiler assets remain unchanged.

The book EEPROM is a raw 64 KiB ZealFS v2 image, created with the existing Zeal Disk Tool CLI. Its two files are byte-checked against `eeprom/autoexec.zs` and `eeprom/welcome.txt`. The startup configuration preserves ZShell's community PATH entries, adds `H:/bin/`, changes to `H:/` so the installed Pascal library is found, and displays the book welcome. Readers launch with `zpascal`, not a full executable/library path.

Current checks:

- The actual native Zeal run at `../build/book-native/20261111T155800.788222Z-79205/report.json` confirms EEPROM startup, argument-free `zpascal` launch through PATH, compilation of all three exact lesson sources, saving the default T: source, and reopening it with the correct path and byte count after restarting. The companion reopen capture is under `../build/book-native-reopen/`. Earlier reopen attempts exposed test input arriving at the wrong readiness boundary; the final check observes the loaded default Work file before injecting any prompt response.
- Node tests run the book entry point with the actual copied ZealNative wrapper and mock browser/runtime facilities. They verify Open Zeal auto-starts once, opening again preserves the running guest, OPFS/project namespace/EEPROM configuration is passed correctly, existing H: files remain intact, HostFS infrastructure callbacks remain visible even for NotFoundError, and real quota/write failures remain visible. A separate test runs the actual bundled HostFS helper: missing command candidates and directories return status 4 without console errors or callbacks; root directory opening succeeds; an invalidated workspace returns failure with both diagnostics and callbacks. Workspace backup and import-path validation tests also pass. These are integration-logic checks, not real WASM browser execution.
- Asset hashes, EEPROM authoring contents, lesson ZIP/source consistency, JavaScript syntax, and a GitHub Pages project-path Hugo build pass.

Reader behavior now differs from the original pilot:

- Open Zeal starts automatically. Start Zeal is only shown after startup failure. The existing theme toggle is preserved.
- TF and EEPROM images use direct OPFS persistence with a stable per-project namespace derived from Hugo's base path and fixed image keys. No image snapshot timer or book-side IndexedDB/localStorage save layer is used. H: retains its original `zeal-pascal-book` OPFS directory, so earlier files are preserved.
- Existing disk images are not replaced automatically. Restore startup EEPROM explicitly replaces B: and restarts the guest only after a reader confirmation, preserving T: and H:. Unsaved editor RAM is discarded by that restart.
- The upstream HostFS fix separates guest filesystem outcomes from infrastructure failures. Expected missing-file probes return guest statuses without console errors or callbacks. The book no longer filters callbacks by operation number or exception name; invalidated workspaces and other real failures remain visible. No storage reset is needed. All six Node tests pass with this bundle; browser boot and interactive ls have not been independently rerun.
- Normal status text has reserved space, the canvas has a stable presentation aspect ratio, and the panel reserves scrollbar space to reduce startup movement. A favicon is provided. Actual visual behavior remains to be checked in Chrome.

The browser automation host remains unavailable; this integration has not been visually or interactively verified through Chrome by Codex. In particular, actual browser save/refresh behavior, sound, and the new startup-EEPROM restore control need a browser pass. The upstream completed-device-write and abrupt-browser-termination tests were inspected during review, not independently rerun here. The ScriptProcessorNode deprecation warning originates in the unchanged upstream audio implementation.

To check the current flow: choose Open Zeal, wait for the book welcome, enter `zpascal`, open an example, save to H: or the default T: path, refresh, reopen Zeal, and verify the saved source. Use W for arbitrary H: files; the IDE automatically selects only its default T: source. Do not clear origin storage as a troubleshooting step without backing up personal files.

## Midnight mockup alignment and syntax highlighting (November 12)

The header now uses a pill-style theme switch with an accessible stable `Dark theme` switch name, checked state, and visible next-action label. The switch retains system-default behavior, remembered choices, and cross-tab updates. Styling follows the approved Midnight direction: sans-serif headings, filled current-chapter navigation, Zeal-purple primary buttons, compact horizontal workbench callout, bordered code panels, and theme-aware terminal artwork. The supplied official SVG is used unchanged for branding and the favicon.

Hugo's Pascal lexer is named `objectpascal`, not `pascal`. The code-block hook and source shortcode now select that lexer; token CSS supports strings, keywords, numbers, names and comments across Pascal, Makefile, CMake, Bash and Zsh. Eight tests pass, including actual Hugo rendering for all requested languages and filename aliases, escaping, and theme switch state. Production Hugo build, 64 asset hashes, generated template checks and diff whitespace checks pass.

Browser visual verification remains unavailable because the browser access host is unavailable. Required creative and code review attempts (`book-midnight-style-v1`, `book-midnight-code-v1`) both failed before producing feedback with `usage_limit_reached`. No completed independent review is claimed.
