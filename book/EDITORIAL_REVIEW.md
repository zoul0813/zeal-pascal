# Book editorial and technical review

Reviewed November 14, 2026, against `ddd823c`. This is a review and proposed development plan, not a revised manuscript. No lesson text or Pascal source was changed during the review.

## Scope and verdict

Only **The Silent Lab** exists as a lesson. The remaining projects are an outline, not chapters whose prose or finished programs can be approved. The home page, About page, lesson, three Pascal sources, existing project examples and current language/IDE references informed this review. Opus supplied separate editorial and technical assessments; Codex checked their claims against the sources and recorded the limits below.

The pilot has a sound premise and a useful test-driven exercise. Keep its small abandoned-lab setting, concrete commands and restrained humor. The main revision is teaching flow: simplify first launch, make the entry routes converge, let readers play before explaining the listing, and separate learning Pascal from maintaining the emulator workspace.

The next manuscript pass should improve the pilot before adding chapters. Once the project sequence is approved, align the sidebar, home page, About and pilot ending: they currently disagree about whether the future adventure is part of the lineup. It should not turn the book into a reference manual or expand the fiction into a story readers must follow to use the projects.

## Pilot changes, in priority order

### 1. Make the file routes agree

The scratch route currently creates `H:/work/LAB.PAS` for the greeting; the supplied route creates `H:/work/HELLO.PAS`. The next section reuses LAB or suggests ADVENT, and the resume section has to hedge with “or your chosen filename.” It also introduces an avoidable overwrite decision.

Use a consistent destination for each result:

| Result | Personal filename |
| --- | --- |
| First greeting, either entry route | `H:/work/HELLO.PAS` |
| Reader's adventure | `H:/work/LAB.PAS` |
| Optional personal copy of the solution | `H:/work/SOLVED.PAS` |

Offer “type it” or “open the supplied copy” **before** instructing the reader to press E. Both routes must reach the same filename, saved state and next step. W chooses or loads a Work file; S saves it; A saves its text under another name. Do not imply that selecting a missing path has already created a saved file.

Keep the essential rule close to the first file operation: book starters belong to the book; save a personal H:/work copy before editing. Keep these flat pilot paths as a deliberate first-chapter exception, so current readers can resume existing files. For later projects, recommend separate folders such as H:/work/calc/ and H:/work/catalog/. Do not rename or migrate existing personal files merely to enforce the convention.

### 2. Shorten the path to the first successful run

The paragraph between `zpascal` and the Y prompt currently teaches EEPROM, drive letters, PATH, the library location, current directory and legacy migration. A first-time reader does not need those details yet.

Keep the launch action, expected screen and reason for choosing Y. Put recovery in a short optional note. Keep migration warnings and disk replacement procedures in the workbench's update section and About; do not weaken or remove their safety instructions.

The recovery note should distinguish an incorrect current directory from a missing/outdated installation. `cd T:/` alone cannot install Pascal on an old TF image or fix an old EEPROM PATH.

### 3. Run the adventure before touring the code

The current leap is from a tiny greeting to a much longer program, followed by several new constructs. The complete START listing is collapsed while the prose discusses fragments of it.

Recommended sequence:

1. Open START and Save As LAB.
2. Run the supplied map/walkthrough table and see the working adventure.
3. Read the visible, highlighted listing in program order.
4. Make one small description change and rerun it.
5. Add `inspect`, with hints and the worked solution still behind reveals.

A complete subject-of-the-lesson listing should be visible. Hints, alternate complete solutions and optional detail can remain collapsed. Long listings should have a clearly offered starter route; typing is a choice, not proof of understanding.

The code tour needs brief explanations of `else if`, `<>`, the parenthesized comparisons joined by `and`, `Write` versus `WriteLn`, and the blank `WriteLn;`. Put the semicolon-before-`else` warning beside the first edit that needs it, rather than only inside a hint.

### 4. Give the lesson clear pauses and a clean ending

Add explicit saved stopping points after the greeting and after the walkthrough. Each should name the saved file and link to the resume steps. Fix the introduction's reference to the “final section,” since the resume instructions precede the closing section.

Keep the final section about what the reader can now do and an optional next experiment. The disk-update paragraph currently placed after the pilot ending should move to the existing operational help.

### 5. Keep storage advice short, consistent and findable

The main lesson needs these rules:

- Save personal copies under H:/work, outside the book-owned starter files.
- Running or compiling is not saving source.
- Reloading loses unsaved editor RAM.
- Save with S, then use Download H: files for an outside-browser backup.
- Clearing browser data can remove saved files.

Drive-image persistence internals, namespace mechanics, legacy migration, image retrieval, bulk starter loading and import details belong in the workbench help or About. Short reminders at risky actions are still appropriate; do not remove a necessary warning merely to avoid repetition.

A useful prose distinction is **Zeal** for the emulated machine and **workbench** for the browser panel. Use exact UI labels only when directing an action.

### 6. Verify the remaining instruction-level ambiguities

Before revising the operational steps, check the exact shipped IDE behavior for:

- The successful Memory-run return prompt/key. Current reference and example READMEs disagree; “follow the on-screen return prompt” is too vague.
- Whether A (Save As) asks before replacing an existing file. The lesson must not promise a confirmation that has not been verified.
- The default compilation mode on a fresh session and the recovery path for a reader who changed it.
- Ctrl-Z and Ctrl-C at the adventure prompt, and input longer than 20 characters.

These are verification items, not established defects in the three Pascal programs. Avoiding filename collisions removes the normal lesson's dependence on overwrite behavior, but does not settle the application's contract.

## Voice guide

Use a friendly, precise second-person voice. Usually explain a goal, give the action, then say what should happen. Treat the reader as capable without assuming Pascal vocabulary.

Let humor come from the lab, program output or a brief transition after a result. Do not put jokes in storage warnings, recovery steps or key sequences. Keep the existing dry touches—“No files are harmed by a syntax error” and the suspicious DO NOT SWITCH OFF label—rather than adding a joke quota. The situation can be amusing; a reader's confusion should not be the joke.

Examples for the revision pass:

| Current pattern | Suggested direction |
| --- | --- |
| Dense B:/T:/PATH paragraph before Y | “Pascal is already installed. At the message prompt, press Y to see explanatory compiler messages.” Put troubleshooting behind a short note. |
| Route choice after the instruction to press E | “Choose how to start: type the short program, or open the supplied copy. Either way, save your greeting as H:/work/HELLO.PAS.” |
| “Press S in Pascal” | “In the Pascal menu, press S to save.” |
| Explain every detail before running START | “First, take a walk through the lab. Then we'll look at how it remembers where you are.” |
| Operational paragraph after the ending | Finish with the reader's working adventure and an optional experiment; link to maintenance help elsewhere. |

Two examples of acceptable light-touch, non-instruction prose:

> A greeting says the same thing forever, which is reassuring in a lab and dull in a game.

> The amber light is still blinking. What it is keeping running can wait until you have saved your work.

These illustrate the register, not lines that must be inserted or a target number of jokes.

Do not silently change the solution's exit wording when adding the amber light. A minimal solution diff makes comparison easier. This is a proposed source-text refinement, not a correctness fix.

## Recommended progression for the rest of the book

This is an outline recommendation. Every implementation and lesson split below still needs authoring and validation. Optional prerequisites should be supplied in complete checkpoints; no project should require a reader's earlier personal program to be present.

| Section | Useful finished result | New Pascal learning | Practical Zeal learning | Suggested lesson splits and acceptance |
| --- | --- | --- | --- | --- |
| First contact: The Silent Lab | A runnable three-room adventure with an extra command | Program structure, variables, bounded strings, assignment, decisions, a repeat loop | IDE launch/edit/save/run; H: personal copies; one error-and-repair cycle | Greeting → walk/test the adventure → explain/change it. Both entry routes converge; all moves, refusals, look, inspect and quit behave as specified. |
| Short foundations bridge | A conversion-table utility and small reusable input routines | Named constants, Integer arithmetic, loops, procedures/functions, value versus `var` parameters, validation with `Val` | Compiler/runtime error location; limits that affect actual code | At most two short lessons: consolidate the pilot's concepts, then factor and validate input. Bad input must return to a useful prompt. A guessing game can be optional practice rather than another required project. |
| Calculator | A reusable accumulator calculator with memory and a visible in-memory calculation history | Numeric validation, decomposition, formatting, then Real arithmetic; a bounded one-dimensional history array in the final lesson | Build and launch a standalone COM from an explicit H: path | Running total with `+ - *` → decimals and guarded `/` → memory/history and saved executable. Invalid input and operations outside the selected numeric limits are refused before changing the total. Test Integer addition/multiplication overflow with pre-checks; do not wait for a wraparound error. Integer quotient work, if included, uses `div` explicitly. No recursive expression parser in the early chapter. |
| The Lab, Rewired | A larger playable adventure with a data-driven map, objects and a goal | Enums, enum-indexed/2-D arrays building on the calculator array, records, sets, command parsing and separation of state from behavior | Only platform details needed by the program | Refactor the existing behavior → map/object data → puzzle and complete walkthrough. Supply a complete starting checkpoint. Adding rooms changes declarations/data, without adding movement branches. This revisits the lab to teach a different architecture, not to repeat the pilot. |
| Text search and reports | A tool that searches text and writes a useful report | Text files, streaming/line handling, string matching, counters and recoverable I/O | Absolute H: data paths, file lifecycle, standalone invocation | Read and count → defined search behavior → report output. Specify substring versus whole-word matching. Test zero matches, long lines, LF/CRLF inputs, missing files and refused overwrites; automated checks verify the input remains byte-identical. |
| Personal collection catalog | Add, list, search and edit a small collection; retain it after restart | Apply records/arrays, then typed files, random access, logical deletion and export | Flush/Close, H: backups, schema and capacity limits | In-memory catalog → persistence → search/edit → deletion/export/backup. Offer creation only for the pinned build's verified missing-file status; other failed opens must never trigger destructive recreation. A record edit changes only its target. “Database” means a typed file of records, not SQL or a transactional storage engine. |
| Optional screen and input side trip | A usable text dashboard or improved adventure display | Separating rendering, commands and application state | CRT lifecycle, colors, cursor placement and non-blocking keys | Verify setup/cleanup on normal exit and document failure limitations; the prompt remains usable afterward. |
| Optional graphics side trip | A map viewer or small plotting program | Arrays as graphical data; coordinates and drawing routines | ZVB mode/palette/tile or sprite setup and restoration | A complete scene renders, accepts input and restores the shell display. Use only APIs verified in shipped units. |
| Optional sound side trip | A small event-sound or note player | Sequences, timing and reusable routines | ZVB audio and browser audio activation | Sound can be enabled by the reader; cleanup leaves no stuck tone. A file-driven tune extension can follow the file project. |
| Optional real-time game | A small catch/dodge game using the earlier graphics/input work | An update loop, collision/state transitions, bounded resource use | Frame pacing, keyboard polling and video/audio lifecycle | Play, lose, restart and quit all work. Label emulator-only evidence separately from physical hardware qualification. |
| Compact reference | A quick route back to a working program | Constructs already taught, common mistakes and examples | Editor keys, compile modes, file locations and recovery | Link from lessons to the relevant entry; keep the reference synchronized with the shipped compiler. |

### Entry points and optional extensions

Checkpoints supply working code; they do not silently supply understanding. A reader joining late should follow the named concepts or their compact reference links first.

| Section | Assumes / earliest entry | Optional extensions |
| --- | --- | --- |
| Pilot | Recognizes variables and loops; no Pascal or Zeal experience | Fourth room, an extra notice command, different descriptions |
| Foundations bridge | Can launch Pascal, select/save a personal file and run the greeting; provide a fast-start recap | Reverse conversion table, turn-limited guessing exercise if Random is verified |
| Calculator | Bridge's routines, loops and validation; bundled input helper | Percentage operator, extra memory slot; expression parsing as an advanced later exercise |
| Lab, Rewired | Pilot control flow plus bridge routines; calculator's one-dimensional array, or a short array primer | Abbreviations, scoring; save/load only after the file lessons |
| Text utility | Bridge's string/input routines and error workflow; no earlier data-file experience | Multiple search terms, word-frequency report, source-code search |
| Catalog | Records and arrays from Rewired or linked primers; text files and IOResult from the text utility | Sort on export, second search field, copy-compaction with verified file lifecycle |
| Screen/input | Bridge routines and command loops; use a supplied display demo without requiring an edited adventure | Keyboard shortcuts, status panel; optional integration with Rewired |
| Graphics | Arrays and routines; supplied scene and restoration helper | Labelled map, extra sprite, a simple plot |
| Sound | Routines and bounded sequences; file-driven tune extension additionally assumes the text utility | Event sounds, alternate instrument/sequence, file-driven notes |
| Real-time game | Graphics/input side trips; sound optional | Difficulty levels, score display, more collision shapes |
| Reference | No linear prerequisite | Use as needed; no new unintroduced core project dependency |

Each checkpoint bundles its helpers; a reader can substitute their own toolbox later. This is intentional project independence, not an unexplained collection of duplicate code.

Existing calc, concord and partsbin examples are useful technical reference implementations. They are not ready-made beginner chapters. In particular, the recursive calculator's parser/depth rules and the catalog's file lifecycle require a teaching sequence, not a pasted listing.

The optional graphics/game route must contain complete usable programs. A box-drawing demo alone would not support the book's promise that readers can build graphics programs and games.

## Technical corrections to apply when outlining

These are constraints from the current project references, not proof that every future example works on the pinned book compiler:

1. **Integer wraparound:** `{$R+}` checks ranges/indexes; it is not a general arithmetic-overflow switch. Demonstrate and prevent overflow deliberately rather than promising an exception for `MaxInt + 1`.
2. **Division:** `/` yields Real. Do not assign it back to an Integer accumulator. Introduce it with Real, or explicitly teach `div` and truncation.
3. **Real precision:** Zeal Pascal uses a six-byte non-IEEE Real format. Choose a precision demonstration after running it; do not import a JavaScript/IEEE `0.1 + 0.2` anecdote or ban all Real equality comparisons indiscriminately.
4. **Input guards:** Boolean expressions are eager. Use nested checks before indexing strings, dereferencing or dividing; `and` does not make unsafe access conditional. Check Length before Line[1]: capacity range checking does not mean the current string contains a first character. Handle empty numeric input explicitly; per the reference, Val on an empty string can succeed without changing the destination, so Code = 0 alone is not validation. Decide and test the whitespace policy.
5. **Declarations and reusable code:** Ordinary `const` declarations are not arbitrary compile-time expressions. Keep the first shared helpers local or in a tested include. Current references restrict console/Text/file I/O in user unit bodies, so do not promise an input toolbox unit. Introduce units after explaining and verifying their build/use contract. String formals use named types; var String capacities must match under the default checking mode. Bundle required helpers with each checkpoint.
6. **Game state:** Typed constants are writable and can retain mutations between Memory runs. Initialize mutable state at program entry and keep map constants read-only by convention. Verify enum printing; a name table is safer than assuming an enum prints its label.
7. **Text:** A bounded `ReadLn` can discard a long line's tail. Define limits or stream the input so searching/counting does not silently omit data. Reports intended for the IDE must respect its 127-byte physical-line limit.
8. **Files:** Runtime file operations do not create parent directories merely because the IDE can do so while saving source. Use explicit H:/work/project paths, created as part of the checkpoint/source setup. Opening a source does not change cwd, nor does launching a saved program from ZShell. Current runtime data-path limits are 15 bytes per component and 127 bytes overall; confirm these on the pinned assets and keep project folder names short.
9. **Persistence safety:** Retrieve `IOResult` before further I/O, and use Flush where its typed-file contract applies plus explicit Close on every normal exit path. Program termination, Ctrl-C and Halt are not substitutes for file finalization. A reported write error is not a rollback: bytes may already have changed, so never blindly replay a write. Do not claim atomic Rename, transactions, crash-safe in-place updates or portable typed-file interchange. A schema change needs a migration plan as well as a backup. Typed Reset checks component size, not a complete schema: an incompatible same-size record change can still be accepted and misinterpreted.
10. **Platform libraries:** CRT, ZOS and ZVB are documented and their unit assets are bundled, but each selected interface/behavior must be validated against the compiler and unit versions actually shipped. CRT is not the source of the ZVB sound API. Test the restoration helper (such as VIDEO.INC) as part of the supplied checkpoint; its presence in the project repository does not mean it ships with the book. Require a reader-visible restart/recovery path after abnormal exits that cannot run cleanup, and do not promise automatic video/audio restoration. Larger graphics projects may need saved Com-file mode instead of Memory mode. Physical hardware and mobile typing support must not be implied by browser/native tests.

## Evidence and remaining verification

- The exact HELLO, START and SOLUTION sources compiled in the prior native verification. The book TF launches Pascal from T:, and an H: source was saved, reopened and saved again byte-identically (`build/book-native/20261114T050556.162860Z-83467/report.json`).
- Static technical review finds the room transitions, unknown-command check, look/quit behavior and inspect branches consistent with the chapter's stated requirements. This is source analysis.
- A new isolated native state playthrough (`build/book-state-playthrough-report.json`) launched the exact previously compiled START and SOLUTION COMs from absolute H: paths. The matching source copies were checked against the shipped PAS files. Real keyboard input and authenticated runtime input checkpoints verified the expected room states for look, every move in the walkthrough, an invalid move, unknown/blank/uppercase commands, and inspect in all three solution rooms. Quit was submitted. This proves those state transitions and H: COM execution, not every printed message, quit completion, the Memory-run IDE return prompt, browser behavior or physical hardware. Earlier output-observer attempts were inconclusive and are not counted as passing transcripts.
- The build/update milestone passed ten Node tests, asset/TF/EEPROM checks and the Hugo build. These do not establish human reading pace or browser keyboard/layout usability.
- Current docs and example READMEs may be ahead of the book's pinned compiler. Future chapters must be tested against the distributed executable/library/units, not merely compiled against main.
- Before a second project is published, generalize the starter catalog/update mechanism beyond its three hardcoded lab files, preserving the same book-owned-versus-personal-path boundary. This capability is not currently implemented.
- Outstanding gates include exact IDE run-return/overwrite/interrupt behavior, chosen numeric and input examples, file edge cases and report preservation, binary catalog backup/restore, and graphics/audio cleanup on the selected browser.
- No remaining chapter has a drafted manuscript, completed project, or claimed acceptance result. Estimated lesson splits require a modest-experience reader trial.

## Decisions recommended before drafting

Use the pilot as chapter one, then a short consolidation bridge. Revisit its setting only when teaching new data structures. Make short listings convenient to type and longer listings convenient to open; always show the reader the choice before the first action. Use per-project personal folders and complete entry checkpoints. Start with local routines/tested includes, with units as a later reusable-code topic.

These are recommended defaults for the next authoring pass, not requirements silently added to the agreed scope. The next concrete step is to revise the pilot's routes, pacing and code tour, verify its operational instructions, then draft the short foundations bridge before the calculator lessons. An earlier calculator checkpoint could be a technical probe, but not the next reader lesson.


One proposed emphasis needs an explicit author decision: making supplied code the prominent route for long listings differs from the original scratch-first planning note. Until that decision is made, preserve a clearly offered scratch route and do not treat the recommendation as permission to change the default. The remaining structural recommendations above are likewise choices for the next authoring pass, not a claim that chapters have been approved or written.
