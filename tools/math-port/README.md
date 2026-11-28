# MathJax reference tools

Folia renders mathematics with the C++ library in `src/math`. These scripts run
offline against pinned MathJax sources; none is loaded by the application.
JavaScript captures official reference observations and generates native data
tables. Python owns portable test orchestration, comparisons, inventories and
reports; C++ owns the executable native tests.
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
| Check original script and leaf observer profiles | `mml-observer-source-profiles.py --packet=script` or `--packet=leaf` |
| Check original operator and registry observer profiles | `operator-registry-observer-profile.py --packet=mo-values`, `--packet=mml-registry` or `--packet=mo` |
| Check the complete MmlNode observer profile | `mml-node-source-profile.py` |
| Check both complete MathChoice observer profiles | `mathchoice-observer-source-profile.py` |
| Compare native font data | `font-data-compare-native.py` and `font-data-dump.cpp` |
| Compare native SVG paint with official MathJax CSS and raster samples | `svg-paint/check.py`; stages and commands in [svg-paint/README.md](svg-paint/README.md) |
| Inventory native source, module ownership and review provenance | `native-raii-inventory.py` |
| Exercise the app's native layout/worker performance | `winui-math-layout-probe.ps1`, `winui-math-worker-probe.ps1` |
| Native verification requirements | `../../docs/math/verification.md` |

Run scripts from the repository root. The source profile is `profiles/scope.json`.
Pinned locks live in `locks/` and reference observations in `profiles/evidence/`.
Witnesses and fixture generators use these files and the pinned MathJax and font
packages to reproduce expected values independently of the C++ implementation.
`tests/math` and CMake own executable native tests.

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

The seven owners in `observer-packets.json` also emit complete official
observations. Their four Python entry points in the table above retain the
canonical and original profile hashes, legacy file sizes, complete ordered
records and summaries. MathChoice checks both its 410 canonical cases and all
409 original Source calls, including the original binary64 and own-key
encoding. For example, run `python
tools/math-port/mml-observer-source-profiles.py --packet=leaf` or `python
tools/math-port/mathchoice-observer-source-profile.py`. Capturing fresh Source
observations still requires each owner's pinned Node version and dependencies;
an unavailable or mismatched reference fails without replacing the source lock.

The ten ledger checkers also run in Python. `ledger-source.cjs`,
`member-ledger.cjs`, and the two specialized member generators only sample the
locked MathJax TypeScript sources, retaining each original reference loader.
`ledger-profiles.json` records the location and SHA-256 of each reviewed ledger.
The names of past acceptance reports serve as historical evidence identifiers.
The checker compares members, module bindings, and native targets against the
reviewed ledger. Use `--freeze-reviewed` only for an explicit snapshot update.

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
Historical Markdown source mappings are loaded from
`reference/source-annotations.json`, pinned to the v0.9.0 commit recorded there;
document paths and line numbers describe that historical snapshot. The index is
data, not a live-doc dependency: regenerate only from an immutable provenance
snapshot and verify all path/hash rows exactly. Current source-file header notes
are still extracted from the live source files.

Use `python tools/math-port/diagnose-tex-package-differences.py <source.json>
<native.json>` for retained package/case difference reports. Differences are
grouped deterministically, and arbitrary unequal JSON values produce a root
diagnostic as well. `python tools/math-port/extpfeil-render-diagnostic.py` keeps
the historical source/native provenance checks and only delegates official
Source generation to JavaScript. The original g113 captures are still required;
missing or mismatched historical observations are reported as failures.
