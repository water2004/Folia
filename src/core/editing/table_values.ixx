export module folia.core.table_values;
import std;
import folia.markdown.table_metadata;

export namespace folia {

struct TableDate {
    std::chrono::sys_days date;
    char separator;
    bool padded;
};

std::u32string_view trim_table_value(std::u32string_view value);
std::optional<double> table_number(std::u32string_view value, bool formatted = false);
std::optional<TableDate> table_date(std::u32string_view value);
std::u32string table_decimal(double value, unsigned decimals, bool pad = false);
std::u32string table_date_text(std::chrono::sys_days date, char separator, bool padded);
std::vector<std::u32string> table_fill_series(
    std::span<const std::u32string_view> values, std::size_t count, bool backwards, bool copy);
std::vector<std::u32string_view> table_tags(std::u32string_view value);
std::optional<double> table_progress(std::u32string_view value);
std::optional<bool> table_checkbox(std::u32string_view value);
std::u32string format_table_value(std::u32string_view value, const TableColumnType& type);

}
