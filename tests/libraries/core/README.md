# Portable core tests

The shared entry is now `python tests/run.py --suite core`; see `tests/README.md`
for suite selection, toolchain options and reports. This directory remains a
compatible standalone core-only CMake entry. Both use the same production and
test definitions.

This standalone CMake entry point builds the production `src/markdown` and
`src/core` C++23 modules and the ordinary foundation, parsing, editing, rendering,
and query tests. New core modules and tests in those directories are picked up
automatically. It does not configure the WinUI application or generate native
math font tables. The search/math regex interoperation test compiles only the
independent regex host needed by that test.

The gate also compiles the production key-command, shortcut catalog, and
shortcut-settings adapters from `src/platform/windows/editor/interaction`,
together with their deterministic tests. These modules use no Windows APIs;
native keyboard, text-service, and renderer integration remain outside this
portable executable.

Production modules use `import std;`. Tests include `support/folia_test.hpp`
and obtain the standard-library API through the repository's forked
**Boost.UT module**, which exports it; tests do not import `std` directly.
A toolchain with real standard-library modules is required; no generated
header-only or concatenated-source substitute is used.

## Build and run

Initialize the test dependency once:

```sh
git submodule update --init third_party/ut
```

With CMake 4.3, Ninja, and Clang/libc++ 21 or newer, including the standard
library module sources and metadata:

```sh
CC=clang-21 CXX=clang++-21 cmake -G Ninja \
  -S tests/libraries/core -B build/core-portable \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-stdlib=libc++
cmake --build build/core-portable --parallel 4
ctest --test-dir build/core-portable --output-on-failure
```

For the Debian/Ubuntu LLVM 21 packages, explicitly select the versioned
metadata with
`-DCMAKE_CXX_STDLIB_MODULES_JSON=/usr/lib/llvm-21/lib/libc++.modules.json`.
Those packages also install a multiarch metadata file whose relative paths
can resolve outside the directory containing the module sources. Selecting
the versioned file preserves the shipped module sources and their correct
relative include paths.

From a Visual Studio developer shell, omit the compiler environment variables
and the `-stdlib=libc++` flag. The Windows console-runtime guard is linked on
MSVC, just as in the main test build.

Boost.UT accepts a positional test-name pattern for a focused run:

```sh
build/core-portable/FoliaCoreTests '*geometry*'
```

All 2,125 pinned GFM entities are read from committed runtime data. The source
lock and complete decoded values are checked by the Python tooling suite.
The full CommonMark/GFM corpora and bounded pathological suites are selected
with `python tests/run.py --suite conformance`. A passing portable run does not claim
DirectWrite, TSF, WinUI, or native visual-test coverage.
