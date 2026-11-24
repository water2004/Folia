# Clean-realm Source capture candidate

The G149 Source and native captures remain unchanged evidence of what happens
when many new `MathJax.document()`/SVG instances share one loaded CJS module
realm. The G149 comparer’s four fresh-group failures are caused by that realm
reuse, not by different fresh Renderer output.

`capture.cjs` retains the 75 G149 continuous records and captures each fresh
record by launching `worker.cjs` in a separate Node process. Every worker reads
the locked 75-case inventory and profile, imports the same MathJax 4.1.3 CJS
profile, verifies the pinned profile/scope/source-lock/corpus/input hashes, and
returns one record plus its loaded dependency hashes. The parent verifies all
locked Source and CJS dependencies, validates every record against the locked
input, and checks that the union of worker dependency closures equals the
complete pinned dependency list. Output defaults to the ignored `build`
directory.

The separate module realms are required by the actual upstream font loading
boundary. In `.reference/MathJax-src/ts/output/common/FontData.ts`,
`dynamicSetup()` installs setup on class-owned dynamic-file records (around
line 802), while `loadDynamicFileSync()` sets the record’s `promise` and calls
`dynamic.setup(this)` only inside `if (!dynamic.promise)` (around lines
1305–1313). The profile reuses the class exported as `MathJaxNewcmFont`; after
the first document loads a dynamic font file, a later font-data instance in the
same module realm skips per-instance setup. That produces the mismatched glyph
metrics and `RangeError` results observed in the G149 fresh group.

`annotate-native-boundary.py` adds explicit metadata to the unchanged G149
native observation. It verifies the captured executable hash and the probe’s
two construction sites: one retained `Renderer` for the continuous group and
one new `Renderer` for each fresh row. It does not alter native output or
require a rebuild. `compare.py` requires the Source and native runtime-boundary
metadata to agree on shared-vs-isolated owner semantics before comparing all
150 SVG/error records.

Run the four-case clean-realm smoke check, the full Source capture, annotate the
existing G149 native capture, then compare:

```powershell
node tools/math-port/candidates/profile75-full-svg-clean-source-g150/capture.cjs --smoke
node tools/math-port/candidates/profile75-full-svg-clean-source-g150/capture.cjs
python tools/math-port/candidates/profile75-full-svg-clean-source-g150/annotate-native-boundary.py
python tools/math-port/candidates/profile75-full-svg-clean-source-g150/compare.py `
  build/math-port/profile75-full-svg-source-clean-g150.json `
  build/math-port/profile75-full-svg-native-boundary-g150.json `
  build/math-port/profile75-full-svg-comparison-g150.json
python tools/math-port/candidates/profile75-full-svg-clean-source-g150/verify-comparer.py
```

The synthetic comparer check tests only acceptance, boundary rejection, and
SVG-mismatch reporting. It makes no parity claim. The G149 captures and
registered tooling are read-only inputs to this candidate.
