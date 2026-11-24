# TextMacros native implementation checkpoint

The C++ source files map directly to MathJax 4.1.3 `TextMacrosMethods.ts`,
`TextMacrosMappings.ts`, and `TextMacrosConfiguration.ts`:

- `textmacros_methods.cpp` implements the text parser methods and stores their
  canonical callable table in `ParserRuntime::textMacrosMethods`. Base-package
  aliases are fetched from `BaseMethods::get(runtime, name)` so the same
  FunctionRef reaches both map registrations and the base package.
- `textmacros_mappings.cpp` registers the actual `MacroMap` and `CommandMap`
  objects and their Source command arguments.
- `textmacros_configuration.cpp` creates the actual `text-base` and
  `textmacros` Configuration objects, builds the inner ParserConfiguration and
  ParseOptions, aliases the outer options/packageData owners, and installs the
  internal-math and preprocessor callbacks.
- `src/math/util/retries.cpp` carries retry control through the actual native
  queue and Promise services; `CheckAutoload` throws a retry-bearing typed
  error after the Source TeX parse call.

Source behavior is observed by `textmacros-package-original-oracle.cjs`,
`retries-original-oracle.cjs`, and `textmacros-native-tree-oracle.cjs`. The new
tree oracle locks the same original CJS modules and records normalized full
MML trees (kind, inferred/token state, token text, all attributes/properties,
and ordered children) for 37 real TexParser/TextParser inputs. The matching
`compare-native-frontend.py` requires exact case coverage and reports the
first structural mismatch. `native-frontend-dump.cpp` writes native trees from
actual owners to an ignored build path for that comparator; Root still needs to
wire and compile the dump/comparison units.

`textmacros-native-candidate.cpp` boots actual
Runtime, BasePackage, TextParser, TextMacros, TeX, ParseOptions, InputJaxFilterData,
and MmlFactory owners; its runnable cases parse `x+1` through the actual TeX
parser, run 17 registered text-macro mappings and BaseMethods aliases through
that same parser, and parse `\\bf bold`, `\\it italic`, and `\\tiny small`
through TextParser. They check MML production, consumed input, parser cleanup,
and inherited attributes. This expanded candidate has not yet run through Root's
shared build gate.
`retries-native-candidate.cpp` covers all five locked Source
observations over actual native Promise and JobQueue objects. These candidates
are in Root's dedicated `FoliaMathParserTests` target. The production archive and
retry gate pass. The parser candidate's first compile exposed a bad module
import, missing node-util import, and parser-list variant access; those errors
have been corrected. No parser candidate result is accepted yet.

The TextParser module hook is registered by `ParserRuntime`; package
initialization still must invoke TextMacros setup in Source order. Context-visible
TextParser own-field and MML node identity adapters also remain integration
dependencies. This checkpoint does not claim full TextParser, TeX input, or SVG
acceptance.
