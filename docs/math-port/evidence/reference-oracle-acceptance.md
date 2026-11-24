# Root-owned reference oracle: local acceptance

Status: reference entry/regression witnesses accepted; complete native MathJax
renderer and full release gates remain **not accepted**.

## Frozen inputs and observations

`../reference-corpus.json` contains 64 ordered cases: 50 package proposals
independently executed by root across the unchanged 38-package Folia profile,
plus 14 root-authored mechanism regressions. It preserves the production
session's configuration lifetime and request order, including macros and tags;
these are not 64 independently reinitialized sessions. The bbm version case
explicitly restores normal mode. Scope-restoration evidence uses an outer
macro `a`, local override `b`, then restored `a`, with expected output `b+a`.

The oracle checks the cloned commit, all 246 inventoried source hashes, installed
source equality, production profile and dependency lock hashes, compiled source
hashes, all four installed font CJS module-tree hashes and the external
mhchemparser 4.2.1 ESM module tree. The frozen manifest records tool hashes and
Node version. Changing a source/profile/tool requires root review and a separate
expectation change; normal verification never rewrites expected results.
Tracked oracle tool/corpus hashes canonicalize physical CRLF to LF so Git's
Windows checkout conversion does not change the fingerprint. Snapshot values
and serialized TeX/SVG strings still compare character-exactly.

Snapshots retain semantic node identity/parent/children, four attribute layers,
resolved attributes, classes/properties/cores, ordered postfilter trees,
per-wrapper full-precision boxes and line boxes, placement calls and glyph
queries. SVG observations retain the complete container, every fragment and
separator, all XML elements/attributes/text, and raw serialized output. In
particular, stretched delimiters retain their nested SVG viewports; they are
not reduced to paths and rectangles.

All 64 serialized results or exceptions equal a fresh process using the same
production profile **without instrumentation**. Lazy box/line probes are
explicitly tagged as after-probe observations, not an untouched runtime trace.
Glyph-query observations include queries caused by those probes.

Full snapshots are regenerated as gzip JSON in ignored
`build/math-port-reference/`; committed `reference-golden.json` pins a complete
snapshot hash and individual semantic/layout/result layer hashes per case.
There is no truncated snapshot or path-only golden. After hash verification,
`reference-compare.py` can compare the full regenerated expected snapshot to a
future native snapshot. Numeric tolerance is limited to listed BBox geometry
and placement coordinates (absolute 1e-10 plus relative 1e-10). Other values,
including numeric priorities, classes and codepoints, compare exactly.

## Independently exercised comparator

All 64 self-comparisons pass. Eleven isolated mutations are detected:
missing second fragment, wrong variant, shifted glyph, wrong fraction-rule
thickness, altered nested viewport clipping, leaked local macro result,
dropped MML attribute, wrong BBox, missing postfilter, wrong diagnostic and
changed numeric priority. Each changes only its relevant recorded representation,
leaving redundant SVG serialization unchanged, so structured comparison itself
must find the defect. A 1e-12 BBox perturbation passes the documented tolerance;
a 1e-12 priority change fails exact comparison.

Run, in order:

```powershell
python tools/math-port/inventory.py --check
node tools/math-port/reference-fixtures.cjs
python tools/math-port/reference-mutations.py
```

`--freeze` is a root-only expectation-authoring action, not a test repair step.
Native implementation packets have no permission to use it or edit goldens.

## Rejected claims and remaining work

This corpus provides entry witnesses, **not** every registered handler or
retained method/branch. Stack/group/environment state transitions and full
configuration API fixtures still need explicit probes; comparing macro output
does not prove the entire parser mechanism. Function/symbol debug tags are not
proof of executable identity or function-body equivalence. Font queries are not
an exhaustive G3 resolver/delimiter/extension comparison. There is no accepted
native scene, SVG renderer, raster comparison or production switch here.
At this oracle's initial freeze, three prerequisite source files had native
acceptance and 243 inventory entries were unreviewed. Subsequent P4/P5 acceptance
is recorded separately in `numeric-acceptance.md` (four reviewed source files,
242 pending); it does not expand this corpus's parser/layout coverage. All remaining inventory
entries remain unreviewed, and non-relative/external dependencies still require
their own source-to-target disposition. Full G0 and later waves remain pending.

Root execution corrected these proposal errors and exposed profile gaps:

- `\mathtip` parses valid TeX but fails SVG host event binding with
  `TypeError: node.addEventListener is not a function`. `\texttip` emits a title
  and renders. A native action host adaptation still needs a written contract;
  neither a silently dropped tooltip nor reproducing a host bug proves parity.
- The original numcases `\text` column sample produces MathMacro; the corrected
  literal text-column witness renders. Both outcomes are recorded.
- configmacros with no configured macros exercises noundefined policy only;
  texhtml with default `allowTexHTML=false` renders literal markup as TeX.
  Positive configuration/HTML fixtures remain untested and must not be claimed.
- mhchem arrows emit `Invalid variant: -mhchem`: the upstream component attaches
  `mathjax-mhchem-font-extension`, but the current production entry attaches only
  bbm/bboldx/dsfont. The reaction snapshot records this defect; it is **not** an
  approved font omission or final chemistry expectation. Resolve the font
  dependency and review the resulting profile/manifest before G3 or release.

Folia retains its existing single production backend until native gates permit
the replacement. These development tools are never native runtime dependencies.
