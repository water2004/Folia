# Bounded native font initialization

The first focused NewCM native run exited with Windows stack overflow
0xC00000FD. A small unrelated font-parameter test passed. Dumpbin disassembly
identifies `makeNewcmFace` allocating 0x163918 bytes (1,456,408 bytes) in its
Debug prologue before any font rows are ingested. Generated repeated calls
create thousands of separate C++ temporary slots. This is native codegen
stack usage, not retained object ownership or recursive JS evaluation.

Do not increase executable/thread stack reserves to accept these generated
tables. Rewrite glyph ingestion as immutable aggregate POD rows and one native
loop per table, just as the accepted alias-array change removes repeated alias
call expressions. Keep the existing Source table ingestion order and overwrite
semantics. All default, 40 dynamic and extension glyph data use the same row
contract; no huge unrolled generated initializer function may remain.

Row fields have explicit glyph-domain types: variant id, codepoint, seven
numeric metrics, path-presence bit, composition codepoints/count, exact path
fragment and serialized path bytes, nullable content recipe. Constant strings
are static data. Any composition storage has a valid static lifetime; do not
perform pointer arithmetic on null for empty composition. The ingestion loop
constructs owned FontGlyph/SVG resource values, decodes once and preserves
typed diagnostics. No process-global mutable index, callable payload, object
registry or new runtime is introduced.

Luna writes a generator-only draft against current canonical bytes. Compare
every ordered row and the resulting last-write-wins table against the original
pinned inputs, including doubles, empty versus absent paths, composition,
nullable recipe, duplicate keys and extension cache ids. Main/default/dynamic/
extension geometry and backend catalogs must remain unchanged. Root rebuilds
with two math compiler jobs, checks focused profile initialization at the normal
console stack reserve, then resumes native dump comparison and broader tests.

## Verified implementation (2026-10-20)

The generator now emits constant POD glyph tables and bounded ingestion loops
for the default font, 40 dynamic modules and three extension fonts. SVG aliases
use the same table/loop form. Native construction, dump and tests pass with the
normal executable stack reserve; no stack-reserve increase was added.

The independent Source/native comparison passed 14,568,049 field comparisons
with zero mismatches (`build/math-port/font-semantic-keys-compare.log`). It
checks all visible glyph lookups, metrics, remaps, delimiter recipes, composition,
outline prefixes/diagnostics, exact SVG path bytes and semantic alias targets.
Measured native data includes 24,307 direct glyph records, 2,624 remap records,
24,305 outline resource objects and 238,666 SVG aliases. Shared cache IDs do
not merge distinct semantic variants such as `-bbm-bold` and `-bboldx`.

Reproduce after configuring and building `FoliaMathFontDump`:

```bat
cmd /c "build\core\FoliaMathFontDump.exe > build\math-port\native-raii-font-dump.json 2> build\math-port\native-raii-font-dump.stderr.log"
python tools/math-port/font-data-compare-native.py build/math-port/font-generated-native-raii/font-source-expected.json build/math-port/native-raii-font-dump.json
```

The full current native component suite also passed 102 tests / 30,600
assertions (`build/math-port/native-raii-font-resource-full.log`). These results
verify the font data and component contracts, not completed CommonWrapper
orchestration, TeX-to-SVG rendering or Folia integration. The pinned Source has
four malformed path fragments: SVG preserves their original bytes, while the
typed outline retains the valid prefix and an explicit diagnostic. No guessed
path correction is applied.
