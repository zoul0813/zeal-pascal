---
title: A book with a computer inside
kicker: ABOUT THIS EDITION
---
This is the pilot of *Programming the Zeal with Pascal*: a practical book for programmers curious about retro computers. Lessons aim for 30–60 minutes, but there is no clock to beat.

The planned core projects are a calculator, a text adventure, a text-file search/report utility, and a personal collection catalog. Graphics and sound will be optional side lessons. Only **The Silent Lab** is included in this pilot.

## Your files

Save your own programs in **`H:/work/`**, using **S** in Pascal. These files stay in this browser at this site address. They do not follow you to another device. Clearing site data, private browsing, or storage eviction can remove them, so keep a downloaded backup. Reloading the page does not save text still in the editor.

Book examples live in `H:/book/lab/`. Before changing one, use **A (Save As)** in Pascal to give it an unused name in `H:/work/`. Save As replaces an existing filename without asking.

### Download or restore a backup

Under the emulator, **Your files** has two actions:

- **Download backup** saves a JSON file containing your saved H: files. Press **S** in Pascal first. The download does not include unsaved editor text or changes to Zeal’s tool disks.
- **Restore backup** reads that JSON file into a new folder under `H:/work/`. It leaves existing files alone. The result message gives the restored folder’s path; use **W** in Pascal to open a file there.

### Update Zeal

Open **More options → Update Zeal** to install the latest Pascal tools, startup files, and operating system. Zeal checks for updates after starting and shows a notice when it finds one. You can also choose Update Zeal without waiting for a notice.

Updating restarts Zeal and replaces its tool and system disks (T:, B:, A:) together. Any files or settings changed on those disks are lost. **Your H: files are kept**, but unsaved editor text is lost. Save personal programs to `H:/work/` first. The backup download covers H: only.

### Refresh lesson examples

Choose **More options → Refresh lesson examples** if you want clean copies of the book’s examples. After confirmation, it replaces only `HELLO.PAS`, `START.PAS`, and `SOLUTION.PAS` in `H:/book/lab/`. Save any changes you want to keep under a new `H:/work/` name first.

A notice that examples differ can mean you edited them, or the published versions changed. Refreshing is optional. It does not change your other H: files or the source currently open in the editor; reopen an example in Pascal to see its new contents.

### Start over

Choose **More options → Erase and start over…** for a completely fresh book workspace. Confirm the warning and type `RESET`.

This permanently deletes all this book’s H: files, including your programs, all saved tool disks, and unsaved editor text. Other projects are not affected. The page reloads; the next **Open Zeal** installs fresh tools and lesson examples.

Save and choose **Download backup** first if you want to keep your programs. The backup does not include tool disks. If erasing reports a failure, retry before starting again.

### Older workbenches and storage details

If an older workbench saved your work on T:, open `T:/src/pascal/UNTITLED.pas` (if present) and any other personal T: sources with **W** in Pascal. Use **A (Save As)** to copy each to an unused `H:/work/` filename before updating Zeal.

Current startup files put the shell on H:. If yours still starts on T:, **Update Zeal** installs the current startup files too. Pascal remains installed at `T:/apps/zpascal.bin`, with its library and units in `T:/lib/`. If an older installation left `H:/lib/zpascal.zpl`, launch with `zpascal T:/lib/zpascal.zpl` to select the book’s library explicitly while staying on H:. You do not need to delete your files.

Browser storage uses OPFS (Origin Private File System), separate from Local Storage and IndexedDB. H: lives in `zeal-pascal-book`; tool disks live in `zeal-native/<book namespace hash>/`. Chapters share the same workspace. Completed writes persist automatically; there is no separate Save Data step. Editor RAM is not a saved file. A localhost preview and the published site have separate storage. There is no account or cloud synchronization.

Use one workbench tab at a time. A second tab is prevented from opening the same workspace concurrently. The workbench needs a current desktop browser with private filesystem support; mobile/touch programming is outside this pilot’s scope.

## Keyboard and mouse

The screen overlay appears whenever Zeal is not receiving keyboard input. Click it, or press **Ctrl+Shift+Space**, to focus Zeal. Press the same shortcut again to return to the book. The keyboard indicator below the screen shows which side has focus. This toggle works while the workbench is open.

**Enable mouse input** is off by default. With it off, scrolling stays with the page or workbench, and clicking the screen only focuses the keyboard. Enable it when using software that needs a mouse.

## Credits and sources

- [Download all lesson examples]({{< relref "/" >}}code/book-code.zip).
- [Bundled software notices]({{< relref "/" >}}media/THIRD_PARTY_NOTICES.md).
- [Zeal Pascal](https://github.com/zoul0813/zeal-pascal): native IDE, compiler, and runtime.
- [Zeal NativeEmulator](https://github.com/Zeal8bit/Zeal-NativeEmulator): the native emulator compiled to WebAssembly, as used by ZShell.
- [Zeal Software Collection](https://github.com/Zeal8bit/Zeal-Software-Collection): the community TF image and its tools.
- [ZShell](https://github.com/zoul0813/zeal-zshell): the shell used to start your session.

Bundled programs retain their existing licenses. Build provenance and asset hashes are shipped with the site. No new license for the book text is asserted by this pilot.
