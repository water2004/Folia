export module folia.markdown.table_metadata;
import std;

export namespace folia {

enum class TablePreset { Default, Zebra, Minimal, Academic };
enum class TableVerticalAlignment { Top, Middle, Bottom };
enum class TableColor { Gray, Red, Orange, Yellow, Green, Blue, Purple };
enum class TableTotal { None, Sum, Average, Count, Maximum, Minimum };
enum class TableColumnKind { Text, Status, Multi, Date, Number, Progress, Encrypted };
enum class TableDateFormat { Dash, Slash, Link };

struct TableColumnType {
    TableColumnKind kind = TableColumnKind::Text;
    TableDateFormat date_format = TableDateFormat::Dash;
    std::optional<unsigned> decimals;
    char32_t currency = 0;
    bool grouping = false;
    bool percent = false;
    bool operator==(const TableColumnType&) const = default;
};

struct TableCoordinate {
    std::size_t row;
    std::size_t column;
    auto operator<=>(const TableCoordinate&) const = default;
};

struct TableRegion {
    TableCoordinate first;
    TableCoordinate last;

    bool contains(TableCoordinate cell) const {
        return first.row <= cell.row && cell.row <= last.row
            && first.column <= cell.column && cell.column <= last.column;
    }
    bool intersects(const TableRegion& other) const {
        return first.row <= other.last.row && other.first.row <= last.row
            && first.column <= other.last.column && other.first.column <= last.column;
    }
    void include(const TableRegion& other) {
        first = {(std::min)(first.row, other.first.row), (std::min)(first.column, other.first.column)};
        last = {(std::max)(last.row, other.last.row), (std::max)(last.column, other.last.column)};
    }
    bool operator==(const TableRegion&) const = default;
};

struct TableCellAppearance {
    std::optional<TableColor> background;
    std::optional<TableColor> foreground;
    float font_scale = 1;
};

struct TableMetadata {
    std::u32string source;
    std::u32string separator;
    std::size_t source_hash = 0;
    std::vector<float> widths;
    std::vector<TableVerticalAlignment> vertical_alignments;
    std::vector<TableTotal> totals;
    std::vector<TableColumnType> column_types;
    std::vector<std::map<std::u32string, TableColor, std::less<>>> options;
    std::vector<std::u32string> formulas;
    std::vector<std::optional<unsigned>> formula_decimals;
    std::vector<TableRegion> merges;
    std::map<TableCoordinate, TableCellAppearance> cells;
    TablePreset preset = TablePreset::Default;
    std::size_t header_rows = 1;
    float font_scale = 1;
    bool header_column = false;
    bool compact = false;
    bool folded = false;
    bool frozen = false;
    bool wide = false;
};

struct TableAxisEdit {
    enum class Axis { Row, Column };
    enum class Kind { Insert, Remove, Move };
    Axis axis;
    Kind kind;
    std::size_t index;
    std::size_t destination = 0;
    std::size_t count = 1;
    std::optional<std::size_t> duplicate_from;
};

enum class TableFormatKind {
    Preset, Density, VerticalAlignment, Background, Foreground,
    FontScale, HeaderRows, HeaderColumn, Merge, Unmerge, Folded, FreezeHeader, Wide,
    TableFontScale, ClearColors, AutoWidths, EqualWidths, Total, ColumnType, OptionColor,
    Formula, FormulaDecimals, ColumnWidths,
};

struct TableFormatEdit {
    TableFormatKind kind;
    TablePreset preset = TablePreset::Default;
    TableVerticalAlignment alignment = TableVerticalAlignment::Top;
    std::optional<TableColor> color;
    std::size_t header_rows = 1;
    float font_scale = 1;
    bool enabled = false;
    TableTotal total = TableTotal::None;
    std::size_t column_width = 0;
    TableColumnType column_type;
    std::u32string text;
    std::optional<unsigned> decimals;
    std::vector<float> widths;
};

std::shared_ptr<const TableMetadata> parse_table_metadata(
    std::u32string_view line, std::u32string_view separator);
std::shared_ptr<const TableMetadata> remap_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, TableAxisEdit edit);
std::shared_ptr<const TableMetadata> slice_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, TableRegion region);
std::shared_ptr<const TableMetadata> rename_table_formula_references(
    const std::shared_ptr<const TableMetadata>& metadata,
    const std::map<std::u32string, std::u32string, std::less<>>& names);
std::shared_ptr<const TableMetadata> paste_table_metadata(
    const std::shared_ptr<const TableMetadata>& destination,
    const TableMetadata& clipboard, TableRegion area, std::u32string_view separator);
std::shared_ptr<const TableMetadata> reorder_table_metadata_rows(
    const std::shared_ptr<const TableMetadata>& metadata, std::size_t start,
    std::span<const std::size_t> destinations);
std::shared_ptr<const TableMetadata> set_table_column_width(
    const std::shared_ptr<const TableMetadata>& metadata, std::size_t column,
    std::size_t width, std::u32string_view separator);
std::shared_ptr<const TableMetadata> format_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, const TableFormatEdit& edit,
    TableRegion selection, std::u32string_view separator);

inline const TableRegion* table_merge_at(const TableMetadata* metadata,
    TableCoordinate position, std::size_t rows, std::size_t columns) {
    if (!metadata) return nullptr;
    const auto merge = std::ranges::find_if(metadata->merges, [&](const auto& item) {
        return item.last.row < rows && item.last.column < columns
            && item.contains(position);
    });
    return merge == metadata->merges.end() ? nullptr : &*merge;
}

inline bool table_cell_is_covered(const TableMetadata* metadata,
    TableCoordinate position, std::size_t rows, std::size_t columns) {
    const auto* merge = table_merge_at(metadata, position, rows, columns);
    return merge && merge->first != position;
}

inline bool table_has_totals(const TableMetadata* metadata) {
    return metadata && std::ranges::any_of(metadata->totals, [](auto kind) { return kind != TableTotal::None; });
}

}
