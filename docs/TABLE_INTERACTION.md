# Table interaction contract

Folia stores Markdown pipe tables as a rectangular tree of rows and cells. Each
cell owns ordinary block-local Markdown source. Rich table behavior is layered
on top through the adjacent `<!-- gogoga-table: {...} -->` metadata comment:
metadata can describe merged-cell regions, dimensions, styles, colors, and other
table presentation and value settings without changing the pipe-table source
grid. The physical rows and cells stay rectangular when cells are merged.

HTML-backed tables remain source content, not editable Markdown table grids.
They use HTML parsing/rendering rules; HTML `rowspan`/`colspan` do not become
Folia Markdown table metadata or table structure commands.

## Selection

The editor has one selection value. It supports block-local text ranges and an
explicit rectangular table-cell range for table actions; these are selection
kinds, not parallel editor selections. Text positions continue to use a cell
owner ID, source offset, and affinity.

- Dragging through cell text creates a row-major text range. The first and last
  cells contribute only the selected source slices; intervening cells
  contribute all their source. Reversing the endpoints selects the same text.
- Copying a same-cell range returns that exact Markdown source substring.
  Copying a cross-cell range separates cells with tabs within a row and rows
  with newlines; it does not copy the whole table.
- A merged visual cell may map to multiple physical cell owners. Rendering,
  hit-testing, navigation, and text editing resolve those owners through the
  parsed merge metadata while preserving the rectangular source tree.
- An explicit cell-range selection is rectangular and expands to include any
  intersected merged regions. It is used for table actions such as appearance
  changes and merge/unmerge.

## Text and structure edits

Backspace, Delete, Cut, and replacement edit selected cell source. Across cells
they preserve every row and cell node; empty cells remain valid. Undo/Redo
restores the edited source and selection as one transaction. A selection that
crosses a table boundary is a structure barrier and must not remove table cells
or leave rows with unequal widths.

Row/column insertion, deletion, and reordering change the rectangular Markdown
grid and require an explicit table command with an unambiguous target cell.
Merge/unmerge and rich formatting update `gogoga-table` metadata instead; they
must preserve the pipe-table content and metadata's exact save/undo behavior.
A cross-cell text selection is not implicitly reinterpreted as a structural
command. Explicit rectangular cell-range actions remain available for metadata
operations.

## Keyboard behavior

- `Tab` moves to the next cell. In the final cell it appends one body row and
  moves to that row's first cell.
- `Shift+Tab` moves to the previous cell; in the first cell it does nothing.
- Navigation that changes only selection creates no history entry. Appending a
  row is a reversible structural transaction.
- Enter and soft-break behavior edit the current cell's Markdown source; they
  do not navigate or reshape the table.

The source tree remains a rectangular Markdown table throughout. Merge and
presentation semantics live in Folia's metadata layer, while HTML tables remain
in their own source-backed editing path.
