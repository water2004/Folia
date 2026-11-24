# TeX package reference fixtures

Each package directory separates successful trees, error inputs, stateful
sequences and direct helper observations. The tests retain their native
assertions and load the complete tables at runtime. These tables replace
compiled C++ arrays; adding or inspecting a reference case does not require
reading escaped C++ source or changing the production parser.

Column headers declare UTF-16, byte, integer, optional integer and boolean
fields. UTF-16 uses printable ASCII plus `\uHHHH` code-unit escapes. Byte
columns use printable ASCII plus `\xHH`. Booleans are `0`/`1`; an absent
optional integer is `null`. No normalization, Unicode replacement, tree
reinterpretation or sorting is applied to reference observations.

The matching `tools/math-port/tex-<package>-source.cjs` generators run only the
pinned official implementation and emit these TSV files directly. Their
default mode checks bytes; `--write-fixture` explicitly updates accepted data.
Normal native test runs do not invoke these generators or require Node.js.

Package-specific context retained from the original fixtures:

- Mathtools uses AMS tags, `base`, `ams`, `newcommand`, `mathtools` and
  `boldsymbol`, with `maxMacros=1000`. Its order table uses `reverse=0` for
  `extpfeil,mathtools` and `reverse=1` for `mathtools,extpfeil`.
- Mathtools tags define `box=["[","]",""]` and
  `bold=["<",">","\\mathbf"]`. The sequence uses mode `all` without
  tagformat; actions `0`–`5` mean keep, select box, replace box with
  parentheses, reselect, clear, and select bold.
- Stateful sequence order is part of the fixture. Do not sort or split it
  into independently initialized cases.

`mhchem/` also retains the ordered replacement and mutation stages, alias and
creator observations, recovery errors, and the retired/untyped mutation labels.
The six stage columns remain in their original order; the source corpus SHA-256
is kept in `mhchem/cases.tsv`. All 309 records are loaded lazily by the package
adapter, including the one-record mutation-input and creator tables.
