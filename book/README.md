# Interactive Pascal book pilot

Hugo source for **Programming the Zeal with Pascal**. The pilot is *The Silent Lab*: a three-room text adventure, taught through the actual native Pascal IDE in a browser-hosted Zeal.

## Preview

Hugo is required (tested version recorded in `VERIFICATION.md`). No Node packages or theme downloads are needed.

```sh
cd book
hugo server --bind 127.0.0.1 --port 1313 --baseURL http://localhost:1313/
```

Open <http://localhost:1313/> in desktop Chrome or Edge. Open the pilot lesson, choose **Open Zeal**; it starts automatically. At ZShell:

```text
zpascal
```

Book-owned starter source lives in `H:/book/lab`. Save personal versions as new files under `H:/work` before editing; the starter-update button can replace only the published starter files. H: points to OPFS and is not replaced by disk-image updates. Existing legacy H:/bin, H:/lib and old lesson folders are left intact, but a new workspace installs no system files on H:.

Pascal is installed at `T:/apps/zpascal.bin`; its library, units and notices are in `T:/lib/`. The book TF image retains every community file byte-for-byte and adds the Pascal installation. EEPROM sets the community PATH and starts in T:/ so bare `zpascal` resolves its library correctly. Existing saved disks are never automatically replaced. To migrate an older workbench, first use W and A (Save As) to copy T:/src/pascal/UNTITLED.pas (if present) and other personal T: sources to new H:/work/ files, then retrieve TF and EEPROM together using **Disk images & updates**. Replacing selected disk images restarts the guest and discards unsaved RAM; H: is kept. The IDE's default T: source filename still exists as application behavior; the book always instructs readers to select H:/work/ for personal work.

Status messages follow the fixed-aspect screen, with natural height. Image notifications link readers to the selection controls. Separate starter refreshes fetch current source files with cache revalidation and update H:/book/lab after confirmation. Personal files are preserved; this does not replace a downloaded backup.

## Build and check

```sh
python3 scripts/prepare.py --downloads-only
node --test tests/*.test.mjs
python3 scripts/check.py
# Requires existing ZDE-built native compiler maps:
python3 scripts/verify-native.py --compiler-build ../build
hugo --minify --baseURL https://zoul0813.github.io/zeal-pascal/book/
```

The **Publish site** GitHub Actions workflow checks and publishes every push to `main` (including merges) and `codex/next-work` (for testing). Both branches publish to the same site URL. Pull requests affecting the book or workflow build and check without deploying. A manual run publishes by default; clear its `deploy` input for an artifact-only build. Configure the repository's **Settings → Pages → Source** to **GitHub Actions** before the first deployment. If the `github-pages` environment restricts deployment branches, allow both branches there as well.

The public book URL is <https://zoul0813.github.io/zeal-pascal/book/>. The workflow assembles one complete Pages artifact in `build/pages`, with Hugo output in `build/pages/book`. The site root is reserved for future content; it currently has no landing page. Add future root content to the same staging directory and workflow, keeping `book/` reserved for Hugo. Pages deployments replace the whole site: a separate root deployment would remove the book, and a separate book-only deployment would remove root content. Extend the pull-request path filters when adding new site sources.

The book's disk namespace follows its base URL path. Moving from `/zeal-pascal/` to `/zeal-pascal/book/` selects a new TF/EEPROM workspace; old disk images are retained in OPFS but are not migrated automatically. H: keeps its existing workspace. Local previews use an explicit localhost base URL.

## Prepared assets

`static/emulator/` contains an unchanged installed native WASM bundle and matching HostFS/library files. `static/media/` contains an unmodified community 2 MiB TF base plus a repacked book TF image with Pascal installed, a compatible installed ZDE ZShell boot ROM, community licenses/notices, a book EEPROM generated from `eeprom/autoexec.zs` and `eeprom/welcome.txt`, and matching Pascal build outputs. `static/provenance.json` binds staged assets to SHA-256 hashes and origins. These small prepared assets are included in the book source so an ordinary Hugo build works without ZDE or private local directories.

To refresh assets from installed, verified builds:

```sh
python3 scripts/prepare.py \
  --wasm-dir /path/to/native/wasm/bundle \
  --emulator-source /path/to/Zeal-NativeEmulator \
  --community-dir /path/to/Zeal-Software-Collection/dist/community \
  --pascal-build /path/to/zeal-pascal/build \
  --boot-rom /path/to/zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img \
  --eeprom-tool /path/to/existing/zdt
```

The WASM directory must contain `zeal.elf.js`, `zeal.elf.wasm`, and matching `libnative/indexeddb.js`, `libnative/disk-images.js`, `libnative/hostfs.js`, and `libnative/zeal-native.js`. Use NativeEmulator `feature/wasm-opfs` at commit `30cb5b6` or a compatible later build. The files are copied unchanged; the NativeEmulator checkout must be clean. The existing Zeal Disk Tool CLI builds a raw 64 KiB ZealFS v2 EEPROM and its contents are independently verified. Build Zeal Pascal through `zde cmake`; do not substitute a host compiler. The original community TF is retained as the overlay input; the book TF is repacked with Pascal installed in /apps/ and /lib/. Its bundled ROM did not launch the current compiler in native checks; the staged compatible ZDE ROM did. Supply a matching supported ZShell boot ROM when refreshing. Inspect provenance, licenses, and checks after refresh. Asset refresh is a maintainer operation, never a reader flow.

`static/code/lab/*.PAS` is the single source for printed listings, served lesson files, and reproducible `book-code.zip`. Never edit copies of those listings into the chapter.

## Workspace rules

- The OPFS directory `zeal-pascal-book` is private to the site's origin. A Web Lock prevents concurrent emulator tabs from modifying it.
- System files are installed only when absent. User files are never replaced by opening a lesson.
- Fresh lesson copies receive new directories. Imports receive new directories and validate all paths/data before writing.
- Workspace downloads include personal H: files at the root and under project folders, excluding system files, the compiler, and unsaved editor RAM. They do not include disk images.
- H: files and completed TF/EEPROM writes survive reload. A reload boots ZShell again; it does not restore CPU/editor RAM.
- `imageStorage: opfs` and `storageNamespace: base.pathname` select one disk set per Hugo project path across all chapters, with stable image keys. HostFS keeps its existing directory and origin-wide H: lock to preserve old files safely.
- Cached images are never replaced automatically. Update notices preserve local data; resetting source examples does not reset disks.
- Disk metadata and image files are in OPFS under `zeal-native/<namespace hash>/`; no image snapshot timers or localStorage/IndexedDB disk saves are used.
- Browser clearing/eviction can remove saves. Backups and import support are explicit.

Only the pilot lesson is implemented. The book-level “all code” option currently loads the same single lesson. Creative and code milestone review status and verification limits are recorded in `VERIFICATION.md`.

To refresh only the workbench bundle and EEPROM without changing the pinned Pascal, ROM, or community card assets:

```sh
python3 scripts/prepare.py --update-workbench \
  --wasm-dir /path/to/Zeal-NativeEmulator/build-wasm \
  --emulator-source /path/to/Zeal-NativeEmulator \
  --eeprom-tool /path/to/existing/zdt
```

`eeprom/autoexec.zs` preserves ZShell's community PATH layout and selects T:/ for the book's installed compiler library. Changing authoring files does not overwrite a reader EEPROM: update checks notify, and the reader explicitly retrieves selected images.

## Appearance and code blocks

The book uses the Midnight light/dark palettes and Zeal purple (`#6B5176`), with the supplied official SVG logo. The sticky header keeps Open Zeal available while reading on desktop and mobile. Its sun/moon switch follows the system until a reader chooses a theme, then remembers that choice. CSS and the theme script use content-hashed URLs so deployments do not reuse stale styles.

Hugo highlights code at build time with Chroma; no client library is needed. Label fenced blocks `pascal`, `makefile`, `cmake`, `bash`, or `zsh`. `Makefile`, `CMakeLists.txt`, and `cli` are also accepted. The code-block hook maps `pascal` to Chroma's `objectpascal` lexer. Text/output blocks stay plain. Token colors follow the light/dark theme.


To rebuild the book TF/EEPROM from already staged assets with the installed Zeal Disk Tool:

```sh
python3 scripts/prepare.py --refresh-media --eeprom-tool /path/to/zdt
```

This operates on the book's distributable images, never a reader's saved OPFS files. The TF is repacked from `community-tf.img` plus staged Pascal files through supported CLI commands and independently checked for exact file contents. Asset checks cover both the retained community base and book installation. Ordinary CI/Hugo builds use these prepared images and need no ZDE or disk tool installation.
