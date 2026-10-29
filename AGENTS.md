# Repository instructions

## Mermaid C++ 迁移指南（Agent 执行规范）

在参与 Mermaid C++23 原生渲染模块（`third_party/mermaid-cpp/`、`src/platform/text/`、`tools/mermaid-cpp/`）的开发或恢复上下文时，必须首先执行以下步骤：

1. **必读入口文档**：
   - 开始编码或规划前，先完整阅读 [`docs/mermaid-cpp/HANDOFF.md`](docs/mermaid-cpp/HANDOFF.md) 与 [`docs/mermaid-cpp/README.md`](docs/mermaid-cpp/README.md)。
   - `docs/mermaid-cpp/HANDOFF.md` 顶部记录了当前已验证的基准 commit、已接入公共入口的 20 个图族清单，以及待收尾任务（**Task A：ZenUML 公共入口**、**Task B：Class 默认 ELK 布局**、**Task C：ER 图原生实现**、**Task D：Requirement 图等补齐**、**Task E：WinUI 宿主切换**）的分步执行指令与允许修改的文件范围。
2. **模块与平台边界（硬约束）**：
   - **核心层（平台无关）**：`third_party/mermaid-cpp/src/` 下的所有模块（`.ixx` / `.cpp`）统一使用 C++23 modules 与 `import std;`，严禁包含任何 Windows、WinRT、Direct2D 或 DirectWrite 头文件。
   - **平台测量与桥接层（Windows）**：字体解析与文本测量位于 `src/platform/text/mermaid_measurer.ixx`，平台渲染桥接位于 `src/platform/text/mermaid_renderer.ixx`，DLL 会话封装位于 `src/platform/text/mermaid_host.ixx`。
   - **CMake 源文件收集规则**：`tools/mermaid-cpp/CMakeLists.txt` 对 `third_party/mermaid-cpp/src/*.{ixx,cpp}` 和 `tests/mermaid_cpp/*.cpp` 使用 `GLOB_RECURSE` 自动收集（放入即参与编译，切勿留存未完成文件）；而 `tests/platform/*.cpp` 则在 `FoliaMermaidPlatformTests` 目标中**显式逐个列出**（新增平台测试文件时必须同步写入该列表）。
3. **算法还原与文本测量纪律**：
   - 行为基准固定为官方 `mermaid-js/mermaid` **12.2.0**（commit `1fb7869866dd619168d840291c9c54b4f129758b`），新增上游依赖文件时需将源码 SHA-256 记入 `docs/mermaid-cpp/source-lock.json` 并核对 `third_party/mermaid-cpp/THIRD_PARTY_NOTICES.md`。
   - 凡涉及文本宽高的布局必须使用平台层传入的 DirectWrite 实测数据（注意调用 `MermaidMeasurer::measure` 前须先通过 `resolve_family` 将 CSS 字体族列表解析为单个已安装字体名）。严禁使用“字符数 × 固定系数”估算字宽、按测试样例硬编码坐标或放宽几何比较容差（SVG 实测几何容差固定为 `1.5`，ELK 算子坐标容差固定为 `1e-4`）。
4. **并行分工与交接更新**：
   - 共享接线文件（`render_entry.{ixx,cpp}`、`frontend.cpp`、`mermaid_renderer.ixx`、`tools/mermaid-cpp/CMakeLists.txt`、`.github/workflows/mermaid-cpp.yml`）在多人/多智能体并行时由集成负责人统一合入；独立图族负责人仅修改所属图族的源码、测试与 fixture。
   - 每完成一个任务包并通过对应 `FoliaMermaidTests` / `FoliaMermaidPlatformTests` 及浏览器 SVG 对照后，立即小批提交，并原位更新 `docs/mermaid-cpp/HANDOFF.md` 顶部的基线状态与任务进度（不追加历史流水账）。

## Math migration continuity

When working on the native MathJax migration, read `docs/math-port/HANDOFF.md`
at the start of a session and immediately after context compaction, before
planning, delegation, edits, or repeating an investigation. It records accepted
decisions, completed checks, known rejected approaches, and the exact next step.
Update that same file after a material decision, verification, or commit and
before handing off; do not create successive competing progress notebooks.
Use the record to resume work, then inspect only changes that could invalidate
it. Do not repeat completed broad audits or reinterpret settled user decisions
merely because context was compacted. This record is continuity information,
not permission to reset Git, overwrite unrelated work, or claim integration.
Actually read the file contents after compaction; a summary mentioning its path
does not satisfy this step. Before reopening a settled issue, identify the code
change or new evidence that invalidates the recorded conclusion. Keep a short
current-state and next-action checkpoint at the top of the same handoff file.
Before context compaction or ending a work session, save the exact stopping
point, live agent/build handles, candidate paths, verification limits, and next
concrete action there. On recovery, use the latest checkpoint rather than old
agent completion messages; search ignored candidate directories with
`rg --no-ignore` before declaring a delivered file missing.
Keep one authoritative current checkpoint at the top; label superseded status
and next-action sections as historical. An instruction to read the notebook is
not enough: update stale entries before handoff and actually read it on recovery.

## Build and test

- Use C++23 modules and `import std;`.
- Core tests: `cmd /c tests\run_core.bat 2>&1`.
- Deterministic Windows editor tests: `cmd /c tests\platform\run.bat 2>&1`.
- Core-only builds may target `folia_core` with CMake.
- Restore WinUI/NuGet and native SVG dependencies with `powershell -ExecutionPolicy Bypass -File .\setup.ps1` before building the WinUI app.
- Keep generated output in ignored build directories; do not commit build products or restored packages.
- Windows console tests/probes must link `folia_console_test_runtime`; manual builds must include `tests/support/windows_console_runtime.cpp`. CRT failures and `abort()` reports go to stderr without desktop dialogs. Stop a broader evaluation after an abnormal process exit and diagnose it with a bounded focused run.

## Code and platform boundaries

- Core modules under `src/core` are platform-independent and must not include Windows, WinRT, Direct2D, DirectWrite, DXGI, or TSF headers.
- Windows and rendering APIs belong in `src/platform` or the WinUI application layer.
- The application shell remains WinUI 3 with C++/WinRT; do not introduce a Win32 WndProc shell.
- Tests use the repository's forked Boost.UT C++23 module through `tests/support/folia_test.hpp`, which performs `import boost.ut;`. Test translation units deliberately do not `import std;`; they include the shared test header and then import the required `folia.core.*` or `folia.platform.*` modules. Do not replace this with the header-only `<boost/ut.hpp>` path. `tests/main.cpp` provides the core runner and `tests/platform/main.cpp` provides the Windows editor runner; both pass `argc`/`argv` to `boost::ut::cfg<>.run()`. Use a positional pattern such as `FoliaTests.exe "*name*"` to filter tests (`-t` lists tags).
- Prefer semantic fixes and focused tests. Do not add source-text special cases to make one example pass.

## Document model — block tree plus block-local lossless source

The authoritative editing state is a block-level structure tree. Each editable content node (Paragraph, Heading, TableCell, and the paragraph/heading bodies inside list items, task items, block quotes, callouts, and footnotes) owns the full, character-exact Markdown source of its inline content plus a lossless editable inline CST over that source.

- `InlineDocument.source` is the authoritative inline character content of its node: complete, unnormalized, including every marker, escape, entity, whitespace, and link-writing detail.
- `InlineDocument.tree` is a lossless editable CST over `source`. Concatenating its leaf token text in order must equal `source` exactly, character for character — not Markdown-semantically equal.
- The CST must preserve the original marker glyphs (`*` vs `_`, `**` vs `__`), escapes, entities, link/url spellings, quote forms, and whitespace, and must represent unclosed, ambiguous, and temporarily invalid editing states as error/incomplete nodes rather than collapsing them to plain text.
- Saving serializes the unchanged `source`; it must not re-derive or re-select markers from semantic fields for content the user did not edit.

## In-block coordinate system

Positions inside an editable content node are expressed as a container node id plus a source offset into that node's `InlineDocument.source`, with a text affinity. The offset counts every source character, including markers.

There is one in-block coordinate. Do not maintain parallel position schemes — logical text offsets, inline node ids, opening/content/closing marker-part offsets, or offsets into the serialized full document. `NodeId` may be returned by hit-testing or queries but is not an authoritative component of selection; CST nodes provide source ranges and structure only and do not define a second coordinate.

## Inline editing is source editing

All inline operations — character input, IME composition, Backspace, Delete, selection replacement, paste, drag-drop text, auto-pairing, bold, italic, strikethrough, inline code, links (insert/edit/remove), inline math — produce block-local source edits applied to the current node's `InlineDocument.source`. The flow is:

1. apply one or more `TextEdit`s to the current node's source;
2. update the selection's source offset;
3. re-parse the current node's inline CST only;
4. reconcile stable node ids where ranges are untouched or merely shifted;
5. update the affected layout and rendering.

Inline editing must not re-serialize the whole document, rebuild the full document AST, restore the cursor through a full-document source map, synchronize a legacy selection against the serialized document, or mutate semantic inline nodes directly (insert/erase into node text, split/merge inline vectors, reassign `children`). NodeId reconciliation is an optimization for keeping UI state on interactive links/code/math spans, not a precondition for cursor restoration.

## Block-level structure editing

Structure commands — split, join, insert, delete, set block type, indent/outdent list item, wrap/unwrap block quote, edit callout/footnote, edit table structure — modify the block tree. Enter inside a content node splits the node: cut the source at the current offset, create two nodes, parse each CST, move the caret to offset 0 of the second node. Cross-block Backspace/Delete joins nodes; it must not delete a newline in a serialized full-document string and re-parse. Keep block-text commands and block-structure commands as distinct layers; do not intermix them.

## Unified block tree

All structure nodes — document, block quote, list, list item, task list item, table, table row, table cell, callout, footnote, paragraph, heading, code block, math block, thematic break — share one tree accessed through uniform parent/children/insert/remove/replace/move/walk interfaces. The special side containers and their bespoke recursive entry points (`quote_children`, `list_items`, `task_items`, `table_header`, `table_rows`) are replaced by this single tree. Block-editing algorithms must not have separate hand-written recursions per node kind.

## Selection and history

Selection is container id plus source offset (and affinity) — one selection, no legacy/double-tracking. Undo/Redo records reversible transactions of `TextEdit`s and tree edits plus the selection before/after, and restores block state and selection exactly. It must not depend on re-parsing the full document. Complete snapshots may exist only as a debug/disaster fallback, never as the primary history mechanism for normal edits.

## Responsibilities

```text
Parser:       Markdown -> block tree + per-node InlineDocument (source + lossless CST)
Inline CST:   source -> token/CST (lossless, every character classified; one-node re-parse on edit)
Serializer:   block tree -> Markdown (inline = node.source, verbatim; markers never re-derived)
Editor:       TextEdit on node source -> re-parse one node -> reconcile ids -> render one node
Renderer:     node CST + selection -> visual representation; hit-test returns TextPosition
```

Markdown syntax recognition belongs to the inline CST parser, Markdown formatting belongs to the serializer, and keyboard interaction belongs to block-tree/source editing.

## Module boundaries and architectural discipline

Source editing, CST parsing, selection, and history remain separate, narrowly-scoped modules. `src/core/document_edit.ixx` is a thin facade and must not accumulate feature-specific editing logic. There is one source of truth: do not introduce an old/new dual track, a compatibility path, a feature flag selecting a prior model, or a fallback to legacy behavior. Reintroducing any of these is an architectural regression, not an incremental migration step.

## Invariants and testing

The CST must satisfy, continuously: `flatten_tokens(parse(source)) == source` and `serialize_lossless(parse(source)) == source`, both character-exact, preserving `*`/`_`, `**`/`__`, `[t](url)`/`[t](<url>)`, quote styles, escapes, and entities. Tests must exercise per-character input, deletion, selection, Enter, cross-block merge, format toggles, copy/paste, and undo/redo on representative inputs (`abc`, `*abc*`, `_abc_`, `**abc**`, `__abc__`, `**`, `**abc`, `a***b***c`, `~~abc~~`, `~~abc`, `` `abc` ``, `` `abc ``, `[title](url)`, `[title](<url>)`, `[title](url "name")`, `[title](`, `![alt](url)`, `$abc$`, `$abc`, `\*abc\*`, `&amp;`, `a\**b*`) across paragraph, heading, list/task item, quote, table cell, callout, footnote, and at document start/end/empty/adjacent-empty/nested contexts — checking source, CST flatten, block tree, selection, rendered text, save, reload, and undo/redo each time. Add random property tests asserting flatten == source, selection in range, save/reload losslessness, and exact undo/redo restoration. Normal-edit paths must perform zero full-document parses and zero full-document serializations; Unicode offsets must be one consistent unit across the core/render boundary, with explicit surrogate/combining/emoji conversion at the WinUI/DirectWrite edge.

## Agent workflow

Before changing editing interaction, understand the existing block tree, inline source/CST, document positions, transactions, normalization, selection, history, and serializer design. Treat the model in this document as the established architecture, not as an active migration. If implementation conflicts with these principles, call out the architectural regression and fix the responsible boundary when the task requires it. Do not add compatibility layers or revive legacy full-document/string-driven editing paths.

Keep changes scoped, preserve unrelated user work, and verify with the narrowest relevant tests plus a build when practical. Commit in small, reviewable batches — never one dump of hundreds of files. Do not use destructive Git operations unless the user explicitly authorizes them.
