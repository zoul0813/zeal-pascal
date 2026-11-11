---
title: A book with a computer inside
kicker: ABOUT THIS EDITION
---
This is the pilot of *Programming the Zeal with Pascal*: a practical book for programmers curious about retro computers. Lessons aim for 30–60 minutes, but there is no clock to beat.

The planned core projects are a calculator, a text adventure, a text-file search/report utility, and a personal collection catalog. Graphics and sound will be optional side lessons. Only **The Silent Lab** is included in this pilot.

## Your files

The workbench uses the native WASM Zeal emulator's unmodified HostFS backend. H: points to a private browser filesystem (OPFS). Press **S in Pascal** to save there; a page refresh does not save editor RAM. Browser site-data clearing, private browsing, and storage eviction can remove files. Download a workspace backup when you stop.

The existing emulator's disk-image store uses **IndexedDB**, not localStorage. The pilot keeps source on HostFS and boots the original community TF image on each new session. Changes made to T: are session-only; use H: for your own files. There is no account, cloud synchronization, grading, or telemetry.

Use one workbench tab at a time. The site prevents a second workbench from opening the same workspace concurrently where Web Locks is supported. The emulator supports desktop keyboards; mobile/touch programming is outside this pilot's scope.

## Credits and sources

- [Zeal Pascal](https://github.com/zoul0813/zeal-pascal): native IDE, compiler, and runtime.
- [Zeal NativeEmulator](https://github.com/Zeal8bit/Zeal-NativeEmulator): the native emulator compiled to WebAssembly, as used by ZShell.
- [Zeal Software Collection](https://github.com/Zeal8bit/Zeal-Software-Collection): the community TF image and its tools.
- [ZShell](https://github.com/zoul0813/zeal-zshell): the shell used to start your session.

Bundled programs retain their existing licenses. The workbench's **Files, backups & fresh starts** section links to the included third-party notices. Build provenance and asset hashes are shipped with the site. No new license for the book text is asserted by this pilot.
