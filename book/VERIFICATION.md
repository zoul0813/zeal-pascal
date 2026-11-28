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
8. Verify Ctrl+Shift+Space releases focus to the book and Ctrl-K reaches the IDE rather than browser search. Verify normal page Tab navigation works outside the canvas.
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

## Disk/starter update flow and T: installation (November 14)

The screen now precedes status messages, and their fixed minimum height is removed. Readers can check and retrieve selected TF, EEPROM and boot-ROM images through the unchanged vendor API, with replacement/restart and old T:/src/pascal/UNTITLED.pas migration warnings. TF+EEPROM are selected by default for migration. H: files are kept. Published starter refreshes fetch all inputs first and replace only HELLO.PAS, START.PAS and SOLUTION.PAS under H:/book/lab after confirmation. Other H: files, legacy system folders and older lesson folders are not deleted or overwritten. Lesson instructions require personal H:/work/ copies before editing.

The original community TF is retained as community-tf.img. A book TF is repacked through the installed zdt CLI with all original regular files and directories preserved and exactly eight Pascal installation files at /apps/zpascal.bin and /lib/. The installed ZDE container reported image tf stage as unknown, so the supported disk-tool CLI was used instead. Both image contents and source hashes are checked independently. The EEPROM uses the community PATH and T: cwd; new H: workspaces receive no compiler or library files.

Ten Node tests pass, including actual vendor-wrapper update cancellation, selected-image replacement/restart, failed staging, combined commit/restart failure followed by successful retry on the same store, failed update checks, and personal/legacy H: preservation. Production Hugo build, generated status ordering and fingerprinted module-dependency checks, 65 asset hashes, exact EEPROM contents, and TF file/directory equivalence pass.

The isolated native run at build/book-native/20261114T050556.162860Z-83467/report.json passed: bare zpascal launches from T:, all three lesson sources compile into H:/work/, and a saved H: source is reopened in a second run and saved again byte-identically. TF and EEPROM hashes from this run match the current distributable images. Old installed emulator binaries could not load their removed Raylib filename, so an unchanged NativeEmulator checkout was built in the ignored book build directory against existing installed Raylib. No emulator source or vendor JS was changed. Earlier failed test attempts remain in ignored build logs.

Restricted Opus code review book-storage-updates-v1 completed two turns with all initial findings resolved and no blocking/high/medium findings remaining. Non-blocking omissions: no independent browser OPFS/layout test, no commit-failure-only integration test separate from combined restart failure, and unchanged legacy export filtering lacks a new integration test. A deliberate TF-only update with an old custom EEPROM can retain the old compiler selection; migration instructions and default selections specify updating both.

## Pilot manuscript revision and H: startup (November 14)

The pilot now separates HELLO.PAS and LAB.PAS, offers scratch/starter routes before actions, puts the playable walkthrough before the visible START listing and code tour, explains the missing Pascal constructs, and supplies explicit pause/resume and backup steps. Personal work always goes under H:/work/; starter files remain under H:/book/lab/. Save As replacement is explicitly warned about. The Pascal examples are unchanged.

The user changed autoexec.zs to cd H:/. The EEPROM was rebuilt using the existing zdt CLI; only its image and authoring provenance hashes changed. The TF remains byte-identical. The native check at build/book-pilot-ide/20261114T150841.389267Z-32213/report.json passed bare zpascal launch with this EEPROM, Memory execution, Enter menu redraw, Ctrl-K D editor return, and byte-identical H:/work/ saves, including replacement of an existing file without confirmation. Earlier room-state walkthrough evidence is in build/book-state-playthrough-report.json; it checks movement and inspect state, not rendered text or full quit completion.

Ten Node tests, 65 asset hashes and EEPROM content checks, project-path Hugo generation, local anchor checks, and visible START listing placement pass. Native filesystem checks use isolated host directories, not browser OPFS. Actual browser persistence and visual layout remain for a browser pass. Existing readers must retrieve the updated EEPROM through Disk images & updates; save personal work first, because retrieval restarts the emulator. H: files are retained.

## Focus, mouse opt-in and full book reset (November 14)

The screen has an unfocused overlay and a keyboard-state indicator. Ctrl+Shift+Space toggles between the lesson and canvas; F8 was replaced because Chrome DevTools uses it. The book tracks keys delivered to its canvas and dispatches matching keyup events before focus leaves, so the native canvas cannot retain held modifiers. Space repeats remain suppressed until release. Return focus stays inside the lesson, with a focusable main fallback. The overlay has an inset visible keyboard-focus outline.

Enable mouse input is unchecked by default and gates canvas pointer events through CSS. The screen wrapper still focuses the keyboard on click. Native mouse handling and wheel capture become available only after explicit opt-in; the choice is not persisted across page reloads. No vendor code or emulator source was changed.

Reset book storage requires confirmation and typing RESET. It acquires book ownership, exits the native runtime and closes the image store, takes the native image namespace lock, removes the complete book disk namespace (including orphan files) and the zeal-pascal-book H: tree, then reloads. Other projects and origin folders remain intact. Missing data is accepted; genuine deletion failures are surfaced, with partial-removal wording only after deletion has started. Failed native teardown prevents deletion. Reset remains available after startup/storage failure. A complete reset is deliberately destructive; normal disk/starter updates retain their existing H: preservation behavior.

Automated checks cover shortcut focus transitions and repeat handling, modifier release events, hidden return targets, browser/tab focus changes, mouse defaults/toggle, reset cancellation and typed-confirmation mismatch, native shutdown/store-close failures, lock refusal, deletion errors and retry, unrelated project preservation, real bundled DiskImageStore namespace parity and locking, and corrupt-manifest recovery. Thirteen Node tests pass. Asset checks and a production GitHub Pages path build pass.

A native Chrome UI pass on a separate preview origin confirmed real WASM boot on H:, the overlay appearing on blur, Ctrl+Shift+Space toggling out and back, and mouse input initially unchecked and explicitly togglable. The initial modifier-keyup-delayed shortcut did not reliably transition through the UI input tool; immediate toggling with held-key release fixed that observed failure. Native UI scrolling could not be independently assessed (the scroll tool reported windowNotFoundAtPosition). The destructive reset/reload was not executed on browser data: automatic approval review rejected a reload after Chrome's unsaved-state warning, so subsequent checks used a separate origin. OPFS reset correctness is covered by mock filesystem/native-wrapper tests and real store-lock/manifest logic, not an end-to-end destructive browser test. Windows/Linux OS shortcut interactions and native guest mouse movement remain untested.

## Workbench management simplification (November 14)

The default panel now has a compact launcher/save-path reminder and Your files with Download backup / Restore backup. One collapsed More options disclosure contains Update Zeal, Refresh lesson examples, and a separated Erase and start over action. Removed the per-image checkboxes, separate manual check, redundant full-code seeding button (this pilot already seeds the complete single-lesson set), folder list, namespace path and DevTools snippet. Source ZIP, notices, migration steps and OPFS details remain in About help. The pilot and help use the same action names.

Update Zeal calls the existing unchanged native API with romdisk/eeprom/tf together. Its confirmation explicitly warns about complete T:/B:/A: replacement, custom files/settings loss, restart and unsaved editor loss, and H: preservation. A background availability notice does not auto-open More options. Updates can be requested even if HEAD-based availability checks are unavailable; no unverified “up to date” claim is shown. Example byte differences are described as divergence rather than a published update. Refresh still replaces only the three managed examples. Reset scope, locking and typed confirmation are unchanged.

The update live region remains rendered but empty at idle, with zero empty margins; a screen reader can observe subsequent availability text without unhiding a display:none region. Operation/storage failures now use the existing main status region, keeping them visible without a second alarming storage banner. The optional example-divergence paragraph is ordinary text inside More options; refresh completion is announced by the main status region.

Thirteen Node tests pass, including the real native-wrapper all-image update path, cancellation and failures, H: preservation, full backup-download payload and restore-folder checks, and existing focus/reset/real-store tests. The workbench stub now derives IDs from the actual partial, so dangling selectors fail. Tests cover a quiet update state, notice state, failed-download messaging, example divergence and clearing it after refresh. Sixty-five asset/provenance checks, production Hugo project-path generation, rendered control-placement/internal-link checks, and whitespace checks pass. The generated About source ZIP/notices URLs resolve under /zeal-pascal/book/; ZIP still contains the three lab Pascal examples.

Native Chrome UI verification on a separate preview origin observed real WASM startup, only backup controls outside More options, and the three maintenance actions after explicit expansion. A screenshot confirmed the visual hierarchy. No browser backup import, disk update, example replacement, erase, or reload was executed. Data-changing paths were tested in isolated mocks/real-store logic; real screen-reader announcement behavior has not been independently exercised. Two creative consultation turns were reconciled and their final copy corrections applied; no immutable-input creative finalization is claimed after those corrections. The final implementation is supplied to the separate technical code review.

## Emulator built by the site workflow — November 16

The site workflow follows `zoul0813/zeal-zshell`: checkout NativeEmulator `main`, set up Emscripten 7.0.0, build upstream Raylib with its supported helper, and compile the minimal WASM template with Meson. `prepare.py --emulator-only` stages the build and generates `static/emulator/provenance.json` with its source commit, compiler metadata, artifact hashes and license origins. The generated emulator directory is ignored by Git. Disk and Pascal asset provenance is stored in `static/provenance.json`.

An isolated NativeEmulator clone at `6a0c61032a6f778a231193434d8547efad182ef7` built successfully with the installed Emscripten 7.0.0 toolchain. A fresh source copy containing no emulator bundle passed staging, three Python tests, thirteen Node tests using the generated upstream helpers, sixty-four asset checks, and the Hugo project-path build. Output checks confirmed the runtime files and source-commit version in the assembled site. The workflow YAML parses. Build logs are in `analysis/book-wasm-ci/`.

A real-browser startup check encountered an unavailable computer-use connection. GitHub-hosted execution awaits a workflow run after these changes are published.

## Course expansion — November 25

The manuscript now includes the calculator, expanded adventure, text search and typed catalog, plus the four screen, graphics, sound and game side trips and a workbench reference. Two Opus creative turns edited the manuscript and About. Two read-only technical turns reviewed the Pascal examples and lesson-file packaging. The review records retain the graphics and audio qualification gaps.

The production-path Hugo build, internal navigation and anchors, 13 Node cases, four Python cases, and the 64 packaged software asset checks passed. The workbench inventory and source ZIP agree for all 16 managed files. The text search fixture and the graphics include are staged as support files beside the Pascal sources.

The original packaged compiler and OS produced native COMs for TOTAL, CALC, REWIRE, SEARCH, CATALOG, PANEL, and CATCH. Chrome with the book's staged SDL3 WASM ran the calculator's overflow and divide-by-zero refusals, memory recall, recent results and rounded display, plus the game's loss, restart, catch and quit. The expanded adventure worked through the hatch refusal and both pickups to the repaired beacon. The status panel changed its selected row and restored the console's text rendition on exit.

The exact compiled text-search program returned matching line numbers for CRLF input with a 290-character line prefix and an unterminated final line. Its report included the runtime's Ctrl-Z terminator and the input remained byte-identical. The exact compiled catalog created its file, added two records, edited one, removed the other, exported the remaining record and reopened the catalog. Repeat report and export attempts preserved the existing outputs.

MAPVIEW and TONES have authored source, supplied dependencies, and instructions. Extended standalone tests using the staged libraries did not reach a finished COM within the tested cycle budget. A disposable TF assembled with the newer local compiler build and project-local copies of the original ZVB unit also failed to finish within their bounded runs. Both lessons are marked as workbench previews. The remaining checks are the compiled examples, the map's rendered scene and text restoration, and audible playback followed by silence. Reader times are estimates.

## Bin-file naming in the book workbench

With the compiler built from `1d57949`, the native verifier booted private copies of the book disks and launched Pascal from ZShell. The three pilot starters compiled to `hello.bin`, `start.bin`, and `solution.bin` under `H:/work/`. Selecting `UpperCase.COM` in Options produced the exact requested spelling and the same bytes as `solution.bin`. A second boot reopened the saved `lab.pas` source and saved a byte-identical copy.
