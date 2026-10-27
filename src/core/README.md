# Core layout

`src/core` is the platform-independent editor and rendering model. It consumes
the standalone [Markdown document library](../markdown/README.md) through
`folia::markdown`; it does not own a second parser or document representation.
The dependency direction is `core -> markdown`, never the reverse.

- `support/`: geometry, errors, settings, storage, scheduling, image metadata and export descriptions.
- `model/`: editor extension configuration; document/CST ownership lives in `src/markdown/model`.
- `editing/`: block-local source edits, block-tree edits, input rules, history, selection, semantic clipboard splicing, snippets, commands, and transactions.
- `query/`: editor content/context projections, rendered/source search, SRELL regular expressions, and LaTeX catalog/completion queries. Parse-time symbols and outline live in `src/markdown/query`.
- `rendering/`: platform-neutral render model, layout plan/tree, hit testing, selection geometry, HTML presentation, and math/render descriptions.
- `source/`: source-mode editing, line projection, and syntax-style projection.

Editing modules should expose narrow operations and keep dependency direction explicit. Block input is split into marker recognition, list transformations, non-list block transformations, and a small public coordinator. Document copy and paste are separate because selection slicing and semantic tree splicing have different invariants. Shared edit support is an aggregate over source-edit primitives, reversible block mutations, and invariant validation rather than one catch-all implementation file.

`document_edit.ixx` is a thin facade over feature-specific modules. Inline input,
formatting, snippets, and search replacement produce block-local `TextEdit`s;
Enter, cross-block deletion, list/quote/callout/footnote behavior, and table
operations produce block-tree transactions. Neither path may serialize and
reparse the full document to restore selection.

Inline formatting is a CST query followed by one block-local source edit. Existing formatting is identified from complete CST delimiter ranges, so removing `_`/`__`, variable-length code spans, math, and nested formatting preserves the exact source spelling instead of comparing against hard-coded marker text.

Whole-document source formatting is a separate explicit command implemented by
`editing/document_format.ixx`; it is not part of ordinary serialization or
Save. The formatter traverses the same unified block tree, canonicalizes only
block-level presentation metadata, records reversible payload operations, and
keeps every inline source, incomplete CST, raw code/math block, and HTML syntax
island exact. Source mode uses the same formatter only when commanded, maps its
flat selection through serializer projections, and records the replacement as
one source-editor history transaction. The canonical result must be
idempotent.

Callout titles are ordinary `CalloutTitle` block nodes: when present they are the first child of the `Callout`, own their own `InlineDocument`, source coordinate, and node identity, and participate in the same recursive traversal, clipboard, history, rendering, and validation paths as every other editable child. The Callout container owns only structural metadata; it has no editable side document.

Some large files are intentionally cohesive:

- `../markdown/parsing/parser.ixx` owns one block-parser state machine and its source-range accounting.
- `../markdown/parsing/inline_parser.ixx` owns the lossless inline CST state machine.
- `rendering/render_builder.ixx` owns one recursive block-tree-to-render-model traversal.

HTML remains a lossless syntax island in the document model. The rendering
layer derives only allow-listed presentation: `html_inline_presentation.ixx`
projects safe inline styles, while `html_inline_flow.ixx` applies HTML
formatting-context whitespace rules without changing source text or source
spans. Block-level alignment is carried explicitly through `BlockStyle`; it is
not inferred from rendered coordinates.

Rendered search uses the visible-text projection and maps matches back to
block-local source ranges, so Markdown markers and paired HTML tags are not
searchable decoration. Source-mode search operates on the exact Markdown
buffer. Both modes use the same SRELL-backed query engine and commit
replacements through the appropriate source/tree editing boundary.

Extract helpers from these only when they have an independent invariant and one-way dependency. Do not divide a parser state machine or tree traversal into arbitrary line ranges.

Run the portable suite with `cmd /c build_test.bat`; test ownership and
filtering are documented in [tests/README.md](../../tests/README.md).
