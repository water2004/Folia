# Markdown document library

`folia::markdown` is a portable C++23 CMake target; `import folia.markdown;`
exposes the existing Folia document API. Public C++ types and functions remain
in namespace `folia` (for example `folia::EditorDocument` and `folia::parse_text`).
There is one implementation and one document model, shared with the editor.

The library owns Markdown parsing, the block tree, block-local source positions,
lossless inline CST, derived symbols/outline, and serialization. It does not
own editing commands, transactions, undo history, rendering, file dialogs,
network requests or mathematical layout. Those are consumers of this model.

```cpp
import std;
import folia.markdown;

auto parsed = folia::parse_text(1, "# Title\n\n**Exact** source\n");
auto saved = folia::serialize_markdown(parsed.document);
auto headings = parsed.outline.flat_items();
```

## Build as a dependency

Use a compiler/CMake combination supporting C++23 modules and `import std;`
(the repository uses CMake 4.3 and Ninja). Enable CMake's matching experimental
`CXX_IMPORT_STD` setting **before** the consuming project's `project()` call.
Then:

```cmake
add_subdirectory(path/to/Folia/src/markdown folia-markdown)
target_link_libraries(my_application PRIVATE folia::markdown)
set_target_properties(my_application PROPERTIES CXX_MODULE_STD ON)
```

This does not configure Folia's root project, invoke Node/font generators,
restore NuGet or link the math engine. The only bundled native dependency is
the tree-sitter HTML parser, used for lossless HTML syntax islands. Add the
library from a complete checkout so its `third_party/tree-sitter` sources are
available. An installed binary package is not provided by this change.

`tests/libraries/markdown` is a standalone consumer project and checks this
dependency boundary by building without `folia_core`. The full CommonMark,
GFM, CST and editor suites continue to exercise these same modules.

## Responsibilities

- `model/`: block/inline structures, ids, source ranges, affinity and text-edit
  value types. These types describe changes; editor command execution remains
  in core.
- `parsing/`: block and inline syntax recognition, parse-session state,
  lossless serialization and source projections.
- `query/`: source text, symbols and outline derived during parsing.
- `support/`: Unicode/entity tables, diagnostics and operation counters used
  to verify that consumers avoid full-document parsing during normal edits.

Inline source is authoritative; CST leaf ranges cover it character-exactly.
The parser and serializer keep the existing incomplete-input and lossless-save
contracts. Module extraction does not change syntax, ownership or coordinates.
