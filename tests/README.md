# Folia test system

Use `tests/run.py` to select, configure, build and run portable suites. The suite
catalog is `tests/suites.json`; the portable project and the repository's main
CMake project use the same production libraries and test definitions.

## Quick start

From the repository root:

```sh
git submodule update --init third_party/ut
python tests/run.py list
python tests/run.py --suite core
python tests/run.py --suite all
```

Use multiple `--suite` options to select focused suites. Native suites require
CMake, Ninja, Python and a C++23 toolchain with `import std` support. On Linux,
portable verification uses Clang/libc++ and the repository's forked Boost.UT
module. MSVC builds use a Visual Studio developer shell and the console runtime
guard. Compiler, standard library and module metadata must come from compatible
toolchains; tests use the repository's Boost.UT module rather than its
header-only counterpart.

A Linux example with versioned libc++ module metadata is:

```sh
CC=clang-21 CXX=clang++-21 python tests/run.py --suite core \
  --cmake-arg='-DCMAKE_CXX_FLAGS=-stdlib=libc++ -fexperimental-library' \
  --cmake-arg=-DCMAKE_CXX_STDLIB_MODULES_JSON=/usr/lib/llvm-21/lib/libc++.modules.json
```

Use paths supplied by the installed toolchain. A CMake toolchain file can be
passed with `--cmake-arg=-DCMAKE_TOOLCHAIN_FILE=...`.

## Suites

| Suite | Coverage | Preparation |
| --- | --- | --- |
| `core` | Editor/model behavior, editor policies and standalone Markdown API | Boost.UT submodule |
| `completion` | Completion context, edits, cache and provider protocols | Vendored protocol sources |
| `math-editor` | Incremental TeX editing and syntax | Boost.UT submodule |
| `math` | Math parsing, packages, MathML, layout, scenes, fonts and C ABI | Pinned MathJax/font inputs for resource generation |
| `mermaid` | Diagram families and shared layout engines | Pinned yaml-cpp fetched by CMake |
| `conformance` | CommonMark/GFM corpora and bounded parser/serializer processes | Committed source observations; ordinary runs work offline |
| `tooling` | Runner, fixture integrity, comparator and failure checks | Python packages in Mermaid comparison requirements |

`--suite all` selects every portable suite. Selecting `math` and `mermaid`
together also runs `FoliaMermaidMathTests` against the shared math library. The
core gate includes SDK-free editor policy tests. The WinUI frame scheduling tests
run in the visual application suite.

The Markdown consumer suite links only `folia::markdown`; its standalone entry
under `tests/libraries/markdown` does not require core, math or WinUI. An optional
md4c conformance target joins the selected aggregate build when enabled.

Tooling alone does not require a C++ compiler:

```sh
python -m pip install -r tools/mermaid-cpp/regression/requirements.txt
python tests/run.py --suite tooling
```

Before the first full math build, restore the existing production inputs:

```sh
npm ci --prefix tools/mathjax-bundle --ignore-scripts
python tools/math-port/prepare-source.py
```

The preparation command copies entity inputs from the installed package and
checks their exact locked bytes, including line endings. CMake also runs this
idempotent step. Node is used by production resource generators; native test
assertions and Python acceptance checks do not execute JavaScript.

A first Mermaid build may fetch yaml-cpp at its pinned revision. An existing
checkout can be supplied with CMake's
`FETCHCONTENT_SOURCE_DIR_FOLIA_YAML_CPP` override. Browser and raster capture
have optional prerequisites documented with the oracle tools; ordinary native
suites use committed observations.

## Runner commands and reports

```sh
python tests/run.py configure --suite math
python tests/run.py build --suite math --jobs 4
python tests/run.py --suite math --test-regex '^FoliaMathSvgBridgeTests$'
python tests/run.py --suite conformance --build-dir build/tests/conformance
python tests/run.py --suite all --test-jobs 1 --timeout 180
```

The default build directory is `build/tests/portable`. The runner builds the
selected aggregate, stops on configure/build failure, propagates the failing
exit status, and treats an empty CTest selection as an error. Each invocation
writes `run.json`; test runs also write CTest's `results.xml` and normal native
output. Bounded Markdown cases run in separate processes with fixed time limits.

To filter tests inside a native executable, pass a Boost.UT positional pattern:

```sh
build/tests/portable/FoliaCoreTests '*geometry*'
```

The root `FoliaTests` target, `tests/libraries/core` entry and existing Windows
wrappers remain available. The portable entry calls the core executable
`FoliaCoreTests`.

## Fixture and registration integrity

Run the integrity checks from the repository root:

```sh
python tests/tools/check_inventory.py
python tests/tools/check_fixtures.py
python -m unittest discover -s tests/tools -p test_fixtures.py
```

The registration check protects the maintained test inventory. The fixture
check discovers portable corpora, verifies SHA-256 and byte sizes, and checks
TSV row counts. It rejects changed, missing, malformed or newly added unrecorded
fixtures. Raw upstream Markdown and PNG/RGBA observations retain their exact
bytes; other portable fixture text uses LF through `.gitattributes`. The tooling
suite exercises negative controls for these checks.

For an intentional fixture update, verify the independent source observation,
review the complete data diff, run the native consumers, then update the
integrity checkpoint explicitly:

```sh
python tests/tools/check_fixtures.py --refresh
```

Commit the reviewed fixtures and checkpoint together. The checkpoint proves
byte integrity; provenance and native behavior tests establish different
properties. See `tests/fixtures/preservation/README.md` for the integrity
check's scope.

An existing Windows checkout may retain CRLF in unchanged files after merging
LF attributes. If `git ls-files --eol` reports `i/lf w/crlf` for a fixture marked
`eol=lf`, use a fresh worktree to check the committed bytes. Tool fixtures live
under `tools/math-port/tests/fixtures` and
`tools/mermaid-cpp/regression/tests/fixtures`.

## Windows integration

The portable project excludes WinUI, DirectWrite and Windows DLL-host targets.
Run the Windows integration suites with:

```powershell
cmd /c tests\platform\run.bat
cmd /c tests\encoding\run.bat
powershell -ExecutionPolicy Bypass -File .\tests\app\run.ps1
```

Application tests need the normal `setup.ps1` and app build workflow. They use
the real UI dispatcher and production objects; portable test results do not
claim Windows SDK or application/UI validation. After building both Windows
distributions, verify their payloads with:

```powershell
python tests/installer/test_release_payload.py <msi> <portable-zip>
```

## Native test conventions

Every test includes `support/folia_test.hpp`, which imports the repository's
forked `boost.ut` module. Test translation units do not directly `import std;`.
App tests preserve their required `pch.h` ordering. Import only the production
modules needed by the behavior under test.

Editor tests assert exact CST flattening and serialization, one block-local
source coordinate, bounded incremental work, and exact tree/source/selection
restoration by Undo/Redo. Keep property and stress cases active; do not replace
them with known-failure skips during reorganization.
