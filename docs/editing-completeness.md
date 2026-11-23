# Editing completeness audit

This audit covers the portable command path in rendered and source views, together with the WinUI input boundaries that dispatch those commands. The block tree remains authoritative; editable owners retain their exact local source and lossless CST; both views use the same reversible transaction history.

## Current audit status — 2026-11-24 (Asia/Shanghai)

The current branch is `fix/editor-operation-completeness`, starting from `338b8f0`. The focused gates recorded below have passed using the production C++23 modules on Linux. They include source splice oracles, rendered boundary operations, input policy, link/image/footnote insertion, source fidelity, and exact undo/redo.

**Final integration status: passed.** The unfiltered `FoliaCoreTests` runner passed **1,093 tests / 1,305,833 assertions**, with exit status **0**, **0 failures**, and **0 skipped tests**, against code/test commit `e9ea0a55cc4bf941dddd1ecb101ab33e5fe4a757`. The final documentation commit records that result without changing the tested code or tests.

The final focused batch also repaired invisible-only documents and HTML paragraph splitting. A zero-source editable destination preserves the original file until the first edit; source replacement and deletion retain a valid destination through history. HTML Enter now creates persistent paragraph boundaries and balances inline HTML envelopes across the split. These cases passed their focused gates before the final integration run.

The WinUI application and Windows native integration tests have **not** been executed in this Linux environment. Portable tests exercise actual production modules, including the platform-independent shortcut adapters; they do not substitute stubs for Windows services or claim to validate native event delivery. Development gates run locally. The audit does not change GitHub workflow definitions, and its pushed commits use `[skip ci]`.

## Interaction contract

### Successful edits and meaningful boundaries

A command that can edit selected content must produce the requested source/tree change, or a meaningful selection change that exposes the next editable position. An actual document edge, an outdent with no indentation to remove, or an already-satisfied formatting request may have no content change. Such cases must not manufacture a revision or an undo entry. Invalid positions consisting only of hidden raw markers remain rejected without changing source, selection, or history.

Each user edit records one transaction containing the required local source edits and tree edits. Boundary paragraph creation, selection replacement, and the requested insertion belong to that transaction. Undo/redo restores exact source, structure, and both selection endpoints. Normal command paths perform no full-document parse, full-document serialization, or full-tree transaction diff. Structural reclassification uses bounded affected fragments and their local mappings.

### Navigation, deletion, and indentation

- Backspace/Delete at compatible text boundaries join neighboring text across list, quote, callout, and footnote ancestry while retaining unrelated siblings. At nonmergeable raw/table boundaries, the directional command moves to the neighboring editable edge without changing revision/history.
- Left/Right collapse an unextended selection in their direction. Home/End and document start/end move from the active endpoint to their actual destination. The portable Up/Down fallback crosses physical lines and neighboring owners and handles LF, CRLF, and CR. Native hosts may supply measured visual-line movement.
- Word movement/deletion uses shared Unicode scalar and grapheme boundaries. Text, whitespace, and punctuation runs have a deterministic locale-independent policy; underscores remain part of identifiers. This is not dictionary-based segmentation for scripts without spaces. Raw blocks navigate their visible body through the source map, excluding hidden fences and indentation syntax.
- Paragraph movement first reaches the current owner's edge, then the neighboring owner's corresponding edge. Word deletion at an owner boundary delegates to the ordinary directional deletion rule instead of deleting fence/table syntax.
- Tab/Shift+Tab belong to the innermost editing surface. Source view and a selection within one raw owner receive indentation/outdent directly; an active table cell receives table navigation even inside a list. Other contexts try list indentation and then the ordinary text fallback. The first table cell can move to the preceding editable owner; a leading table obtains an undoable preceding paragraph. Tab after the last cell appends a row. Range navigation uses the active cell, while row/column edits retain their unambiguous-target requirements.
- Text indentation edits physical source lines. It can reclassify paragraphs/headings and indented code locally, including ATX and Setext headings, with exact history. Raw code/math indentation remains inside the body. A range ending at the following owner's start does not indent that unselected owner.

### Selections, objects, and insertion commands

- Atomic objects expose before/after positions. Enter, typing, paste, and inline insertion obtain a real paragraph in the same flow parent. Selecting an object and replacing it retains the intended flow position. Image labels use actual source/alt content instead of the object replacement character.
- Mixed selections across tables and raw/atomic owners edit each selected visible slice while preserving the table's rectangular structure. Fully selected non-table objects/raw owners may be removed. Empty visible cell/boundary selections can collapse, allowing replacement, Enter, or paste to continue. A range wholly between hidden raw markers is not a rendered selection.
- Link insertion across owners links each selected local label in one transaction and preserves block structure. Existing links are retargeted through their parsed destination/title ranges. Unchanged semantic fields retain their exact original escaping, entities, attributes, and quote spelling. A selection crossing part of an existing link expands to its complete label; embedded link wrappers are removed before constructing a new outer link, preserving label formatting and images.
- Generated link syntax escapes semantic destinations/titles without decoding the user's literal entity-looking text. Opaque code spans and HTML attributes inside a label retain their original source. Table pipes and physical line endings in generated fields cannot silently change the table shape.
- Image insertion replaces the ordered selection in one transaction, including selections crossing owner kinds. Default alt text excludes raw fences and object placeholders; physical line endings become spaces. Raw code/math owners receive literal generated syntax within their visible body.
- Footnote insertion preserves selected text and appends the reference at the ordered selected end. A terminal owner at offset zero contributes no selected text. The command creates an empty definition and focuses it in the same transaction. A new standalone definition is placed before a trailing unclosed raw/HTML block that would otherwise absorb it on reload.
- An explicit after-boundary insertion at unterminated HTML retains that direction: the missing grammar terminator is completed before creating the following paragraph, and only that local block is reclassified. Existing raw bytes remain intact and undo restores the incomplete source. Blank-line-terminated HTML and complete forms do not acquire unnecessary closers.
- A document containing only invisible HTML/comments has an ordinary zero-source paragraph as its editing destination. Opening or changing views adds no source bytes or history. The first rendered edit materializes the required saved boundary in that same transaction; deleting or replacing the final visible owner also preserves an editable destination. Existing document line endings are counted once.
- Enter inside a semantic HTML container creates explicit paragraph elements, including empty paragraphs that would otherwise disappear on save. Complete inline HTML elements crossing the split close on the left and reopen on the right with their exact original attributes and delimiters. The right caret lands inside those reopened elements; quote/list physical-line anchors account for the inserted prefixes. Visible raw text retains its original caret semantics.

### Source and host input boundaries

Source view applies local patches and explicit resulting selections. It does not leave source mode or maintain a separate undo history. Shared code-language editing uses the authoritative parsed code owner and projection, including nested list/quote fences, preserving other info-string fields and validating the marker/language combination.

Input intent belongs in portable core modules. The WinUI host supplies native gestures, clipboard/IME integration, focus, and measured visual geometry. Configurable shortcuts resolve in the active context, preserve Shift extension, and do not revive a disabled binding through a second modifier mapping. AltGr remains available for text entry. Command rejection is propagated so a legal fallback is not swallowed.

Text-service commits and later character events are correlated by exact ordered text fragments, revision, and a bounded monotonic lifetime. A character is suppressed only when it matches an outstanding commit fragment; a mismatch, revision change, expiry, or consumption clears that expectation. This avoids losing unrelated repeated characters while preventing duplicate delivery.

Shift-click and drag extension preserve the original selection anchor. Word selection uses the same grapheme/run policy as keyboard movement and maps raw bodies back to visible source boundaries. Empty line/block clicks have an editable fallback. Native viewport paging retains a portable logical-page fallback when measured movement cannot provide a target.

## Confirmed defects repaired in this round

| Area | Failure | Repair and regression coverage |
| --- | --- | --- |
| Flow deletion | Backspace/Delete stopped at different container ancestry even when text could join | Neighbor-owner joins with local recorded tree/source edits; nested lists, quotes, callouts, footnotes, both directions |
| Raw/table boundaries | A nonmergeable neighbor swallowed a directional action | Selection-only movement with no revision/undo mutation |
| Physical navigation | Up/Down stopped at the first/last line of an owner; Home/End only collapsed a range | Active-endpoint destination policy and cross-owner fallback across LF/CRLF/CR |
| Table navigation | Shift+Tab in the first cell had no destination | Previous-owner navigation or reversible paragraph creation before a leading table |
| Mixed selection | Tables caused valid raw/atomic deletion or replacement to reject | Visible source slices, removable full objects, surviving selection target chosen before tree mutation |
| Hidden raw markers | Accepting empty structural ranges also accepted hidden-only marker gaps | Explicit protection of invalid hidden-only ranges, retaining the visible-boundary behavior |
| Code language | Source mode rejected nested fences or damaged other info-string content | Shared grammar-aware local edit plan through the parsed owner and projection |
| Source patches | A local inline patch could leave stale separators; empty list items lost the following physical boundary | Correct local eligibility and separate body/emitted-marker completion facts |
| Raw source provenance | Blank lines or a final prefix-only line in unclosed nested raw blocks were lost | Retain accepted physical endings and publish the existing displaced EOF source anchor |
| Mixed line endings | Separate CR and LF lines could lose a quote/container prefix | Preserve physical-line prefix ownership instead of treating unrelated characters as one CRLF |
| Word/page/Tab commands | Recognized shortcuts lacked an effective portable command or context fallback | Shared navigation/indentation/input modules, source equivalents, and result-aware host dispatch |
| Initial indentation | Typing after initial indentation failed to recognize the resulting code block | Local block reclassification after typing, with exact source/history tests |
| Text input | Later character events could be suppressed by stale or overly broad text-service echo state | Portable ordered prefix matcher with revision/time invalidation and host integration |
| Pointer selection | Extension replaced the selection anchor; raw word selection could include hidden syntax | Shared anchor-preserving selection policy and visible-body word mapping |
| Inline insertion | Raw owners and cross-owner image selections rejected enabled commands | Literal raw insertion and transactional selection replacement; fence/body boundaries remain intact |
| Link insertion | Multi-owner ranges rejected; nested old links produced malformed new links; generated fields changed semantics | Per-owner operations, local retargeting/label flattening, opaque-span preservation, semantic escaping, and CST/reload assertions |
| Footnotes | Cross-owner/raw/atomic endpoints rejected; an appended definition could disappear into an unclosed block | Ordered-end insertion, shared safe definition placement, definition focus, exact history and reload |
| Incomplete HTML | Inserting after an absorbing HTML object produced content that disappeared into it on reload | Complete its missing terminator in the same transaction and reclassify only the affected owner |
| Invisible-only documents | Complete comments left no editable destination; removing the last visible owner could reject or leave an unusable selection | Zero-source root paragraph, reversible first-edit boundary materialization, and source/rendered deletion history with exact projections |
| HTML paragraph splitting | Empty Enter vanished on save; splitting formatted HTML could produce crossing tags | Explicit paragraph provenance, CST-based inline closure/reopening, right-caret placement, and adjusted container-prefix anchors |

Earlier work on this branch's inherited editing foundation also covers Unicode grapheme deletion, list range indent/outdent and style changes, block insertion over selections, clipboard whitespace/objects, inline format toggles across owners, HTML format aliases, safe code-span delimiters, and source formatting menus. Those earlier results are not counted as new findings in this round.

## Portable module responsibilities

| Module | Responsibility |
| --- | --- |
| `folia.core.input_command` | Input intent, default portable key commands, modifier boundaries, contextual Tab fallback, pointer anchor policy |
| `folia.core.input_text_echo` | Text-service/character echo correlation using host-supplied text, revision, and time |
| `folia.core.text_navigation` | Grapheme-safe word boundaries and pointer word/run selection over text |
| `folia.core.document_text_navigation` | Word/paragraph movement and deletion over owner order; raw visible-body mapping |
| `folia.core.document_indentation` | Physical-line text indentation/outdent and bounded structural reclassification |
| `folia.core.document_editable_fallback` | Zero-source editable destination for invisible-only documents; first-edit boundary materialization and shared history |
| `folia.core.document_html_split` | CST-based split planning that closes and reopens inline HTML envelopes with exact attributes and a valid right-hand caret |
| Existing document editing modules | Source/tree transactions, boundary navigation and creation, table/mixed selections, links/images, footnotes |
| Source editor/command modules | Equivalent source commands, local patch mapping, explicit source selections, shared history |
| Markdown parser/serializer modules | Grammar-aware field/opaque-span queries, physical prefix/completion provenance, safe newly generated syntax |
| Windows/WinUI adapters | Native event delivery, focus and configurable bindings, IME/clipboard, measured visual lines/pages; forwarding portable policy outcomes |

`document_edit` remains a facade. The extracted core modules contain no Windows, WinRT, Direct2D, DirectWrite, DXGI, or TSF dependencies. The standalone portable gate also compiles the production Windows-key/shortcut adapters that themselves use no Windows APIs; the native services they coordinate are outside that gate.

## Verification design and observed focused results

Tests check actual command acceptance and resulting content, rather than only checking a boolean return. They cover forward/reversed selections, empty and nonempty ranges, both object boundaries, nested containers, raw/atomic/table combinations, physical line endings, non-BMP/combining text, generated syntax, and mode transitions. Assertions include exact save/reload where the command promises source preservation, CST token flattening, valid selection offsets, exact cached indexes, symbols, rejection atomicity, one-step undo/redo, and the operation counters guarding bounded work.

The new source acceptance oracle exercises 49 document shapes at every tested source offset and at forward/reversed range endpoints. Insertions, Backspace/Delete, empty replacement, line breaks, and multiline Unicode replacements are compared with direct source splicing, then checked through shared history, view switching, and reload. Separate parser-completion tests compare recorded completion with actual fragment serialization; this caught the distinction between an empty item's body and its list-owned marker.

The following are observed focused passes from this round. Earlier batches were checked at their corresponding commits; **these overlapping rows must not be summed into a full-suite total.** The final unfiltered gate independently passes all of these regressions together with the inherited suite.

| Focused gate | Observed result |
| --- | ---: |
| Rendered flow boundary operations | 2 tests / 531 assertions, exit 0 |
| Rendered physical navigation | 4 tests / 949 assertions, exit 0 |
| Table navigation | 3 tests / 110 assertions, exit 0 |
| Mixed table/raw/atomic selections | 4 tests / 1,202 assertions, exit 0 |
| Raw/atomic inline and image insertion | 5 tests / 1,820 assertions, exit 0 |
| Link insertion/retargeting/source preservation | 11 tests / 1,698 assertions, exit 0 |
| Rendered footnote insertion contexts | 7 tests / 2,150 assertions, exit 0 |
| Existing raw history/hidden-marker regressions | 5 tests / 538 assertions, exit 0 |
| Source operation acceptance oracle | 2 tests / 592,920 assertions, exit 0 |
| Terminal raw prefix preservation | 2 tests / 1,152 assertions, exit 0 |
| Source footnote contexts | 2 tests / 9,756 assertions, exit 0 |
| Source word/paragraph commands | 6 tests / 156 assertions, exit 0 |
| Nested code-language commands | 4 tests / 315 assertions, exit 0 |
| Parser completion/source oracle | 1 test / 143 assertions, exit 0 |
| Empty list-item opening boundaries | 1 test / 360 assertions, exit 0 |
| Unclosed raw/list physical endings | 2 tests / 225 assertions, exit 0 |
| Invisible/empty HTML rendered editing | 6 tests / 1,070 assertions, exit 0 |
| Source editable destinations and untouched suffixes | 4 tests / 622 assertions, exit 0 |
| HTML inline split and source/history preservation | 4 tests / 2,740 assertions, exit 0 |
| Combined final link/insertion/history/source-command focused run | 53 tests / 8,564 assertions, exit 0 |

Portable input policy, text echo, shortcut scope, pointer selection, and word/page/indent tests also passed their focused runs and the final unfiltered gate. The inherited Unicode 15.1 foundation retains the pinned 1,187-case official grapheme corpus and generated source lock; this audit reuses that boundary policy rather than introducing a second coordinate system.

### Current full portable gate

| Final gate | Result |
| --- | --- |
| Production-module build | Exit 0 |
| Complete unfiltered portable runner | **1,093 tests / 1,305,833 assertions, exit 0** |
| Failed / skipped tests | **0 / 0** |
| Code and test commit | `e9ea0a55cc4bf941dddd1ecb101ab33e5fe4a757` |
| WinUI project registration | Both project XML files parse; all seven new core modules occur exactly once |
| Whitespace / patch validation | `git diff --check` passes |
| GitHub Actions on this branch | 0 workflow runs; workflow definitions unchanged |

The final run took **15.15 seconds** in this workspace. This is an observed portable test-run duration, not a Windows UI latency measurement. During integration, the comments-only destination invalidated one footnote fixture's assumption that the last root was its original literal block. The fixture now tracks the original first block, additionally verifies that its source remains the saved prefix, and retains its placement/history assertions. The complete runner was rebuilt and rerun after that correction; no assertion or suite was disabled.

The standalone project is `tests/libraries/core`. This round uses CMake 4.3.4, Clang 21, libc++ 21, C++23 named modules, `import std;`, and the repository's forked Boost.UT module to compile the actual production modules. Focused executables reuse those production objects and select test translation units; they are not header-flattened simulations or mock editors.

Follow the [portable core toolchain instructions](../tests/libraries/core/README.md) to configure the standalone directory. The complete gate used for this audit is:

```sh
cmake --build build/core-portable-llvm21 --target FoliaCoreTests --parallel 6
build/core-portable-llvm21/FoliaCoreTests
```

A positional filter selects individual suites, for example:

```sh
build/core-portable-llvm21/FoliaCoreTests '*link_insert*'
```

The standalone gate deliberately avoids the root WinUI/native rendering build and its unrelated font/resource generation. Generated conformance/pathological executables remain separate gates unless their execution is explicitly recorded.

### Native validation boundary

No Windows application build or Windows-native editor run is claimed here. Linux checks prove portable model/command behavior and the pure adapter policies; they do not execute TSF/IME delivery, keyboard layout/AltGr delivery, WinUI focus routing, DirectWrite hit-testing, measured wrapped-line/page motion, bitmap clipboard acquisition/storage, drag/drop, dialogs, or menu click delivery.

Native follow-up should exercise both views with Ctrl/Shift movement and deletion, disabled/custom shortcuts and editor focus changes, repeated/fragmented text-service commits, surrogate/combining input, Shift-click/drag and double-click in raw blocks, visual paging, first/last table-cell Tab, image paste followed by Enter/typing, and undo/redo across these operations. Use LF, CRLF, and CR documents. The shared semantic tests provide expected outcomes for those native checks; they do not assert that native delivery has already been observed.

## Historical validation — previous audit only

The previous report recorded a complete Linux portable run of **932 tests / 629,906 assertions, exit 0**, after integration with upstream `master` at `aa884b619b15a0dab361e4f01ec34c0442afa98b`. This is retained for traceability only. The current final full gate must independently revalidate the inherited suite together with this round's changes.
