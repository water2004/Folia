# TeX frontend native candidates (not acceptance)

The expected results come from the actual pinned CJS observers:

- `tools/math-port/tex-parser-original-oracle.cjs` -> `tools/math-port/profiles/evidence/tex-parser-original-source.json`
- `tools/math-port/parse-util-original-oracle.cjs` -> `tools/math-port/profiles/evidence/parse-util-original-source.json`
- `tools/math-port/text-parser-original-oracle.cjs` -> `tools/math-port/profiles/evidence/text-parser-original-source.json`

These are candidate comparisons only. No native fixture has yet exercised the
full set of methods through a real Context instance graph, so class names or
prototype coverage must not be reported as semantic acceptance.

## TexParser and TeX

Use the actual runtime `TeX`/`ParseOptions`/`StackItemFactory`/`Stack`/`TexParser`
and `MmlFactory`, retaining each object identity. The TexParser source rows
cover: constructor registration and stack initialization, `mml`'s separate
`Top()` reads, own `isInner` delete/copy phase, null environment failure before
parser push, live options/factory/tags getters, UTF-16 code-point and control
sequence movement, argument/bracket/dimension/token errors with consumed index,
nested `ParseArg`/`ParseUpTo`, `create` token class side effects, inferred-node
push, stack-item source spans, and live handler lookups. The TeX rows separately
cover actual `configure`, `tags`, `reset`, `findMath`, format-error MML, successful
compile and source callback ordering.

Candidate native rows must compare each observation's full result and exception
before/after state, including partial state on failure. Required retained
identities include the actual input jax, its ParseOptions, nested parser,
Stack, StackItems, MmlFactory and produced MML nodes. Current blockers are the
Context MathItem/MathDocument bridge, Context MML-node identity, TextParser
constructor registration, and TextParser live own-field access. TeX
`compile`/`formatError` Context rows remain pending until the Root-owned MathItem
and MML identity bridges exist. Direct typed calls do not satisfy those rows.

## ParseUtil

Candidate rows are the 19 labels in `parse-util-original-source.json`. They
exercise actual node factories and include a live `fixInitialMO` input slot,
callback selection for `internalMath`, math/box delimiter parsing, Source
captured arrays, Macro input errors and counters, list-copy identity, current
environment reads, option descriptor callbacks, and real NFD character tests.
Use the real TexParser, ParseOptions, Stack, NodeFactory, MmlFactory,
Attributes, NodeUtil, UnitUtil and UnicodeNormalization services. The
`internalMath` case is specifically a canonical registered `FunctionRef` whose
receiver is the live options record and whose return is the actual MML
`Children` array; it cannot be replaced by an inlined approximation or a copied
Context array. Its TextMacros callback must use the same actual TextParser and
MML products.

## TextParser

The twelve labels in `text-parser-original-source.json` exercise emitted class and
accessor descriptors, inheritance from the exact TexParser constructor,
actual TextMacros package configuration, callback-driven internal math,
script-level wrappers, empty/single/multiple node returns, text flushing,
token attributes, StopItem/StyleItem behavior, inferred math wrapping,
node-list transfer, nested argument parsing, and TexError formatting. The
successful callback observations use the actual `TextMacrosConfiguration`,
ParseOptions, TexParser, TextParser, StackItemFactory, NodeFactory and MmlFactory.

The candidate fixture must assert the instance identity is a TextParser whose
prototype inherits the actual TexParser prototype; the textmacros record's
`texParser` points to the exact main parser; inner and outer `nodeLists` arrays
contain the same MML nodes; and `internalMath` returns the actual one-element
MML array from its canonical FunctionRef body. Compare node-tree shape and
attributes, parser-stack length, source string index and exception partial
effects. Never create an alternate ParseOptions, TextParser, MML node or
TextMacros record to make a case pass.

The remaining adapter dependencies are concrete: Context-visible TextParser
own fields (`text`, `envStack`, `level`, and `nodes`) need live bindings to the
same typed members; NodeValue results/arguments need the Root-owned MML identity
binding; the runtime must register the real TextParser constructor once before
any package callback; and the TextMacros package initializer must install the
canonical callback returned by `TextParser::internalMathFunction` only for the
actual package configuration. The current typed implementation alone cannot
prove Context-level class descriptor or field parity while those bindings are
absent.
