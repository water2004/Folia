# MathJax reference tools

Folia renders mathematics with the C++ library in `src/math`. These scripts run
offline against pinned MathJax sources; none is loaded by the application.
Do not remove a script just because it is CommonJS: many produce the formal
native tests' expected values.

## Current entry points

| Responsibility | Entry point |
| --- | --- |
| Generate native font tables (also run by CMake) | `font-data-generate-native.cjs` |
| Generate native entity tables (also run by CMake) | `entities-data-generate.cjs` |
| Verify TeX package registrations and produce alias/SVG fixtures | `tex-package-conformance.cjs` |
| Compare native font data | `font-data-compare-native.cjs` and `font-data-dump.cpp` |
| Exercise the app's native layout/worker performance | `winui-math-layout-probe.ps1`, `winui-math-worker-probe.ps1` |
| Native verification requirements | `../../docs/math-port/native-raii-acceptance.md` |

Run scripts from the repository root. The source locks, `reference/`,
`common-layout-reference/`, and the pinned MathJax/font packages are inputs,
not application assets. Source witnesses, fixture generators and their helper
chains remain together so expected values can be regenerated independently of
the C++ implementation. `tests/math` and CMake own executable native tests.

The old Rust `regress` candidate, its source-copy preparation scripts and its
experimental comparisons were removed after the production implementation
settled on SRELL (`src/math/util/regexp_host.cpp`). Independent JavaScript
RegExp reference witnesses remain. Historical documents describe those trials;
their removed scripts can be recovered from Git history, not from an alternate
active implementation directory.

Before retiring further tooling, check build/test consumers and relative or
dynamically constructed dependencies. A reference generator can be useful even
when no production source names it. Candidate patches are not build inputs and
must never be applied automatically as part of a test run.
