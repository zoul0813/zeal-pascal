# Program execution pages

The native baseline measures four arithmetic programs using the production IDE
at checkpoint 062515f. The library case authors an Integer unit through the IDE,
then imports its compiled artifact. Every program is compiled and run in Memory
mode and saved for a separate cold standalone launch. Native completion,
physical image handles, actual root mappings, and kernel page ownership are
checked. Emulator evidence is bound in `program-pages-baseline.json`.

| Case | File bytes | Code bytes | Global bytes | IDE execution pages | Standalone owned pages |
| --- | ---: | ---: | ---: | ---: | ---: |
| Minimal | 16906 | 522 | 0 | 3 | 3 |
| Integer | 16988 | 602 | 2 | 3 | 3 |
| Real | 17189 | 799 | 6 | 3 | 3 |
| Library | 17159 | 775 | 0 | 3 | 3 |

Every page is 16384 bytes. Each Memory compilation initially retains two image
pages. Run allocates the third window for the stack and execution controls,
retaining all three image pages for repeated Runs. Ten compiler code pages
remain resident while the child runs. Those pages and the IDE's other storage
are additional to the measured child image.

The unmodified OS loader initially gives a standalone process three pages.
Standalone ownership is measured before process cleanup. These arithmetic
cases use no explicit file, module, or dynamic heap operations. Their counts
are entry/exit measurements, not a claim about those other operations' peaks.

The allocation change must preserve runtime/source locations, repeated Run
state, stack/heap bounds, unsupported/native feature behavior, allocation
failure cleanup, and old images. Its measurements will use the same driver.
