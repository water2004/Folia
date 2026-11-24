# Next seven TeX packages

Pinned Source: MathJax 4.1.3, locked through `docs/math-port/scope.json`.
`tools/math-port/tex-next-seven-original-oracle.cjs` verifies every Source lock
before loading modules and emits 23 actual parser cases to
`build/math-port/tex-next-seven-original.json`. The native candidate is
`tools/math-port/tex-next-seven-native-candidate.cpp`; compare with
`tools/math-port/compare-tex-next-seven.py` once Root registers and builds it.
The seven owner pairs and native observer enter the Gate78 build; no native
parity claim is made until the actual run passes the strict comparer.

| Native module | Source owner | Responsibility |
| --- | --- | --- |
| `folia.math.input.tex.noundefined.configuration` | `input/tex/noundefined/NoUndefinedConfiguration.ts` | Fallback macro body, default options and priority. |
| `folia.math.input.tex.verb.configuration` | `input/tex/verb/VerbConfiguration.ts` | `Verb` parse method and command map. |
| `folia.math.input.tex.unicode.configuration` | `input/tex/unicode/UnicodeConfiguration.ts` | Unicode/RawUnicode/Char methods, shared cache and command map. |
| `folia.math.input.tex.upgreek.configuration` | `input/tex/upgreek/UpgreekConfiguration.ts` | Character map and normal-variant token transformation. |
| `folia.math.input.tex.gensymb.configuration` | `input/tex/gensymb/GensymbConfiguration.ts` | Unit character map and token transformation. |
| `folia.math.input.tex.fontsizev3.configuration` | `input/tex/fontsizev3/FontSizeV3Configuration.ts` | Source `BaseMethods.SetSize` references and scale arguments. |
| `folia.math.input.tex.extpfeil.configuration` | `input/tex/extpfeil/ExtpfeilConfiguration.ts` | Newextarrow method, predefined map and Newcommand configuration composition. |

The candidate uses the real ParserRuntime and MmlFactory. Its per-case package
arrays match the pinned Source fixture; maps and package data are serialized
from the actual runtime. Root enrolls the production and candidate translation
units in CMake, while the native session registers the actual package owners.

The oracle covers successful parsing plus missing/unclosed verb delimiters,
Unicode forms and errors, character decimal/octal/hex/control forms,
all map packages, `Newextarrow` success and argument errors. The current oracle
run reports 23 cases across 246 checked Source files. The `unicode` cache is a
module-level table, so its lifetime is shared across Jax instances as in the
Source module. Range validation for `\unicode` is deferred until the Source
`numeric()` call, preserving cache/stack side effects before an invalid scalar
throws; `\U` and `\char` validate at their respective Source conversion point.
