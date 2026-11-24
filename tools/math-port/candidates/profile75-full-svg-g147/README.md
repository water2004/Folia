# Full-profile 75-case SVG comparison candidate

This candidate compares complete SVG output from MathJax 4.1.3 CJS with the
public native `folia.math::Renderer`. It uses the locked 75-record input
inventory at `build/math-port/common-layout-profile-root-locked.json` (18
baseline cases followed by the 57 cases in `common-layout-reference/profile-cases.json`).
The CJS capture checks the inventory hash, scope/profile hashes, Source commit,
profile source lock, dependency closure, and configured packages.

The `continuous` group renders all cases in order through one document/session.
The `fresh-session-per-case` group creates a fresh CJS document or native
Renderer for each case. Both groups retain their labels and are compared
independently. The source run currently observes six thrown cases and ten
serialized MML errors across 150 records; the comparer uses those captured
results as expectations rather than assuming success.

After enrolling the target from `cmake-probe.patch` and building it, run:

```powershell
python tools/math-port/candidates/profile75-full-svg-g147/run-profile75-native.py `
  <path-to-FoliaMathProfile75FullSvgDump.exe>
python tools/math-port/candidates/profile75-full-svg-g147/compare-profile75-full-svg.py `
  tools/math-port/candidates/profile75-full-svg-g147/profile75-full-svg-source.json `
  build/math-port/profile75-full-svg-native.json
```

`verify-comparer-tool.py` checks the comparer’s positive and mismatch paths
with synthetic records; it does not assert renderer parity.
