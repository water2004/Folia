# Test layout

Folia's Boost.UT C++23-module test executables include:

- `FoliaTests` covers the platform-independent core under `foundation/`,
  `parsing/`, `editing/`, `query/`, and `rendering/`.
- `FoliaWindowsEditorTests` covers deterministic Windows editor adapters under
  `platform/` without creating a WinUI window.
- `FoliaWinUITests` covers application controllers under `app/` using the real
  WinRT UI dispatcher, production app objects and a bounded message pump. File
  loading tests hold the surface unavailable, then initialize it or cancel the
  operation; they check UI thread affinity, exact document content and reopening.
  Math scrolling tests cover UI queue fairness under repeated slow frames,
  deferred repaint without recursion, canceled-work notification, offscreen
  geometry lookup without new native work/IO, and rapid viewport replacements
  that settle while preserving document content and selection.
- `FoliaEncodingTests` covers the app storage boundary against the pinned ICU
  runtime: BOM handling, detection candidate ordering, strict transcoding,
  converter enumeration, and byte-faithful round trips.
- `FoliaCommonMarkTests` registers all 652 official CommonMark 0.31.2 examples
  individually, plus per-example source-preservation assertions. It remains a separate conformance suite; no known-failure skips or
  alternate expected outputs are used. Run `cmd /c tests\parsing\run_commonmark.bat`.
  Reports and test-projection limits are documented in
  [markdown-conformance](../tools/markdown-conformance/README.md).

Run the following from the repository root. Each wrapper locates the repository
relative to its own file and initializes the Visual Studio compiler environment:

```powershell
cmd /c tests\run_core.bat
cmd /c tests\platform\run.bat
cmd /c tests\encoding\run.bat
cmd /c tests\parsing\run_commonmark.bat
cmd /c tests\parsing\run_gfm.bat
```

After `setup.ps1` and a Debug `build_app.ps1`, run application tests with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\app\run.ps1
```

The application runner compiles the same Boost.UT module against the app's
module/PCH settings and links its existing production objects plus the console
test runtime. Tests and logs stay in `build/tests/winui/`, outside the MSI payload.
It does not launch the normal application or change an installed copy.

The md4c diagnostic runner is `tests/parsing/run_md4c_reference.bat`; run the
official conformance tests first. Historical validation notes retain their old
root-level command names; the commands above are the current entry points.

The wrappers configure an ignored build directory, build only the requested
executable, and forward command-line arguments. Boost.UT accepts a positional name pattern,
for example `cmd /c tests\run_core.bat "*inline*"`; `-t` lists tags.

## Conventions

- Every test translation unit includes `support/folia_test.hpp` first. That
  shared header imports the repository's forked `boost.ut` module and exposes
  the common literals; test files deliberately do not `import std;`.
  Application tests use the required `pch.h` before this shared header.
- Import the narrowest required `folia.markdown.*`, `folia.core.*`, or `folia.platform.*` modules.
  Do not use the header-only `<boost/ut.hpp>` path.
- Shared fixtures belong in `support/`; feature tests stay beside the owning
  concern rather than accumulating in a single giant file.
- Core tests must remain platform independent. Window/XAML behavior is not
  simulated inside the core suite; deterministic geometry, scrolling,
  shortcuts, snippet sessions, completion sessions, and viewport work plans
  belong in the platform suite.

Editing and parsing tests assert the long-lived architectural invariants, not
only visible examples: inline CST flattening and serialization are
character-exact, selection remains in one block-local source coordinate,
normal edits avoid full-document parse/serialization, and Undo/Redo restores
the exact tree, source, and selection. Property tests exercise randomized
source edits, nested containers, structural transactions, tables, save/reload,
and malformed-but-editable Markdown states.
