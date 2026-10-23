# Editing patches and shared history

## Current checkpoint

Implementation complete in working tree, 2026-10-23. No agents or live builds.
Foundation committed c799ee62 (24 files,431 additions/114 removals). Shared source/
rendered history integration and its tests are next to commit. Do NOT rerun ignored
build/undo-stage-foundation.py: its HEAD-based staging job is finished.

Verified final implementation:
- Core752 tests /499404 assertions: build/undo-core-verified.log.
- Windows editor80 tests /3429 assertions: build/undo-platform-verified.log.
- CommonMark652/652 strict + lossless/save: build/undo-commonmark/results.json.
- GFM1426 exact save,1395 strict,20 unscored,11 verified reference differences,
  0 unresolved: build/undo-gfm/results.json.
- setup.ps1 and WinUI Debug compile/link succeeded: build/undo-setup.log and
  build/undo-winui-build4.log; build/app-winui/bin/x64/Debug/Folia.exe.

One Editor history now owns text/tree/document-property patches across modes;
SourceEditor is a derived source view with no undo stack. Mode exit never rebuilds
Editor. Ordinary local edits/replay do zero full parse/serialize/tree diff; changed
reference definitions may expand their genuine document-wide grammar dependency.
Untouched roots retain IDs even across structural batch edits.13 new history test
groups cover exact source/CST IDs/payload/render/selection,300 seeded source steps,
Unicode/endings, references, frontmatter, cross-mode search/format and redo, failed
patch atomicity and capacity. Fixed existing callout/frontmatter physical-boundary
save defects and old WinUI XML/missing parser-module registrations exposed by
these checks. Hand-built text patches also roll back CST identities exactly.

Next: commit shared-view integration, incremental WinUI check, record resulting
hashes/clean status. No MSI/version bump/push; no interactive UI session exercised.
Previous Markdown task complete (markdown-conformance.md).
## Responsibilities

- DocumentTransaction: canonical reversible block-local text, tree and document
  property patches. Text patches validate the removed text; edited CST identities
  must be restored by history, not newly assigned on every undo.
- DocumentHistory: one timeline for both modes, atomic replay, selection before/
  after in the originating view, redo invalidation and bounded retention. No
  complete-document snapshots or full-document parsing during replay.
- Source view: derived text/line styling and a view selection. Commands produce
  source patches; they do not own a second application history or editing model.
- Source patch adapter: map a source-view edit to local source/tree operations.
  Reparse only the affected block region, expanding when Markdown context really
  changes the following region. Keep untouched block identities and source exact.
- Editor: owns document and shared history. Mode switches only project selection
  and presentation; they do not recreate the Editor or create history entries.
- WinUI EditorSession: route both command/search/IME paths through those core APIs.

Source-view offsets are presentation-boundary data, not another coordinate inside
an editable block. Rendered editing continues to use container id + source offset.

## Acceptance

- Rendered edit -> source mode -> undo/redo, source edit -> rendered mode ->
  undo/redo, alternating edits/modes and redo invalidation use one history.
- Undo restores exact source, block payloads/tree/ids, CST identities/partition,
  selection and rendered content; redo restores the edited state. Include history
  capacity, invalid patch atomicity and empty/no-op edits.
- Cover incomplete syntax, marker variants, escapes/entities, links/images/math,
  CR/LF/CRLF, Unicode/graphemes, nested containers, tables, structural boundaries,
  multiline edits, search replacement and random edit/undo/redo sequences.
- Normal local edits/replay do zero full-document parses/serializations/tree
  diffs. Unaffected blocks keep their identities. Source formatting and genuine
  document-wide edits may intentionally touch the whole requested region.
- Core and deterministic Windows tests pass; compile the WinUI integration with
  parallel4. Keep generated files ignored and commit reviewable responsibility
  groups. Do not bump the package version or publish an MSI for this request.
