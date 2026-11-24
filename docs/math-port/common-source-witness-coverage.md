# Common pinned Source witness coverage

These standalone witnesses exercise existing MathJax 4.1.3 Common behavior. They are evidence for the named Source cases, not native behavior or complete Common/renderer parity. All generated JSON and logs are written beneath ignored `build/math-port/common-source/`.

| Candidate formal tool | Source and exercised behavior | Limits |
| --- | --- | --- |
| `tools/math-port/common-factory-source-witness.cjs` | Fixed hashes for Common `Wrapper.ts`, `WrapperFactory.ts`, Tree `WrapperFactory.ts`, and SVG/CHTML wrapper catalogs; checks package versions, extracted common style/variant defaults, empty generic registry, wrapper initialization and explicit-parent side-effect order, and 38 SVG/36 CHTML catalog entries. | Confirms source declarations and profile tables; does not instantiate native wrappers or exercise a renderer catalog. |
| `tools/math-port/common-jax-source-witness.cjs` | Runs pinned 4.1.3 CJS CommonOutputJax methods through a real MathML→SVG Source document. Deep-compares metrics, CSS font data, font styles, linebreak width/marking and cleanup, nested parent marking, and unknown-text cache results against the existing expected JSON. Pins the Common TS/CJS hashes and package versions. | Uses the upstream SVG renderer as a Source harness. It does not compare native Common layout or the final SVG output. |
| `tools/math-port/common-variant-source-witness.cjs` | Calls pinned Source `CommonWrapper.getVariant` for invalid/null/valid explicit variants and font-family precedence, then compares the existing expected fixture. Validates `scope.json`, locked Common TS/CJS source, and MathJax/NewCM package versions. | Focused method calls with small receiver fixtures; no full wrapper lifecycle or renderer comparison. |
| `tools/math-port/common-text-source-witness.cjs` | Runs nine real MathML→SVG cases for Unicode `mi`, leading-minus `mn`, scaling, quoted `ms`, `mspace`, `mglyph`, explicit font selection, mtext widths, and display linebreaking. Compares wrapper records and geometry to the original expected JSON; checks locked TypeScript/CJS files, scope and package versions. | These nine cases cover selected token/text behavior and wrapper measurements, not all text input, line-breaking, or font cases. |
| `tools/math-port/common-row-source-witness.cjs` | Runs a pinned TeX→SVG row with several break opportunities; traverses actual `CommonMrow` wrappers and checks that source-created rows and line breaking occur. Locks `mrow.ts`, `LineBBox.ts`, their CJS outputs, and package versions. Writes the observed row geometry under `build/`. | The existing witness has semantic assertions rather than an external expected JSON snapshot; it does not compare every line geometry field to a checked-in baseline. |
| `tools/math-port/common-mo-source-witness.mjs` | Executes pinned `CommonMoMixin` methods with the narrowly scoped test support module. Checks dimensions/symmetry, baseline, full-extent rounding, fixed size/schar selection, zero-size truthiness, mathaccent size adjustment, assembly metrics, and linebreak style. Pins `mo.ts` hash and `@mathjax/src` version. | Dependency doubles are explicit test support; called Mo methods come from Source. This focused method witness does not exercise a full font repository or MML renderer. |

The row witness also reflows its actual broken row to widths 6, 8, and 10 with
left alignment and a 1em indent. Every line must retain L=1 at each width;
this checks that `shiftLines` clears the prior left offset before realignment.

`common-inline-policy-source-witness.cjs` pins Common/SVG TS and CJS hashes and
executes three real SVG configurations plus same-document display/enable
transitions. It checks the Source Common and SVG forceInlineBreaks getters,
marking, recursive cleanup and conversion-entry ordering. It compares the
checked-in inline-policy fixture and writes observations under ignored build.
It does not compare final SVG bytes or prove the native linebreak algorithm.

The Mo loader intercepts imports only when their parent is pinned `ts/output/common/Wrappers/mo.ts`; it returns the named test support module. The witness imports that support module directly only to supply the typed `CommonWrapper` base and `BBox` used by the fixture. The stubs do not replace `CommonMoMixin` or its tested methods.

Source hashes and expected observations are scoped to the repository's pinned MathJax 4.1.3 profile. Passing these scripts does not establish native ownership, native Common integration, complete layout parity, SVG parity, or Folia application integration.

## Fraction and radical method pairs

| Formal tool | Source and exercised behavior | Limits |
| --- | --- | --- |
| `tools/math-port/common-fraction-source-witness.cjs` | Executes pinned MathJax 4.1.3 `CommonMfrac` methods (`computeBBox`, `getTUV`, `getUVQ`, bevel/fraction BBox helpers and wrap widths) and SVG fraction placement methods for all 288 existing paired inputs. Checks the existing fraction fixture SHA-256 and row count, validates Source TypeScript against `scope.json`, and checks the four pinned Common/SVG/BBox CJS hashes. Writes `build/math-port/common-source/fraction-source.json`, invokes `layout-source-table.cjs`, then compares every generated header and row scalar (ignoring provenance comments only) with the tracked table. The last verified table has 288 rows and 84 fields per row. | This is isolated method pairing with synthetic BBoxes and a `length2em` stub that receives already-converted em values. The SVG methods run against fixture wrappers; this is not complete MathML, font measurement, SVG document, Renderer, or Folia output parity. |
| `tools/math-port/common-radical-source-witness.cjs` | Executes pinned `CommonMsqrt`/`CommonMroot` methods (`getPQ`, `getRootDimens`, `rootWidth`, `getStretchedSurd`, `computeBBox`, `rootHeight`) and SVG `addRoot`/`toSVG` placements for all 192 existing paired inputs. Checks the existing radical fixture SHA-256 and row count, validates Source TypeScript against `scope.json`, and checks the four pinned Common/SVG wrapper CJS hashes plus the executed `util/BBox.js` hash. Writes `build/math-port/common-source/radical-source.json`, invokes `layout-source-table.cjs`, then compares every generated header and row scalar (ignoring provenance comments only) with the tracked table. The last verified table has 192 rows and 89 fields per row. | This is isolated method pairing with injected base, degree, and surd BBoxes plus synthetic SVG wrappers. It does not measure real delimiter glyph outlines, run complete MathML/Renderer layout, compare final SVG bytes, or establish Folia integration. |

Both scripts accept `--repo-root <repository>` and also locate the repository from their own path, so they work from any current directory. The 288/192 fixture files are checked in place and remain unchanged. Reproduction from any working directory: `node <repo>/tools/math-port/drafts/native-raii/common-arithmetic-formal-tools/common-fraction-source-witness.cjs --repo-root <repo>` and the corresponding `common-radical-source-witness.cjs` command. Each emits a `*-current.txt` table under ignored `build/math-port/common-source/` and fails with the first differing case id, column, and generated/tracked value.

## Scripts, limits, fences, actions and linebreaks

| Formal tool | Pinned Source behavior and evidence | Limits |
| --- | --- | --- |
| `tools/math-port/common-scripts-source-witness.cjs` | Checks the MathJax 4.1.3 CJS locks stored in `tests/math/fixtures/common-source/scripts-source-lock.json`, then executes the actual `MmlFactory`, `CommonScriptbase`, `CommonMsub`, `CommonMsup`, `CommonMsubsup`, and `CommonMmultiscripts` mixins. Exercises sub/sup selection and offsets, cached UVQ, script/prescript indexing, and writes `build/math-port/common-source/scripts-source.json`. | Uses isolated wrapper classes and hand-supplied font parameters/BBoxes. It does not measure fonts or run full MML layout, SVG output, or Folia integration. |
| `tools/math-port/common-limits-source-witness.cjs` | Checks pinned CJS and TypeScript hashes from `tests/math/fixtures/common-source/limits-source-lock.json`, executes actual `CommonMunder`, `CommonMover`, and `CommonMunderover` mixins with MathJax `BBox`, and deep-compares its eight-case report with `limits-source-expected.json`. Evidence goes to `build/math-port/common-source/limits-source.json`. | The wrappers, metrics, child boxes, and length conversion are controlled fixtures; this is an isolated method oracle, not real token measurement, final SVG, or application output. |
| `tools/math-port/common-fence-action-source-witness.cjs` | Checks the pinned `mfenced.ts`/`maction.ts` and installed CJS hashes from `tests/math/fixtures/common-source/fence-action-source-lock.json`; executes actual `CommonMfenced` and `CommonMaction` mixins. Asserts generated child order/parent/stretched state, selected-child clamping and fallback, tip offsets, BBox, line BBox, and break count. Writes `build/math-port/common-source/fence-action-source.json`. | Factory, wrapper catalog, MML nodes, BBoxes, and line boxes are intentional stand-ins. It does not establish complete Mfenced/Maction construction or rendered output parity. |
| `tools/math-port/common-linebreak-source-witness.cjs` | Pins `LinebreakVisitor.ts` and `MmlNode.ts` through `scope.json`, validates installed CJS against the MathJax profile lock, and checks fixed penalties/break results in `linebreak-source-expectations.json`. Executes the actual Source visitor for inferred-row previous-operator search (including 8800/3300 cases), overfull selection, applied break styles, recursive fraction/radical requests, signed unmatched-close depth, and mspace penalties. Writes `build/math-port/common-source/linebreak-source.json`. | This is an isolated visitor-method witness with explicit geometry/wrapper fixtures, not full LinebreakVisitor integration into Common layout or final SVG. |

All four scripts accept `--repo-root <repository>` and otherwise locate the repository from their own path, so they run from any current directory. From an unrelated directory, run each command with the same repository path:

```text
node <repo>/tools/math-port/common-scripts-source-witness.cjs --repo-root <repo>
node <repo>/tools/math-port/common-limits-source-witness.cjs --repo-root <repo>
node <repo>/tools/math-port/common-fence-action-source-witness.cjs --repo-root <repo>
node <repo>/tools/math-port/common-linebreak-source-witness.cjs --repo-root <repo>
```

All generated reports are under ignored `build/math-port/common-source/`; none of the witnesses edits Source or tracked expectations.

## Table and containers

`common-table-source-witness.cjs` checks locked table TypeScript/CJS, the
unchanged 14-case SVG corpus, and NewCM versions/hashes. It preserves all 14
renderer observations in `table-svg-source.json`; those observations do not
have a checked-in whole-output SVG baseline. The companion branch witness
exactly compares eight table records against `table-branch-source-expected.json`
and writes actual MML tree snapshots and measurements to `table-branch-source.json`.
The branch cases cover labels, fixed/percent/equal widths, frame spacing,
stretching, text breaks, and expression breaks. Several width cases deliberately
replace cell content with measured mspaces; these are explicit numerical inputs.

`common-containers-source-witness.cjs` runs the isolated Mpadded/Semantics/TeXAtom
method checks and six actual MathML/TeX→SVG/NewCM observations against
`containers-source-geometry-expected.json`. The real cases cover relative padding,
overflow, semantics presentation/operator projection, italic glyph correction,
and the TeXAtom inferred-row boundary. Method fixtures use supplied child boxes;
the six renderer cases use real upstream wrappers and fonts.

Both entrypoints accept `--repo-root`, use locks/expectations in
`tests/math/fixtures/common-source/`, and write only ignored evidence under
`build/math-port/common-source/`. The helper tools reside beside the entrypoints
and do not depend on ignored implementation packets. These Source observations
do not by themselves establish native behavior, full family coverage, or Folia
integration. Root replayed both formal tools from `C:\Windows`, exit0; logs are
`build/math-port/common-{table,containers}-formal-source.log`.

`common-scripts-real-source-witness.cjs --repo-root <repository>` additionally
executes four actual MathML→SVG/NewCM trees (msub, msup, msubsup, mmultiscripts).
It locks Source/CJS/font dependencies in `scripts-real-source-lock.json` and
deep-compares all captured child/role/root boxes, inherited attributes, inline
policy, and offsets against `scripts-real-source-expected.json`. Generated
observations go only to `build/math-port/common-scripts-real-source-actual.json`.
Root replayed the formal tool from `C:\Windows`, exit0, recorded in
`common-scripts-real-formal-source.log`. This is stronger than the isolated
scripts method witness for these four trees; it does not establish every script
shape, final native SVG, or application integration.

## Menclose notation and projected radical

`common-menclose-source-witness.cjs --repo-root <repository>` locks the profile
and relevant TS/CJS files, then executes CommonNotation and CommonMenclose
methods. The parameter observation uses actual CommonWrapper.length2em and
the pinned lengths utility: `2mu` becomes exactly `0.1111111111111111`, with
box extenders `0.6111111111111112`. Its scale, rule thickness and pxPerEm are
explicit fixture inputs. The separate createMsqrt observation uses controlled
wrapper/factory services; native projection tests additionally exercise the
actual document arena, original MML parent links and wrapper index replacement.

`common-menclose-geometry-source.cjs` executes all 28 decorative SVG handlers
for two controlled child boxes (positive and negative height). It uses the
actual Source zero-initialized CommonWrapper.getBBox/computeBBox lifecycle,
BBox and fixed formatting. The 29th catalog entry, radical, is the projected
child case rather than a decorative handler. Raw Source SVG, BBox, TRBL and
ordered geometry are retained in the locked JSON. Normal replay compares it
read-only and writes observations/header only under ignored build output.

`common-menclose-compare-svg.py --repo-root <repository> --dump <tsv>` compares
the 56 actual native Scene-to-SVG fragments with those raw handler outputs.
Decimal values use exact reduced rationals: relative and absolute path forms
are equivalent without floating-point summation artifacts, rounding or epsilon.
Rectangles/ellipses are expanded by explicit path rules, and Source `mitre`
maps to typed `miter`; ordered transforms, paint inheritance, cap/join and arc
flags remain checked. This is semantic geometry equality, not byte equality.
`common-menclose-comparator-selftest.py` verifies an equivalent baseline and
13 rejected changes, including a `1e-15` coordinate change; direct parser tests
also check negative arc radii, overflow and raw fill semantics.

Root replayed all four formal tools from `C:\Windows`, exit0. Logs are
`common-menclose-formal-source.log`, `common-menclose-geometry-formal-source.log`,
`common-menclose-formal-selftest.log`, `native-menclose-formal-svg-compare.log`
under `build/math-port/`. These 56 controlled fragments do not establish every
notation combination, a full native TeX-to-SVG document, or Folia integration.
