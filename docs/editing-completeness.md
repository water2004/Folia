# Editing completeness audit

This change audits the portable editing command path in both rendered and source views. It follows the bounded document/layout architecture and keeps the block tree authoritative, inline source and CST lossless, and transaction history shared between views.

## Interaction contract

- An atomic rendered object has a position before and after it. Enter, typing, and inline insertion at either boundary create a real editable paragraph in the same flow parent. Creating that destination and inserting content form one history transaction.
- Backspace and Delete respect their direction. At the outward-facing edge of an object they move to the neighboring editable position without a document revision; deleting a selected object removes it. Cross-owner selections may include objects and raw fenced blocks.
- Markdown input rules use physical source lines. A thematic break must not be synthesized by concatenating an empty list marker line with a later line. Setext recognition retains source spelling and physical line endings. HTML text islands retain their existing literal-text policy.
- Rendered table cells accept literal pipes and line breaks without changing the table shape on save/reload. Source mode remains a direct Markdown editor. Clipboard content retains whitespace, empty selected owners, and terminal line breaks.
- Inline formatting spans all selected eligible inline owners in one transaction. Mixed selections receive the missing format; an entirely formatted selection removes it. HTML formatting is recognized through the same semantic aliases as rendering, preserving original attributes on partial removal.
- Structural commands operate on the selected range when meaningful. List style changes preserve owners and content; moving an item preserves reading order, descendants, and valid marker/continuation indentation. Block insertion replaces the selection in the same transaction.
- Source formatting commands emit local source patches. They do not leave source mode, create an independent history, or parse/serialize the entire document. Replacing text with the same text can still update the selection without creating a history entry.
- Both views use Unicode scalar offsets with Unicode 15.1 extended grapheme boundaries for caret movement and deletion. Source is never normalized.

## Confirmed gaps and fixes

| Area | Failure | Correction |
| --- | --- | --- |
| Atomic owners | Pasted image, thematic break, TOC, and other objects could trap Enter or typing | Shared reversible boundary paragraph creation |
| Atomic selection | Wrong-side deletion removed the object; object-only selection could not be replaced | Directional navigation and explicit selected-object deletion |
| Input syntax | `---`, spaced thematic markers, and Setext headings failed to activate on Enter | Physical-line-aware recognition using existing grammar and bounded source fragments |
| Lists | Delete did not join adjacent items; outdent reordered following content | Recorded item moves and continuation reparenting |
| List styles | Changing ordered/unordered/task style unwrapped the list | In-place style conversion retaining leaf IDs |
| List selection | Indent/outdent handled one item instead of the selected range | Range-aware moves with endpoint exclusion |
| Block insertion | Code, math, table, and TOC commands rejected nonempty selections | Transactional selection replacement and safe block placement |
| Clipboard | Copy returned an object replacement character; blank selection boundaries disappeared | Serialize actual selected objects and preserve separator-only ranges |
| Clipboard | Table paste trimmed spaces; paragraph/heading paste dropped terminal newline | Preserve original clipboard text and use shared cell source conversion |
| Tables | Typed/pasted pipe changed columns after reload; deleting a displayed pipe left a stray escape | Contextual pipe escaping and semantic escape deletion |
| Links and images | Generated destinations/labels broke Markdown or HTML syntax | Shared syntax-aware source generation, reachable image caret |
| Inline formatting | Multi-owner selections were rejected; mixed formatting could not be applied consistently | Read-only owner plans, one global apply/remove decision, and one reversible transaction |
| HTML formatting | Repeating a format command nested tags instead of removing the format | Recognize semantic HTML aliases while retaining exact delimiter and attribute source |
| Inline code | Selected backticks collided with generated delimiter; toggle-off retained padding | Safe delimiter sizing and syntactic-padding-aware removal |
| Headings and wrappers | Multiline/cross-owner headings were rejected, wrapper removal could merge sibling paragraphs, and level changes could lose anchors | Physical-line heading leaves, incoming paragraph boundaries, and retained existing slugs |
| Callout source | Empty body prefixes/maps or first-body line endings were lost, causing source commands to reject otherwise valid edits | Exact parser header boundaries and explicit prefix/empty-owner serialization |
| Source view | Formatting menus were disabled or commands had no implementation | Portable source command adapter and enabled host menus |
| Source view | CRLF boundary patches, vertical movement, and selection-only replacements behaved incorrectly | Explicit replacement selections, physical line endings, and preferred-column movement |
| Unicode | Navigation/deletion split clusters or joined unrelated characters | Pinned UAX #29 data, official corpus, and editor history tests |

## Verification design

Focused regressions exercise forward and reversed selections, both object boundaries, nested containers, mixed physical line endings, save/reload, exact undo/redo, and source/rendered view switching. The sequence test combines 16 initial document shapes, two initial views, and 48 commands per run (1,536 commands total). After each command it checks block invariants, exact indexes, lossless CST source, valid selection offsets, and rejection atomicity. It also checks that ordinary commands perform no full-document parse, full-document serialization, or full-tree transaction diff.

The grapheme implementation is generated from pinned Unicode 15.1 files. `tools/markdown-conformance/grapheme-sources.lock.json` records upstream URLs and SHA-256 digests; `generate_grapheme.py` reproduces the checked-in data and official test fixture. The official 1,187-case corpus is exercised in both directions and against grapheme counting.

### Native boundary

The semantic clipboard path is tested with the same Markdown payload that the WinUI clipboard-image store generates. This Linux validation does not execute Windows bitmap acquisition/storage, IME, DirectWrite hit-testing, dialogs, or menu click delivery. Relative to the merged upstream `master`, host changes are limited to module registration and enabling source-mode formatting menus; no GitHub Actions configuration is changed by this work.

For native review, paste an image, press Enter, type, and undo/redo; type each thematic-break spelling and press Enter; exercise nested list range indent/outdent; paste multiline text and pipes in a table; then repeat formatting/history and navigation checks in source view. Check physical CRLF files and non-BMP/combining text as well.

## Recorded validation

Validated on Linux with Clang 21, libc++ 21, C++23 named modules, `import std;`, and the repository's forked Boost.UT module. This uses the actual production modules through `tests/libraries/core`; no header-flattened substitute or Windows API stubs were used.

The branch incorporates upstream `master` at `aa884b619b15a0dab361e4f01ec34c0442afa98b`, including its MSVC render-model change and viewport-anchor adjustment. There are no `.github` differences relative to that merged master.

| Gate | Result |
| --- | ---: |
| Complete portable core runner | 932 tests, 629,906 assertions, exit 0 |
| Multi-owner/HTML formatting regressions | 11 tests, 1,075 assertions, exit 0 |
| Inline code padding regressions | 4 tests, 1,325 assertions, exit 0 |
| Container editing regressions | 25 tests, 3,367 assertions, exit 0 |
| Source command integration and edge cases | 14 tests, 1,081 assertions, exit 0 |
| Source editing interactions | 10 tests, 11,713 assertions, exit 0 |
| Atomic interactions, including existing matching case | 12 tests, 1,873 assertions, exit 0 |
| Clipboard regressions and boundary cases | 12 tests, 367 assertions, exit 0 |
| Unicode grapheme foundation/editor groups | 4 tests, 20,932 assertions, exit 0 |

The complete runner was repeated after upstream integration and the final HTML formatting correction. Focused rows identify the corresponding regressions; the final complete run includes them. It took 11.45 seconds in this workspace; that duration is a test-run observation, not a Windows UI latency measurement.

Recorded build and run commands (the build directory was configured for the standalone core gate with Clang/libc++):

```sh
cmake --build build/core-portable-llvm21 --target FoliaCoreTests --parallel 4
build/core-portable-llvm21/FoliaCoreTests
```

Additional checks: `git diff --check`; both WinUI project XML files parse and contain the new source-command module exactly once; `git diff origin/master -- .github` is empty; upstream master is an ancestor of the feature branch. The 4,096-item indent/outdent and 2,000-item formatting tests assert zero full-document index scans and no full-document parse or serialization. The 4,096-item test additionally asserts zero root DFS node visits.

The separate generated conformance/pathological executables and Windows-specific application suites are outside this portable-runner result. The supplied large attachment was not read for this editing audit.
