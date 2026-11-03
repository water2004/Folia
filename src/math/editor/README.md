# TeX editing syntax

`folia::math_editor` is a small standalone CMake target exposing
`import folia.math.editor.syntax;`. It shares the native Tree-sitter runtime
with Markdown's HTML CST, and does not link the mathematical renderer or fonts.

`SyntaxSession` owns one incremental tree and its analysis. `Update` accepts
block-local TeX as UTF-32; result ranges use the same Unicode scalar offsets.
UTF-8 byte coordinates remain inside the Tree-sitter adapter. `MatchAt` reads
the current analysis without parsing again.

`PlanBracketEdit` returns source edits and a directed selection for typed
parentheses, square brackets and braces. It uses recovered delimiter pairs,
reuses existing closers, skips matched closing delimiters and excludes comments
and escaped commands. Scalable parentheses preserve `\left`/`\right`.
Deleting either matched bracket removes both delimiters while retaining their
contents. The host maps these TeX offsets back through the Markdown CST and
applies one reversible block-local source transaction. Literal paste/snippets
and active IME composition do not run the typing rules.

Interaction reference: VS Code's
[typing operations](https://github.com/microsoft/vscode/blob/main/src/vs/editor/common/cursor/cursorTypeEditOperations.ts)
and [delete operations](https://github.com/microsoft/vscode/blob/main/src/vs/editor/common/cursor/cursorDeleteOperations.ts).
Folia resolves pairs from its TeX syntax analysis and removes any matched bracket
pair on deletion, including existing source, as required by its math editor.

The editing grammar preserves incomplete input and classifies source tokens,
groups, sized delimiters and environments. It follows MathJax's control-word
and control-symbol boundaries, checked against `TexScanner` in tests. It does
not evaluate macros, decide whether a command renders, or replace the strict
TeX-to-MML parser. Markdown CST ranges decide where this grammar is applied.

- `syntax.ixx`: public source ranges, analysis and parser ownership.
- `syntax.cpp`: UTF-8 adapter, incremental parsing, token and bracket queries.
- `tree-sitter/grammar.js`: editable grammar; `tree-sitter.json`: ABI metadata.
- `tree-sitter/src/parser.c`: generated tables; never edit by hand.
- `tree-sitter/src/node-types.json`: generated syntax-tree schema.

On Windows, run `tools/math-editor-grammar/generate.ps1` to check reproducibility,
or add `-Update` after changing the grammar. Generation happens under `build/`;
only the two reviewed outputs are copied back. The pinned Node tool is needed
only for regeneration. Normal builds consume committed C and existing runtime
headers. `FoliaMathEditorTests` checks recovery, incremental equivalence and
Unicode; `FoliaMathTests "*editing command boundaries*"` compares MathJax's lexer.
