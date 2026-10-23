# Pinned cmark-gfm generated parsing cases

These tests adapt two non-paired inputs from cmark-gfm commit
`27d942c8b0a62d192f616e5bf3578f4b6a89e180`. They use Folia's parser and
block-local source/CST; cmark is data provenance only and no cmark library or C
API is linked. The entity source is cached at
`build/dependencies/markdown-conformance/gfm/entities.inc`, fetched from its
pinned URL and SHA-256 checked through `gfm-sources.lock.json`. Its SHA-256 is
`3932dfa630067813f47be6ae0072e1645c1183bde41140b51443f50857c88909`.
Regenerate the 2,125-row C++ fixture with:

```powershell
python tools/markdown-conformance/generate_pinned_gfm_entities.py --fetch
```

For each named entity, `test_gfm_entities.cpp` checks that parsing creates an
Entity CST node, the visible value is the pinned Unicode value, CST tokens
flatten and serialize to the exact source, and Markdown save returns that same
source. Upstream `entity_tests.py` checks rendered HTML substring values and
has output escaping exceptions for `quot`/`QUOT`; Folia checks decoded text
semantics directly, so HTML renderer escaping is outside this parser test.
The pinned `nvlt`/`nvgt` combining overlay values are retained exactly.

`test_gfm_pathological.cpp` ports all 21 active generator entries from
`pathological_tests.py` at their original scales. Each one asserts parsing
completes, every editable block-local CST remains lossless, and save preserves
the original Markdown. The original cmark HTML regular expressions are
translated into exact character patterns or counts against the existing Folia
CST-to-HTML semantic projection; no second Markdown parser or cmark renderer is
involved. `run_gfm_pathological.py` starts every named test in a separate
process with a 30-second timeout, verifies Boost.UT reports exactly one
matched test, and writes per-case statuses and bounded metadata as JSON. This
target is excluded from the default core suite so an unusually large input
cannot hold the ordinary test console. CMake generates the entity fixture in
the binary tree at `gfm-entities/gfm_entity_cases.inc` as a `FoliaTests`
dependency.

| Upstream generator | Original scale / source bytes | Parsing behavior exercised |
|---|---:|---|
| nested strong emphasis | 65,000 nested pairs / 910,001 | nested delimiter pairing |
| many emphasis closers without openers | 65,000 / 195,000 | unmatched closing delimiters |
| many emphasis openers without closers | 65,000 / 195,000 | unmatched opening delimiters |
| many link closers without openers | 65,000 / 130,000 | unmatched closing brackets |
| many link openers without closers | 65,000 / 130,000 | unmatched opening brackets |
| mismatched openers and closers | 50,000 / 200,000 | competing emphasis markers |
| opener/closer count multiple of three | 50,000 / 150,004 | repeated delimiter-run handling |
| link openers and emphasis closers | 50,000 / 200,000 | mixed unmatched constructs |
| `[ (](` repeated | 80,000 / 400,000 | bracket/parenthesis scanning |
| `![[]()` repeated | 160,000 / 960,000 | image and nested bracket scanning |
| hard link/emphasis case | one / 16 | emphasis delimiters crossing a link label |
| nested brackets | 50,000 each side / 100,001 | deep bracket balance |
| nested block quotes | 50,000 markers / 100,001 | deep block quote prefixes |
| deeply nested lists | 1,000 levels / 1,003,000 | indentation and nested list structure |
| U+0000 input | one seven-byte source | embedded NUL source handling |
| backticks | 4,999 runs / 12,502,499 | long unmatched code delimiter runs |
| unclosed angle links | 30,000 / 180,000 | repeated incomplete destinations |
| unclosed parenthesis links | 30,000 / 150,000 | repeated incomplete destinations |
| unclosed HTML comments | 300,000 markers / 1,200,002 | repeated comment-like text |
| table line endings | 30,000 rows / 330,000 | CR, LF and vertical-tab boundaries |
| reference-map collisions | 49,999 definitions/references / 1,143,036 | reference lookup under collisions |

The source also contains a commented-out `many references` generator (79,999
definitions plus 5,000 uses). It remains disabled upstream and is not counted
among the 21 active cases. cmark `api_test/main.c` cases exercise its C object
model, renderer APIs, UTF-8 validator, streaming feed API and source positions;
those are intentionally outside Folia Markdown parser conformance.

Build and run the isolated target with `cmake --build build/core --target
FoliaGfmPathologicalTests` followed by `ctest --test-dir build/core -R
FoliaGfmPathologicalTests --output-on-failure`. No build was run while preparing
this adaptation.
