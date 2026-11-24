# Common reference with the passive event host

The tracked G252 observer preserves the original Common instrumentation and
MathJax algorithms. It supplies the same register-only Lite event host already
used by the G213 SVG reference. The historical host-less capture and comparator
remain unchanged. All source/profile/dependency pins are checked before use.

Root independently regenerated the proposed Source capture before promotion.
After LF normalization, SHA-256 is
`8bc9c64c70a2c8a27d92b7e7535c8f80ae196ed3bb8c1b3c31db42f84f6d13f7`.
The producer exits 0 with the existing `Invalid variant: -mhchem` warning.
All 73 non-action records are deeply identical to the historical capture.
The two action records complete with their actual click/mouseover/mouseout
registrations. No callback is dispatched or mathematical output rewritten.

The strict comparison against the saved Gate247 native observation accepts
75/75 full records: BBoxes, wrappers, placement calls, paint, loads, complete
SVG, MML, warnings and errors. Native configured and registered packages must
both match all 38 Source packages, with no configuration warnings.
This reproduces Gate247 evidence; it does not certify a newer native binary.

Reproduce from the repository root:

```powershell
node tools/math-port/regressions/g252/common-passive-source.cjs --profile-corpus > build/math-port/common-passive-source.json
python tools/math-port/regressions/g252/compare-common-passive.py build/math-port/common-passive-source.json build/math-port/g247-layout-font/common-native.json build/math-port/common-passive-comparison.json
```

The source JSON is an ignored generated artifact. Its expected bytes, observer,
host, original capture, profile and dependency hashes are in the tracked lock.
The four tracked witness files use LF explicitly to preserve those byte locks.
Root's final independent artifacts are
`build/math-port/common-passive-source-lf-root-g252.json`
and `build/math-port/common-passive-lf-comparison-root-g252.json`.
The initial draft used CRLF; normalizing the pinned scripts to LF changes only
the observer provenance hashes in the capture, and the full-record comparison
still accepts 75/75. Working-tree and Git blob hashes now agree.
