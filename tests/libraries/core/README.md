# Portable core tests

This standalone CMake entry point builds the production `src/markdown` and
`src/core` C++23 modules and the ordinary foundation, parsing, editing, rendering,
and query tests. New core modules and tests in those directories are picked up
automatically. It does not configure the WinUI application or generate native
math font tables. The search/math regex interoperation test compiles only the
independent regex host needed by that test.

The tests use the repository's forked **Boost.UT module** and `import std;`.
A toolchain with real standard-library modules is required; no generated
header-only or concatenated-source substitute is used.

## Build and run

Initialize the test dependency once:

```sh
git submodule update --init third_party/ut
```

With CMake 4.3, Ninja, and a Clang/libc++ installation that ships its standard
library module sources and metadata:

```sh
CC=clang-21 CXX=clang++-21 cmake -G Ninja \
  -S tests/libraries/core -B build/core-portable \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-stdlib=libc++
cmake --build build/core-portable --parallel 4
ctest --test-dir build/core-portable --output-on-failure
```

From a Visual Studio developer shell, omit the compiler environment variables
and the `-stdlib=libc++` flag. The Windows console-runtime guard is linked on
MSVC, just as in the main test build.

Boost.UT accepts a positional test-name pattern for a focused run:

```sh
build/core-portable/FoliaCoreTests '*geometry*'
```

The pinned GFM entity fixture is generated with the same source-lock checks as
the main build; its first generation may fetch the pinned upstream input.
The full CommonMark/GFM corpus and generated pathological suites remain their
separate gates in the root project. A passing portable run does not claim
DirectWrite, TSF, WinUI, or native visual-test coverage.
