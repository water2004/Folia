# Portable mathematics tests

The test source layout follows the modules exercised by each suite. Test names
remain stable; helpers and committed reference observations have separate homes.

| Directory | Coverage |
| --- | --- |
| `input/tex/` | Parsing, scanning, stack/session state, options, errors, and callbacks |
| `input/packages/` | Individual TeX packages and their integration with the parser |
| `core/mml/` | MathML nodes, ownership, cloning, attributes, operators, and inheritance |
| `core/tree/` | Generic tree ownership, factories, and visitors |
| `core/` | Math-item state |
| `output/common/` | Shared layout, measurement, tables, scripts, and line breaking |
| `output/scene/` | Scene ownership, geometry, transforms, resources, and interaction metadata |
| `output/svg/` | SVG serialization and wrapper behavior |
| `fonts/` | Font repository ownership and metadata |
| `adaptors/` | Lite/XML DOM adaptation and export |
| `util/` | Numeric, Unicode, RegExp, entities, collections, and utility contracts |
| `integration/` | Full package profile, canonical scenes, renderer/C ABI, and cancellation |
| `editor/` | Math editor syntax and scanner conformance |
| `support/` | Typed fixture adapters and reusable setup/assertion helpers |
| `fixtures/` | Committed, independently observed inputs and expected values |

## Running

From the repository root, the shared runner configures, builds, and executes the
selected portable suite:

```sh
python tests/run.py run --suite math
python tests/run.py run --suite math --test-regex '^FoliaMathSvgBridgeTests$'
python tests/run.py run --suite math-editor
```

Use the toolchain options documented in `../README.md` for the C++23 module build.
These suites do not require the Windows SDK. `FoliaMathTests` contains the complete
math suite, while `FoliaMathSvgBridgeTests` reuses its focused output/C ABI tests.
`editor/editor_syntax.cpp` belongs to `FoliaMathEditorTests`; scanner conformance
remains in the complete math suite. CMake discovers categorized math sources
recursively and keeps the focused SVG source list explicit.

## Reference observations

Ordinary native tests read committed data through the `folia.test.fixture_data`
module. Paths are rooted at the test directory, so the process working directory
does not select a different corpus. Missing files, invalid columns, bad encodings,
and unexpected record counts fail visibly. Typed adapters keep UTF-16 code units,
optional values, sequence order, and exact floating-point bits intact.

JavaScript in `tools/math-port` captures independent MathJax/reference-host
observations. It is used when deliberately regenerating those expectations and
is not invoked by native test execution. Each migrated data directory documents
its schema and provenance. Expected values must not be regenerated from Folia's
own output. New datasets belong in `fixtures/`; test sources should express the
behavior and comparisons that consume them.
