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
| `output/common/` | Shared layout, measurement, scripts, and line breaking; `table/` separates dimensions, layout and source comparisons |
| `output/scene/` | Scene ownership, geometry, transforms, resources, and interaction metadata |
| `output/svg/` | SVG serialization and wrapper behavior |
| `fonts/` | Font repository ownership and metadata |
| `adaptors/` | Lite/XML DOM adaptation and export; `lite/` separates parsing, queries, mutation, entities and lifetime |
| `util/` | Numeric, Unicode, RegExp, entities, collections, and utility contracts |
| `integration/` | Full package profile, canonical scenes and cancellation; `renderer/` separates C ABI, batch, SVG and lifetime behavior |
| `editor/` | Math editor syntax and scanner conformance |
| `support/` | Typed runtime fixture adapters, owning observation readers, reusable setup and assertion helpers |
| `fixtures/` | Data files grouped by domain, with schema and source provenance beside the observations |

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

### Runtime data by responsibility

| Fixture location | Contents |
| --- | --- |
| `fixtures/packages/` | Package observations, mappings, arguments and session expectations |
| `fixtures/scenes/` | Common/canonical scenes and typed MathML, SVG, table, action and glyph observations |
| `fixtures/numeric/`, `fixtures/text/`, `fixtures/unicode/` | Numeric bit patterns, character/code-unit cases and exhaustive Unicode observations |
| `fixtures/operators/`, `fixtures/entities/` | Registry lookup, spacing, entity and retry observations |
| `fixtures/adaptors/` | DOM parsing, queries, mutations and measured element cases |
| `fixtures/fonts/` | Expected font/CSS metadata |
| `fixtures/common-source/` | Source locks and independently captured layout/reference expectations |

Large static matrices are data files, not compiled `.inc` or `.hpp` arrays.
Simple flat schemas use TSV rows; nested scene records use named
`field`/`type`/`value` paths with explicit counts and presence markers. Byte
strings, UTF-16 code units and IEEE-754 binary64 values have lossless encodings.
The owning readers in `support/` validate the declared schema, sequence counts,
known callback/enum bindings and complete consumption before exposing typed
views. Test sources retain the assertions and tolerances.

### Integrity and regeneration

```sh
python tests/tools/check_inventory.py
python tests/tools/check_fixtures.py
python -m unittest discover -s tests/tools -p test_fixtures.py
```

These checks also run under `python tests/run.py --suite tooling`. The fixture
guard records every portable data file's exact bytes and all TSV row counts;
missing, added, changed or reordered observations fail. Fixture paths and data
are documented next to each dataset, while the shared preservation checkpoints
live in `../fixtures/preservation/`.

To update an expectation, use the pinned independent source generator identified
by that dataset's README, review its output and run the affected native tests.
Refresh the byte checkpoint explicitly with
`python tests/tools/check_fixtures.py --refresh` only for that reviewed update.
The JavaScript oracle stage generates reference data; Python owns corpus
completeness, native-process execution, comparisons and pass/fail reporting.
The ordinary math suite needs no JavaScript test runner.
