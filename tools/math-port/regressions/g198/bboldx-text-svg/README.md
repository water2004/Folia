# Text-mode BBoldx SVG comparison

This regression covers Latin and Greek text-mode double-struck commands,
dotless aliases, and explicit `bboldx.light=true`. The two default inputs use
the public Renderer; the configured input uses a real NativeRenderSession with
the locked 38-package profile, actual document conversion and completed Scene.
Source uses the pinned MathJax 4.1.3 CJS modules and verifies their source lock.

The three inputs each run in continuous and fresh-session groups. Continuous
default rows reuse a Renderer/document; the different light configuration has
its own document. The comparison checks configuration, complete SVG/error,
and native Scene glyph codepoints and outline paths against Source SVG paths.
It captures the native Scene but does not claim a complete cross-backend
comparison of every Scene field.

Build `FoliaMathBboldxTextSvgProbe` in the existing `build/core` tree, then run:

```powershell
python tools/math-port/regressions/g198/bboldx-text-svg/compare.py build/core/FoliaMathBboldxTextSvgProbe.exe
```

The original candidate was compiled against unchanged Gate198 modules and
passed all six exact SVG and glyph-path comparisons. The first execution
exposed a test protocol mismatch (the plain configuration tag was decoded as
base64); that fixture was corrected and the probe rebuilt before acceptance.
The failed attempt is retained. Root provenance for the passing attempt is
`build/math-port/bboldx-text-svg-comparison-g198-r2.provenance.json`.
The subsequent CMake registration and promoted path require a configured
target build in the next shared gate.
