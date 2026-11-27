# `regress` native ECMAScript regexp candidate audit

**Disposition:** evidence only. This is not an adoption recommendation or an integration/API freeze. The inspected checkout is `regress` 0.12.0 at commit `771e6c8fa1e472ef86c45b5206b37fa4e4494e25`; MathJax is 4.1.3 at `fb987178c2d279a99b0db5ea02e751691703955e`. The actual available reference runtime reports Node `v24.13.0`, Unicode `16.0`, ICU `77.1` (`node -p 'JSON.stringify({node:process.version,unicode:process.versions.unicode,icu:process.versions.icu})`).

Root review: checked the pinned API flag parser, from_unicode compilation,
UTF-16/UCS-2 entry points, and Unicode generator/version boundary. The
[pinned upstream source](https://github.com/ridiculousfish/regress/tree/771e6c8fa1e472ef86c45b5206b37fa4e4494e25)
is a research reference only; no runtime dependency or candidate acceptance was
added. In particular, Flags::new('v') sets unicode_sets independently of unicode,
while the UTF-16 entry passes compiled flags.unicode to its indexer for matching
semantics. Root's executable follow-up corrected the decoding interpretation:
Utf16Input always decodes valid pairs, regardless of that boolean. Legacy mode
must select Ucs2Input explicitly. The parser also branches directly on unicode.
Exact v-mode syntax/decoding routing needs
direct differential verification before any adapter can be frozen; a claimed
v feature is not sufficient evidence. Full matching/constructor/state behavior
and versioned tables remain open. The next callable protocol is independently
specified in ../callable-receiver-contract.md.

## Candidate behavior and boundaries

The crate is a Rust backtracking regexp engine. `README.md` describes ECMAScript 2018 syntax, backreferences, lookarounds and variable-width lookbehind, and says `v` is supported. `Cargo.toml` defaults to `backend-pikevm,std`; the `utf16` feature is off by default. The README distinguishes UTF-8 string APIs, UTF-16 decoding (`*_utf16`) and UCS-2 code-unit APIs (`*_ucs2`). This is a candidate for syntax/matching experiments, but the public interface is a Rust API, not a C ABI.

`Flags` in `src/api.rs` contains only `icase`, `multiline`, `dot_all`, `unicode`, `unicode_sets`, and an internal `no_opt`. `Flags::new` maps only `i,m,s,u,v` and silently drops all other characters. It does not reject duplicate flags or reject simultaneous `u` and `v`. The ECMAScript `d`, `g`, and `y` flags have no corresponding fields or semantics. Consequently a wrapper cannot hand MathJax's parsed flags to `Flags::from` and claim JS constructor parity: malformed flag strings, unknown letters, duplicates, and `u`+`v` must be validated before compilation; `g/y` need stateful behavior rather than being dropped.

`Regex::with_flags` and `Regex::from_unicode` return `Result<Regex, parse::Error>` for syntax errors. This gives the wrapper a place to convert compile failures into the reference's error outcome, but the Rust error is not itself a JavaScript `SyntaxError`, and the wrapper must preserve source pattern/flag validation order. The parser bounds nesting to 256 and capture groups/loops to 65,535 (`src/types.rs`); those limits are implementation boundaries to test against the accepted input budget, not ECMAScript semantic limits.

The UTF-16 feature is the relevant path for JavaScript strings. `find_from_utf16` chooses `Utf16Input` with the compiled `unicode` flag; `find_from_ucs2` explicitly chooses `Ucs2Input`. The distinction must be tested at the adapter boundary: JS strings and `lastIndex` are UTF-16 code-unit indexed; regex matching without `u/v` must advance through surrogate halves, while `u/v` must treat a valid surrogate pair as one code point. Tests in `tests/tests.rs` cover unpaired surrogate code units and paired input through UCS-2; the source also has a separate UTF-16 indexer. Enabling the crate feature is mandatory for both APIs. The ordinary `&str` API cannot represent lone surrogates and returns UTF-8 byte offsets, so it is insufficient for the fixed oracle.

The independent 119,218-case follow-up is recorded in
`p19c-regexp-candidate-experiment.md`. It found 2,347 differences even after
explicit legacy UCS-2 routing and experimental normalization of `v` to Unicode
matching. No candidate dependency or regex host was adopted.

`Match` provides whole-match range, optional capture ranges, names, and group lookup. Ranges are indices into the input representation used by the selected method (byte offsets for `&str`; UTF-16 code-unit offsets for the UTF-16 indexer). It does not return the JavaScript exec/match array, `index`, `input`, `groups` object, `indices` (`d`) array, or mutable `lastIndex`; a host wrapper must construct any needed result shape. `Regex::find_from(text,start)` is stateless and takes an explicit start; its UTF-8 form panics when `start` is not a character boundary. It is not equivalent to `RegExp.prototype.exec` with persistent `lastIndex` for `g/y`.

The crate's iterator handles non-overlapping matches and explicitly advances past empty matches to avoid looping, but this is library iterator behavior, not a stateful JS RegExp object contract. The UTF-8 `Pattern` adapter advances by byte/character boundary and its zero-width rule is plainly not a substitute for JS `AdvanceStringIndex` over UTF-16 with/without `u/v`. `replace`/`replace_all` are Rust convenience functions with their own documented replacement syntax; they do not supply JS `String.prototype.match/replace` protocol semantics.

Backtracking budget support exists only on `find_from_budgeted(&str, start, budget)`. The default `find`, `find_iter`, UTF-16 iterators and replacement APIs are unbudgeted; the candidate therefore does not give the host a uniform bounded-work guarantee across the APIs needed for JS behavior. The budget is per call, while a JS global/sticky operation would require persistent regexp state. The compiled regex is `Clone`, matches own vectors of capture ranges, and there are no C ABI ownership/lifetime contracts. A C-compatible wrapper would be additional work: opaque compiled handles, explicit UTF-16 buffers/lengths, result allocation/release, error encoding, budgets/cancellation, and stateful lastIndex all need independent design and validation.

## Syntax and Unicode risk surface

The parser contains explicit `unicode_sets` handling for nested sets, set operations and string properties; tests in `tests/unicodesets.rs` cover generated Test262-style cases, including `\p{RGI_Emoji}` and intersections/differences involving string properties. Backreferences and lookbehind have dedicated tests in `tests/tests.rs`. These are useful positive evidence, not proof of equivalence with V8 for all capture-selection, alternative-order, lookbehind, or backtracking edge cases. Differential cases should include the exact patterns and flags encountered in MathJax plus adversarial capture/backreference/lookbehind cases.

The generated tables come from `gen-unicode/src/main.rs`, whose `UCD_PATH` is hard-coded to `/tmp/ucd-17.0.0`; `gen-unicode/README.md` likewise documents downloading UCD 17.0.0 and emoji sequence files. The generated Rust file has only a generic “generated from gen-unicode” banner, not an embedded version marker. The local oracle is Node 24.13.0 / Unicode 16.0, so the candidate's Unicode 17 tables are observably from a newer Unicode dataset. V-flag emoji/string properties, binary/general-category/script properties, and case-fold tables therefore require differential checks for changed assignments, not just syntax coverage. `unicode.rs`/the generated table implementation also needs casefold comparison for legacy, `u`, and `v` modes; a version match alone would not establish V8's exact ECMAScript canonicalization.

UTF-16 storage addresses lone-surrogate input, but pattern ingestion still matters. `Regex::from_unicode` accepts an iterator of `u32`, and the normal `with_flags(&str, …)` route cannot itself carry lone surrogate code points. A JS-compatible adapter must preserve raw UTF-16 pattern units and confirm how parser escapes and literal lone surrogates map through the chosen construction path. `u` and `v` are distinct parse modes in the candidate, but no constructor-level `u`+`v` rejection was found.

## MathJax usage that constrains the wrapper

The inspected production scope is the pinned `tools/mathjax-bundle/entry.cjs` profile recorded in `docs/math-port/scope.json`; its TeX package list includes both `base` and `setoptions`. The following is source evidence from the pinned TypeScript, not a claim that every branch has already been rendered in Folia:

* `SetOptionsConfiguration.ts` recognizes a string matching `^\/(.*)\/([dgimsuvy]*)$` and calls `new RegExp(match[1], match[2])` (`filterValue`, around lines 128–139). Thus it deliberately accepts all eight standard flag letters syntactically and relies on the JS RegExp constructor to reject duplicate/unsupported/incompatible flags and invalid patterns. Any candidate adapter must preserve these error outcomes rather than let `Flags::new` discard characters.
* The same configuration defaults `allowPackageDefault` and `allowOptionsDefault` to true. Its `tex` denylist explicitly leaves `digits` and tagging options configurable (comments near lines 200–230; `digits` is not set false). `filterOption` requires that the option exists, then `SetOptions` assigns the result of `filterValue` back onto the live `parser.options` object. This makes dynamic RegExp values reachable through `\setOptions{digits=/.../flags}` in the setoptions-enabled profile.
* `BaseConfiguration` has default `digits: ''`, `numberPattern`, `identifierPattern`, `initialDigit`, and `initialLetter` regex values (around lines 224–227). During Base configuration it copies truthy `options.digits` into `options.numberPattern` exactly once, then creates digit/letter `RegExpMap`s from `initialDigit`/`initialLetter` (around lines 160–169). `RegExpMap` stores the supplied RegExp as `_regExp` and calls `_regExp.test(token)` on each `contains` (`TokenMap.ts`, around lines 147–169). `ParseMethods.digit` reads `parser.configuration.options['numberPattern']` and calls `parser.string.slice(...).match(pattern)` (`ParseMethods.ts`, around lines 81–82); `BaseMethods` reads `parser.options.identifierPattern` to pass it into multi-letter-identifier parsing (`BaseMethods.ts`, around line 424). `\setOptions` can assign any existing allowed TeX option including `numberPattern` and `identifierPattern` directly on the live options object, so those are direct dynamic RegExp-valued consumers. Keep the initialization-only `digits` → `numberPattern` alias separate: changing `digits` later only writes `options.digits` and does not itself rerun that alias assignment. Likewise later `initialDigit`/`initialLetter` writes do not replace the RegExp already captured by an initialized `RegExpMap`. `.test` on a global/sticky RegExp mutates `lastIndex`; String `.match(RegExp)` has JS `@@match` behavior and cannot be modeled as a generic stateless search.
* Other pinned TeX code uses both `String.match(RegExp)` and global `.exec` loops (`FindTeX.ts` around lines 139–164; `BaseItems.ts` around line 1263), so wrapper validation should inventory the actual profile call sites rather than expose only `find`.

For independent witnesses, use the actual setoptions path to install global/sticky `numberPattern` and `identifierPattern` regexps, then run their actual consumers twice on controlled input while observing `lastIndex` and results. Separately verify the one-time `digits` alias and that changing `digits`, `initialDigit`, or `initialLetter` after configuration does not retroactively rewire already-consumed values/maps. Also compare invalid duplicate flags, `u`+`v`, unknown flags, malformed delimiters, and invalid pattern diagnostics with fresh Node 24.13 processes.

## Recommended differential gates (not implementation directions)

Before any adoption decision, a host-level oracle should compare the candidate and Node 24.13 over:

1. Every `d,g,i,m,s,u,v,y` flag alone and in pairs; duplicate flags; unknown flags; `u`+`v`; malformed setoptions literals; valid/invalid patterns and exact error class/phase.
2. UTF-16 input containing BMP text, astral pairs, isolated high/low surrogates, and starts at every code-unit boundary. Check `u/v` versus legacy decoding, empty matches, captures, named groups, match ranges, and repeated global/sticky execution including `lastIndex` reset on failure.
3. Variable lookbehind with captures/backreferences, alternatives with capture participation, named and numbered references, zero-width loops, and the relevant ES legacy-vs-unicode escape rules.
4. `v` nested set algebra, string-valued properties such as `RGI_Emoji`, complement/negation restrictions, and code points added/changed between Unicode 16 and 17. Include case-insensitive matching across ASCII, Kelvin sign, long s, dotted/dotless I, supplementary letters, and backreference folding.
5. Budget exhaustion and cancellation for both UTF-8 and UTF-16 APIs, high-complexity inputs, and explicit ownership/release tests for compiled regexes and result buffers if a C ABI is added.

## Pinned source identity

`regress` commit: `771e6c8fa1e472ef86c45b5206b37fa4e4494e25`; crate version: 0.12.0. `MathJax-src` commit: `fb987178c2d279a99b0db5ea02e751691703955e`.

SHA-256 evidence (paths relative to the repository root):

| File | SHA-256 |
|---|---|
| `.reference/regress/README.md` | `b996bfcf4d92b4101ec3360b922446f9e29b5d1065aa698dc1994424470cf95b` |
| `.reference/regress/Cargo.toml` | `c78ec2e4f0ca4f84c005db377926c7d38d5d9a35cf5267906f897b23d1dc3e5c` |
| `.reference/regress/src/api.rs` | `b748e71e56e40c46108b86842dc0face872ff9b0d4f36f5612aa0f61731c424a` |
| `.reference/regress/src/parse.rs` | `591243258a598d58e9ed9c4998bff3d75004ceb5dc15bb40e2f35b9c2e813833` |
| `.reference/regress/src/indexing.rs` | `8df75d23395147e16a06112aba619d8305eba6a0455046c2fe3291ed303ab5f4` |
| `.reference/regress/src/exec.rs` | `2150e785f0067de90451b0c24595ee8773c85f24f04e018855ad1784338aadbc` |
| `.reference/regress/src/types.rs` | `2649c903fbf003731ebc6377163e2134d6c61405264359b6df0a90c4300a76a4` |
| `.reference/regress/src/matchers.rs` | `575b5c0cbbfbe97c528eaac64bfc768204e8565c5370ee5055f1f0a39465931b` |
| `.reference/regress/src/unicodetables.rs` | `e2f63d08fbe2f0b523986ad29dd87e5e9c24a3e4389632f043b2e1185d859849` |
| `.reference/regress/gen-unicode/src/main.rs` | `f420ac195dc831b3aadf9ba65cbcac5047d6974b8f07c1cdd617ceb7fff20bd0` |
| `.reference/regress/tests/unicodesets.rs` | `15ce00eb27e1d2db2f41d54e378d68e5adbaa88f152e43caea77e61a0e4b709b` |
| `.reference/regress/tests/tests.rs` | `ea80b4caa29221726f4a3c95fd612a39a141ef5800084704fd702a1feafa8b26` |
| `.reference/MathJax-src/ts/input/tex/setoptions/SetOptionsConfiguration.ts` | `ccaa243137c68208e7f442013aa5dc6cc04b735ac3e0ca099ea8f30be557790d` |
| `.reference/MathJax-src/ts/input/tex/TokenMap.ts` | `0209895b13569008b2129b61c88050e7c70e70ce9477d234b9a6e303a86a738e` |
| `.reference/MathJax-src/ts/input/tex/ParseMethods.ts` | `1cdc4c6828078cd40863269864cdd6b7e5d22024e2ed36f4eeafb673781a5a70` |
| `.reference/MathJax-src/ts/input/tex/base/BaseConfiguration.ts` | `578d2c6b03f2f3e902476a0b73a5323b12262215fcbdc831a1135c1d9c1737ad` |
