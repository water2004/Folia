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
| Check complete reviewed TypeScript member inventories | `*-ledger-check.py` and `ledger-profiles.json` |
| Replay complete pinned Source profiles in Python | `container-observer-source-check.py`, `remaining-observer-source-check.py` |
| Compare native font data | `font-data-compare-native.py` and `font-data-dump.cpp` |
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

The nine `*-oracle.cjs` owners listed in `source-profile-checks.json` emit only
actual upstream observations as JSON. The two Python Source checkers retain
every frozen profile hash, complete record comparison, own-key ordering, and
the independent 88-case multiscript parent projection. Run `python
tools/math-port/container-observer-source-check.py` or `python
tools/math-port/remaining-observer-source-check.py`; the historical Node
`--check` commands in old acceptance reports now correspond to these entries.

The ten ledger checkers also run in Python. `ledger-source.cjs`,
`member-ledger.cjs`, and the two specialized member generators only sample the
locked MathJax TypeScript sources, retaining each original reference loader.
The reviewed targets, evidence requirements, all members and module bindings,
and optional explicit `--freeze-reviewed` operation are preserved. Validation
never freezes metadata automatically; a missing pinned TypeScript installation,
runtime version mismatch, or stale native target still fails.

`python tools/math-port/parser-host-mutation-test.py` (or `--preflight`) is
read-only: it verifies all 33 retained fault anchors, original baseline hashes,
and named native witnesses. Executing mutations requires `--execute
--build-dir <configured-build>`; use `--repo-root <isolated-checkout>` to keep
other builds independent. CMake must be configured from that same checkout.
`--target` defaults to the historical optional `FoliaMathHostTests` target, and
`--executable` supports multi-configuration output paths. That historical
candidate target was not part of master's tracked CMake graph; the tool does
not silently configure it or update stale plan hashes. Each compile/test is
bounded, every changed source is restored byte for byte, and the original
progress, result, witness, and unchanged-gold checks remain in place.

`python tools/math-port/native-raii-inventory.py` inventories native source,
module ownership and review provenance without Node. `--build-dir` and
`--configuration` select existing CMake File API observations. Lexical CMake
discovery follows the shared `.cmake` includes; it remains distinct from
verified configured target membership. `--self-test` runs the retained original
checks together with missing, malformed and stale File API controls. The tool
only reports review state and does not mark unreviewed implementation as accepted.

Use `python tools/math-port/diagnose-tex-package-differences.py <source.json>
<native.json>` for retained package/case difference reports. Differences are
grouped deterministically, and arbitrary unequal JSON values produce a root
diagnostic as well. `python tools/math-port/extpfeil-render-diagnostic.py` keeps
the historical source/native provenance checks and only delegates official
Source generation to JavaScript. The original g113 captures are still required;
missing or mismatched historical observations are reported as failures.
