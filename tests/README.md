# Folia test system

Use `tests/run.py` to configure, build and run portable suites. The suite catalog
is `tests/suites.json`; the standalone project and the repository's main CMake
project share the same production libraries and test definitions.

The completed refactor's [validation record](../docs/test-system-validation.json)
records the tested revision, all 26 portable CTest entries, fixture preservation
checks, and the separate scoring boundaries for CommonMark/GFM and browser
captures.

## Start here

From the repository root:

```sh
git submodule update --init third_party/ut
python tests/run.py list
python tests/run.py --suite core
python tests/run.py --suite core --suite completion --suite math-editor
python tests/run.py --suite all
```

Native suites require CMake 4.3, Ninja, Python, and a C++23 toolchain with real
`import std` support. Clang/libc++ 21 and the repository's forked Boost.UT module
are used by the portable Linux verification. MSVC builds use a Visual Studio
developer shell and the existing console-runtime guard.

A Linux example with the versioned libc++ module metadata is:

```sh
CC=clang-21 CXX=clang++-21 python tests/run.py --suite core \
  --cmake-arg='-DCMAKE_CXX_FLAGS=-stdlib=libc++ -fexperimental-library' \
  --cmake-arg=-DCMAKE_CXX_STDLIB_MODULES_JSON=/usr/lib/llvm-21/lib/libc++.modules.json
```

Use the paths supplied by your installed toolchain. A CMake toolchain file can
also be passed with `--cmake-arg=-DCMAKE_TOOLCHAIN_FILE=...`. Compiler, standard
library and module metadata must belong to a compatible toolchain; neither a
header-only Boost.UT substitute nor concatenated source files are used.

## Suites and dependencies

| Suite | Native targets / coverage | Additional preparation |
| --- | --- | --- |
| `core` | `FoliaCoreTests`, `FoliaPortablePolicyTests` and `FoliaMarkdownConsumer`: editor/model behavior, SDK-free editor policies and the standalone Markdown API | Boost.UT submodule |
| `completion` | `FoliaCompletionTests`: context budgets, edits, cache and provider protocols | Vendored protocol sources |
| `math-editor` | `FoliaMathEditorTests`: incremental TeX editing and syntax | Boost.UT submodule |
| `math` | `FoliaMathTests` and `FoliaMathSvgBridgeTests`: parser/packages, MathML, layout, scenes, fonts and C ABI | Pinned MathJax/font packages for production resource generation |
| `mermaid` | `FoliaMermaidTests`: all diagram families and shared layout engines | The pinned yaml-cpp dependency is restored by CMake |
| `conformance` | Complete CommonMark/GFM corpora plus bounded parser/serializer processes | Committed, hash-verified source observations; ordinary runs work offline |
| `tooling` | Python fixture, comparator, failure, integrity and runner tests | Python packages in the Mermaid comparison requirements |

Selecting `math` and `mermaid` together also builds and runs
`FoliaMermaidMathTests` against the actual Math shared library. `--suite all`
selects every portable suite. The core gate also runs the SDK-free policy tests
under `platform/` for display mapping, LaTeX/snippet sessions, priority queues,
scrolling and selection dragging. The WinUI frame scheduling tests run in the
visual application suite so they can use the compositor's rendering callbacks.

`FoliaMarkdownConsumer` retains four library-only consumer cases and links
`folia::markdown`. Its `tests/libraries/markdown` standalone entry configures only
the Markdown library and test support, without core, math or WinUI dependencies.
When the optional md4c conformance target is enabled, it is included in the
selected aggregate build.

Tooling alone does not require a C++ compiler:

```sh
python -m pip install -r tools/mermaid-cpp/regression/requirements.txt
python tests/run.py --suite tooling
```

Before the first full math build, restore its existing production inputs:

```sh
npm ci --prefix tools/mathjax-bundle --ignore-scripts
python tools/math-port/prepare-source.py
```

The preparation command copies the TypeScript entity inputs from that installed
package and verifies their exact source-lock bytes, including pinned line
endings. It does not download sources or change locks. CMake also invokes this
idempotent step. Node remains a dependency of the existing production font and
entity data generators; executable native test assertions and Python acceptance
checks do not evaluate JavaScript.

A first Mermaid build may fetch yaml-cpp at its pinned revision. An already
prepared dependency can be supplied through CMake's
`FETCHCONTENT_SOURCE_DIR_FOLIA_YAML_CPP` override. Official browser/raster capture
has separate optional prerequisites documented beside the oracle tools; ordinary
native suites use committed observations.

## Commands, filtering and reports

```sh
python tests/run.py configure --suite math
python tests/run.py build --suite math --jobs 4
python tests/run.py --suite math --test-regex '^FoliaMathSvgBridgeTests$'
python tests/run.py --suite conformance --build-dir build/tests/conformance
python tests/run.py --suite all --test-jobs 1 --timeout 180
```

The default build directory is `build/tests/portable`. The runner builds the
explicit `FoliaSelectedTests` aggregate, including selected executables marked
`EXCLUDE_FROM_ALL`. It stops on configure/build failure, propagates the failing
exit status, and treats an empty CTest selection as an error. It uses the same
Python interpreter that launched it; arguments are passed without a shell.

Each invocation writes `run.json` with selected suites, command arguments,
actual exit codes, timings and success status. A test run also writes CTest's
`results.xml`. Native output and any failed assertions are included in CTest's
normal reports. Pathological Markdown cases retain their individual bounded
processes and established time limits.

For a test-name filter inside a native executable, use Boost.UT's positional
pattern after building it, for example:

```sh
build/tests/portable/FoliaCoreTests '*geometry*'
```

The root CMake target `FoliaTests`, the `tests/libraries/core` entry and existing
Windows wrappers remain available. The portable entry calls the core executable
`FoliaCoreTests`.

## Source and data layout

| Location | Responsibility |
| --- | --- |
| `foundation/`, `parsing/`, `editing/`, `query/`, `rendering/` | Portable editor/model behavior, grouped further by concern |
| `completion/` | Completion and provider contracts |
| `math/` | Input/packages, semantic trees, layout, scenes, SVG, fonts, utilities and integration; see its README |
| `mermaid_cpp/` | API, frontend, config, themes, labels, diagram families and layout engines; see its README |
| `support/` | Reusable test setup, assertions and the `folia.test.fixture_data` module |
| Feature `fixtures/` directories | Runtime input tables, independently observed expected values, schemas and provenance |
| `fixtures/preservation/` | Original registration inventory, current fixture byte/row checkpoint and historical migration evidence |
| `cmake/` | Shared suite definitions used by both CMake entry points |
| `tools/` | Portable test-system checks |
| `platform/`, `app/`, `encoding/` | Existing Windows/WinUI/storage integration entry points; SDK-free policies under `platform/` also participate in the portable core gate |

Large data matrices belong in fixture files. Their readers preserve byte strings,
UTF-16 code units (including isolated surrogates), Unicode scalar sequences,
ordered fields and exact floating-point representations. Missing or malformed
files fail loudly. Paths are resolved against the configured test-data root,
independently of the process working directory.

Small inputs that explain one behavior can remain beside that assertion. Shared
setup belongs in a focused support file; do not assemble unrelated behaviors into
one large translation unit. Each migrated dataset documents its schema and
provenance. CommonMark/GFM originals and their transport rules are documented in
`parsing/fixtures/conformance/README.md`.

## Preserving cases and updating fixtures

Run both preservation checks from the repository root:

```sh
python tests/tools/check_inventory.py
python tests/tools/check_fixtures.py
python -m unittest discover -s tests/tools -p test_fixtures.py
```

The registration inventory tracks the refactor's 2,811 original Boost.UT
registrations, including multiplicity and any documented replacement tests. Its optional
`--windows-baseline` audit retains the original hashes for all 120 historical
Windows-runner test/support files: 118 are unchanged under the recorded
CRLF-to-LF hash policy. The two SDK-free LaTeX/snippet policy tests each add one
explicit module import; removing only that declared, unique import line must
recover the original hash. Their assertions, inputs and helpers remain intact,
and Windows SDK tests are unchanged. This audit records the refactor's scope;
it does not restrict future Windows test maintenance.

The fixture guard discovers portable corpora independently of the checkpoint and
checks each file's exact SHA-256, byte size and, for TSV files, record count.
Changed values or row order, missing data and newly added unrecorded files fail.
Raw upstream Markdown and PNG/RGBA observations retain their exact bytes; other
portable fixture text uses LF through `.gitattributes`. The guard neither
normalizes files nor regenerates expectations. Its negative controls and the
repository check run with the `tooling` suite.

An existing Windows checkout may retain CRLF in unchanged files after merging
the LF attributes. If `git ls-files --eol` shows `i/lf w/crlf` for a fixture
marked `eol=lf`, run the checks from a fresh worktree so Git writes the
committed bytes. Tool fixtures are discovered under `tools/math-port/tests/fixtures`
and `tools/mermaid-cpp/regression/tests/fixtures`.

For an intentional fixture change, verify the independent source observation,
review the complete data diff and run its native consumers. Then record the new
checkpoint explicitly with `python tests/tools/check_fixtures.py --refresh` and
commit it with the reviewed data. A checkpoint proves later byte integrity;
field-by-field migration evidence and native behavior tests establish different
properties. See `fixtures/preservation/README.md` for their scopes.

## JavaScript and reference captures

The test tooling has two stages. JavaScript calls the pinned official
MathJax/Mermaid implementations or real reference rendering backends to capture
observations. Python owns native-process execution, corpus completeness checks,
comparisons, tolerances, negative controls, diagnostics and pass/fail status.
Saved Canvas/formula observations can be compared entirely offline without Node.

Keep source locks and source-derived expectations independent of Folia's output.
Do not regenerate expected values from a candidate implementation or relax
numeric/pixel tolerances to pass a refactor. Both positive cases and deliberate
corruption cases belong in the portable tooling tests.

Historical candidate tools retain their original strict provenance checks.
Some require old build captures or drafts that were already absent from master;
replaying those commands fails visibly if their prerequisites are unavailable.
Their comparison policies and retained example records remain covered by the
portable Python tests. Historical replay is not counted as a successful current
native run.

The product's tree-sitter grammar and production resource generators are
separate build inputs. They are not general-purpose test runners.

## Windows-specific integration

The portable project excludes WinUI, DirectWrite and Windows DLL-host targets.
The existing integration commands are:

```powershell
cmd /c tests\platform\run.bat
cmd /c tests\encoding\run.bat
powershell -ExecutionPolicy Bypass -File .\tests\app\run.ps1
```

Application tests require the existing `setup.ps1` and app build workflow. Their
runners use the real UI dispatcher and production objects; this portable test
refactor does not claim Windows SDK or application/UI validation.

After building both Windows distributions, run
`python tests/installer/test_release_payload.py <msi> <portable-zip>` to compare
their payloads, verify their SHA-256 sidecars, and check the extracted files
for embedded workspace and user-profile paths. `--forbid-path <directory>`
also checks a separate dependency or build directory.

## Native test conventions

Every test includes `support/folia_test.hpp`, which imports the repository's
forked `boost.ut` module. Test translation units obtain the standard library
through that module and do not directly `import std;`. App tests keep their
required `pch.h` ordering. Import the narrowest production modules required by
the behavior under test.

Editing/parsing tests retain their architectural assertions: exact CST flattening
and serialization, one block-local source coordinate, bounded incremental work,
and exact tree/source/selection restoration by Undo/Redo. Property and stress
cases remain active; examples are not removed or replaced with known-failure
skips during reorganization.
