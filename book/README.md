# Interactive Pascal book pilot

Hugo source for **Programming the Zeal with Pascal**. The pilot is *The Silent Lab*: a three-room text adventure, taught through the actual native Pascal IDE in a browser-hosted Zeal.

## Preview

Hugo is required (tested version recorded in `VERIFICATION.md`). No Node packages or theme downloads are needed.

```sh
cd book
hugo server --bind 127.0.0.1 --port 1313 --baseURL http://localhost:1313/
```

Open <http://localhost:1313/> in desktop Chrome or Edge. Open the pilot lesson, open the Zeal, and press **Start the Zeal**. At ZShell:

```text
H:/bin/zpascal.bin H:/lib/zpascal.zpl
```

Source lives in `H:/book/lab`. Save personal work under `H:/work`. HostFS uses a browser-private OPFS directory; saving inside Pascal persists files. No third-party emulator sources or generated application code are patched. T: boots the unchanged community TF image; T: edits are session-only in this pilot. There is no account or second localStorage persistence implementation.

## Build and check

```sh
python3 scripts/prepare.py --downloads-only
node --test tests/workspace.test.mjs
python3 scripts/check.py
# Requires existing ZDE-built native compiler maps:
python3 scripts/verify-native.py --compiler-build ../build
hugo --minify --baseURL https://zoul0813.github.io/zeal-pascal/
```

The manual **Book preview** GitHub Actions workflow builds an artifact and can deploy to GitHub Pages when explicitly selected. Configure Pages to use GitHub Actions before deployment. The workflow derives the standard project URL from the repository owner/name; local previews use an explicit localhost base URL. No commit, push, or deployment is part of the local pilot implementation.

## Prepared assets

`static/emulator/` contains an unchanged installed native WASM bundle and matching HostFS/library files. `static/media/` contains the original community 2 MiB TF image, a compatible installed ZDE ZShell boot ROM, community licenses/notices, and matching Pascal build outputs. `static/provenance.json` binds staged assets to SHA-256 hashes and origins. These small prepared assets are included in the book source so an ordinary Hugo build works without ZDE or private local directories.

To refresh assets from installed, verified builds:

```sh
python3 scripts/prepare.py \
  --wasm-dir /path/to/native/wasm/bundle \
  --emulator-source /path/to/Zeal-NativeEmulator \
  --community-dir /path/to/Zeal-Software-Collection/dist/community \
  --pascal-build /path/to/zeal-pascal/build \
  --boot-rom /path/to/zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img
```

The WASM directory must contain `zeal.elf.js`, `zeal.elf.wasm`, and matching `libnative/hostfs.js` and `libnative/zeal-native.js`, with the supported HostFS exports. Build Zeal Pascal through `zde cmake`; do not substitute a host compiler. The community TF card remains unchanged. Its bundled ROM did not launch the current compiler in native checks; the staged compatible ZDE ROM did. Supply a matching supported ZShell boot ROM when refreshing. Inspect provenance, licenses, and checks after refresh. Asset refresh is a maintainer operation, never a reader flow.

`static/code/lab/*.PAS` is the single source for printed listings, served lesson files, and reproducible `book-code.zip`. Never edit copies of those listings into the chapter.

## Workspace rules

- The OPFS directory `zeal-pascal-book` is private to the site's origin. A Web Lock prevents concurrent emulator tabs from modifying it.
- System files are installed only when absent. User files are never replaced by opening a lesson.
- Fresh lesson copies receive new directories. Imports receive new directories and validate all paths/data before writing.
- Workspace downloads include `book/` and `work/`, not the compiler or unsaved editor RAM.
- Source saved on H: survives reload. A reload boots ZShell again; it does not restore CPU/editor RAM.
- Browser clearing/eviction can remove saves. Backups and import support are explicit.

Only the pilot lesson is implemented. The book-level “all code” option currently loads the same single lesson. Creative and code milestone review status and verification limits are recorded in `VERIFICATION.md`.
