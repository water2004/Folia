# Package registry and texvc observations

- `registrations.tsv`: all 1,763 finite-map observations in original order,
  including handler kind, map, token, effective owner, character flag and value.
- `packages.tsv`: all 39 package names in profile order, with explicit indices.
- `texvc-svg.tsv`: all 216 original SVG observations (108 inputs in two modes),
  preserving every output byte and the ordering used by sequential/batch tests.
- `aliases.json`: all 101 alias definitions captured from the pristine MathJax
  2.7.9 reference file, with its source version, path and SHA-256.

UTF-16 columns use reversible `\uHHHH` unit escapes; byte columns use `\xHH`.
`package_registry_fixtures.hpp` loads the tables lazily and validates dimensions
and package ordering. The migration compared all 2,018 existing test records
against compiler-decoded originals, then reproduced the TSV bytes with the
updated independent reference serializer.

Regenerate reference observations with `tools/math-port/tex-package-conformance.cjs`.
That JavaScript entry point executes pinned MathJax and writes data only. Native
alias C++ serialization is handled by `tools/math-port/generate-texvc-aliases.py`.
Its `--check` command verifies the committed alias definitions without JavaScript.

The completion registry digest is SHA-256 of normalized-LF `registrations.tsv`
followed by `packages.tsv`, including their headers. The completion generator
checks that digest before generating the application catalog; changing storage
format did not change its 1,769 commands/templates.
