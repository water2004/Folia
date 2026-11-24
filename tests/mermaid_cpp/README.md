# Portable diagram tests

The native suite follows the same boundaries as the library. Every original
test remains registered, with independent observations kept in `fixtures/`.

| Directory | Responsibility |
| --- | --- |
| `api/` | Renderer entry points and diagram registration |
| `frontend/` | Source text, frontmatter, directives and numeric parsing |
| `config/` | Configuration values and session behavior |
| `themes/` | Theme colors, options and renderer fallbacks |
| `labels/` | Shared text/math label behavior |
| `geometry/` | Shared geometric primitives |
| `layout/dagre/`, `layout/elk/`, `layout/cose/`, `layout/fcose/` | Layout-engine contracts and regressions |
| `diagrams/<family>/` | Parsing, graph models, layout and regression cases specific to a diagram family |
| `support/` | Independent fixture codecs |
| `fixtures/` | Existing official TSV observations, including theme and layout corpora |

```sh
python tests/run.py --suite mermaid
python tests/run.py --suite math --suite mermaid
python tests/run.py --suite tooling
```

The first command builds the portable diagram library and all its native tests.
Selecting both `math` and `mermaid` also enables `FoliaMermaidMathTests`, which
uses the real Math shared-library boundary. The portable entry excludes the
Windows/DirectWrite host targets; their existing tests remain available through
the main project and dedicated Windows entry points.

CMake discovers native cases recursively. Fixture locations are absolute paths
set by the test target, so moving a test or changing the process working
directory does not change the data it reads.

## Independent visual comparison

`tools/mermaid-cpp/regression/tests/` covers the Python comparison policies,
including their rejection of mutated geometry, ordering, styles, colors,
incomplete corpora and tampered raster evidence. `compare-family.py` is the
common entry for all diagram families. Canvas and formula-fragment comparisons
also support saved observations, allowing acceptance checks without Node.

JavaScript observation scripts call the pinned official Mermaid/MathJax or
raster backends to generate independent samples. Python owns native execution,
comparison, tolerances and the final pass/fail decision. For capture prerequisites
and full-corpus commands, see `docs/mermaid-cpp/regressions.md` and the comparison
documentation beside those tools.
