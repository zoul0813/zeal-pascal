# GitHub builds and releases

## Build a distribution

The **Build distribution** workflow runs on pushes to `main`. It can also be
started from the repository's **Actions** tab with **Run workflow**.

For a manual build, choose the emulator mode:

- **release** (default): download the latest stable Linux emulator release.
- **source**: build that same release's source with Meson.

Both modes use the matching raylib library from the emulator's source tree.
The Ubuntu runner uses ZDE's setup action, then builds the emulation-enabled
Zeal OS ROM with its bundled command shell through `zde kernel zeal8bit/emu`.
It runs `make JOBS=6` to build the compiler and native units, compile CRT and
ZVB concurrently, and populate `bin/`. This uses ZDE's container toolchain;
the emulator and Pascal-unit automation run on the host runner.

The workflow packages `bin/` and `examples/` into `zeal-pascal.zip` and uploads
it as the **zeal-pascal-dist** Actions artifact. The ZIP retains those two
top-level directories. Examples include their tracked sources, documentation,
assets and prebuilt executables; this workflow does not recompile the example
applications. Artifacts are retained for 30 days. Failed unit-compilation logs
are retained separately for seven days when available.

The ZDE setup action is pinned in the workflow. Its container image and
catalog dependencies use ZDE's defaults, and the emulator uses its latest
stable release at build time. Build logs record the selected emulator release.
The packaged result is reused unchanged when creating a release.

## Create a versioned release

1. Wait for a successful **Build distribution** run on the default branch.
2. Open **Actions → Create release → Run workflow**, selecting the default
   branch (currently `main`).
3. Enter an unused version such as `1.0.0`, `v1.0.0` or `1.1.0-rc.1`.
4. Run the workflow.

The workflow normalizes versions to tags prefixed with `v`. A version suffix
such as `-rc.1` produces a GitHub prerelease. Versions must have three numeric
components without leading zeros and may have an alphanumeric prerelease suffix.

It selects the most recent successful default-branch build with an unexpired
**zeal-pascal-dist** artifact. Runs are considered in GitHub's newest-first run
order; an older run's rerun does not move it ahead of a newer run. Failed,
in-progress, pull-request and fork builds are excluded. If the newest successful
run's artifact is missing or expired, the workflow considers earlier successful
runs. If no artifact remains, run **Build distribution** again first.

The version tag points to the **selected build's commit**, which can differ
from the release workflow's current commit. The workflow downloads that exact
artifact, creates the tag and a draft release, uploads the unchanged ZIP as
`zeal-pascal-vVERSION.zip`, then publishes the release with generated notes
and a link to the source build. Release notes start with a direct link to the
prebuilt ZIP, a description of its `bin/` and `examples/` directories, and a link
to the README installation instructions at the release tag. GitHub's generated
change notes follow under **Changelog**. The workflow never rebuilds the package or overwrites an
existing tag/release. At most one release runs at a time. GitHub keeps only one
pending dispatch in the concurrency group; a newer pending dispatch replaces
an older pending one. Check that each requested version completed.

If a failure occurs after tagging, the tag and any draft release are retained
for inspection. An asset-upload failure leaves the release unpublished. Inspect
that state before removing a failed attempt or choosing another version; a
rerun will reject a version whose tag or draft already exists.

The workflows use the built-in `GITHUB_TOKEN`. Build jobs request read-only
repository access; release jobs require `contents: write` and `actions: read`.
No personal token or custom repository secret is needed. The custom release
helper is Python and uses the runner's GitHub CLI for API requests. Its focused
tests can be run locally with:

```sh
python3 -m unittest discover -s tests -p test_release.py
```

After committing and pushing the workflow files, confirm that a hosted
**Build distribution** run succeeds before creating the first release. Local
helper tests cover API logic; the hosted build verifies runner dependencies.
