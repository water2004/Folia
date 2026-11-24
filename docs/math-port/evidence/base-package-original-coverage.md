# Base package original observations

`tools/math-port/base-package-original-oracle.cjs` independently loads the
pinned MathJax 4.1.3 CJS package. It locks these source/CJS pairs:

| TypeScript source | SHA-256 | Installed CJS | SHA-256 |
| --- | --- | --- | --- |
| `ts/input/tex/base/BaseConfiguration.ts` | `578d2c6b03f2f3e902476a0b73a5323b12262215fcbdc831a1135c1d9c1737ad` | `input/tex/base/BaseConfiguration.js` | `0feedc60ac600b647cf2a1437c7244196a5aec04715f4a301fe6e9ec09722c57` |
| `ts/input/tex/base/BaseMethods.ts` | `7336240c8f9c5037f9fdcd5a84ea079312854f1ef3f406aeb5a8b6a8ff765cda` | `input/tex/base/BaseMethods.js` | `b8d5c74e01f85ae391ec8d16ffcd216a4ca4267bd8ffa091c5bfe1462ad7b491` |
| `ts/input/tex/base/BaseMappings.ts` | `08dada0ed7fab0134da2c3a341e30637959ebb730512835b9482d7c78d72adae` | `input/tex/base/BaseMappings.js` | `3522fc3fbb50af2f65442be3b2fbb2623824620899ebe775a87b885928c4acce` |

The generated JSON contains the emitted 75-method `BaseMethods` names and
function arities, the `BaseConfiguration` record fields and values, the active
configuration handler keys, and all 14 registered token maps with their CJS
class, parser, regex, and static entries. Formula observations cover common
identifier/operator parsing, scripts, limits, fractions, roots, fences, text,
spacing, accents, arrays, and errors. The environment-only `\breakAlign`
case remains recorded as an error. The equation-environment path is not
represented as a valid fixture:
it requires the owning MathItem supplied by the full TeX input-jax pipeline.

The JSON is regenerated under ignored `build/math-port` by running:

```powershell
node tools/math-port/base-package-original-oracle.cjs
```

This corpus is a starting point for differential testing, not exhaustive
coverage of every method branch or an acceptance claim.

An additional method-focused corpus is maintained independently in
`tools/math-port/base-methods-original-oracle.cjs`. It records 37 real parser
observations, including explicit Base-only error cases and package-specific
AMS, boldsymbol, newcommand, and configmacros profiles. The corresponding
native full-tree candidate is
`tools/math-port/parser-family-reference/base-methods-native-candidate.cpp`;
`tools/math-port/compare-base-methods.py` compares the complete normalized
MML tree, parser state, function-member order, error shape, and selected package
list. The native comparison has not yet been run or accepted. Package-specific
cases remain useful source gold even while their native package owners are
incomplete.
