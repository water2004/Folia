// Portable editing intents consumed by the editor transaction pipeline.
// Hosts provide clipboard access, text-service delivery and measured layout;
// navigation, selection and document mutation semantics live in the core.
export module folia.core.command;
import std;
import folia.markdown.table_metadata;
import folia.markdown.dialect;
import folia.markdown.text_edit;

export namespace folia {

enum class CommandKind {
    InsertText, DeleteBackward, DeleteForward, DeleteSelection, InsertNewline, InsertSoftBreak,
    MoveLeft, MoveRight, MoveUp, MoveDown,
    MoveWordLeft, MoveWordRight, DeleteWordBackward, DeleteWordForward,
    MoveParagraphStart, MoveParagraphEnd,
    MoveLineStart, MoveLineEnd, MoveDocumentStart, MoveDocumentEnd,
    MovePageUp, MovePageDown, SelectAll,
    ToggleStrong, ToggleEmphasis, ToggleStrikethrough, ToggleInlineCode, FormatDocument,
    SetHeading, ClearHeading,
    ToggleUnorderedList, ToggleOrderedList, ToggleTaskList, ToggleTaskCheckbox, ToggleBlockQuote,
    IndentListItem, OutdentListItem, IndentText, OutdentText, TabForward, TabBackward,
    InsertCodeBlock, SetCodeBlockLanguage, InsertMathInline, InsertMathBlock,
    InsertTable, InsertImage, InsertLink, InsertFootnote, CreateFootnoteDefinition, InsertToc,
    MoveTableCellNext, MoveTableCellPrevious,
    InsertTableRowAbove, InsertTableRowBelow, DeleteTableRow, MoveTableRowUp, MoveTableRowDown,
    InsertTableColumnLeft, InsertTableColumnRight, DeleteTableColumn, MoveTableColumnLeft, MoveTableColumnRight,
    SetTableColumnAlignment, SetTableColumnWidth, SetTableAppearance, CopyTable, NormalizeTable,
    SortTableRows, FillTableSelection, CopyPlainTable,
    CommitTableCell, SetTableCellValue, ToggleTableCheckbox, MakeTableCheckbox,
    DuplicateTableRows, DuplicateTableColumns, DeleteTable,
    InsertTableRowAt, InsertTableColumnAt, MoveTableRowTo, MoveTableColumnTo,
    ToggleCallout, ExtensionCmd,
    Undo, Redo, Cut, Copy, Paste, ReplaceSourceText, MoveSelection,
};

struct ReplaceTextTarget {
    TextSpan range;
    TextSelection selection_after;
};

struct Command {
    CommandKind kind = CommandKind::InsertText;
    std::u32string text{};                 // InsertText / Paste / Extension payload
    bool typed_input = false;             // Direct typing, not paste or snippet expansion
    bool extend_selection = false;         // Move*
    // Logical page size for clients without measured visual layout. Native
    // hosts use their viewport geometry; the portable fallback stays useful.
    std::size_t page_line_count = 20;
    std::uint8_t level = 0;               // SetHeading
    std::optional<std::u32string> lang;    // InsertCodeBlock / SetCodeBlockLanguage
    std::size_t rows = 0, cols = 0;       // InsertTable
    TableAlignment table_alignment = TableAlignment::None;
    std::size_t table_index = 0;
    std::optional<TableFormatEdit> table_format;
    std::optional<TableCoordinate> table_fill_target;
    bool table_copy_fill = false;
    std::u32string path, alt;              // InsertImage
    std::u32string href;                   // InsertLink
    std::string footnote_label;            // CreateFootnoteDefinition
    std::optional<std::u32string> title;    // InsertLink
    std::u32string callout_kind;           // ToggleCallout
    std::u32string ext_name;               // Extension
    std::optional<ReplaceTextTarget> replacement; // Explicit block-local replacement
    std::optional<TextSpan> paste_range;  // Semantic Markdown paste without changing selection first
    bool paste_as_table_grid = false;      // Clipboard identified this payload as TSV
    std::optional<TextPosition> move_destination;

    static Command InsertText(std::u32string t) { Command c; c.kind = CommandKind::InsertText; c.text = std::move(t); return c; }
    static Command MoveLeft(bool ext)  { Command c; c.kind = CommandKind::MoveLeft;  c.extend_selection = ext; return c; }
    static Command MoveRight(bool ext) { Command c; c.kind = CommandKind::MoveRight; c.extend_selection = ext; return c; }
    static Command MoveUp(bool ext)    { Command c; c.kind = CommandKind::MoveUp;    c.extend_selection = ext; return c; }
    static Command MoveDown(bool ext)  { Command c; c.kind = CommandKind::MoveDown;  c.extend_selection = ext; return c; }
    // ...other factories are small enough to inline at call-sites.
};

} // namespace folia
