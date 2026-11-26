module;
#include <nlohmann/json.hpp>
module folia.markdown.table_metadata;
import std;
import folia.markdown.utf;

namespace folia {
namespace {

using Json = nlohmann::ordered_json;

constexpr std::u32string_view marker = U"gogoga-table:";
constexpr std::array<std::string_view, 7> color_names{
    "gray", "red", "orange", "yellow", "green", "blue", "purple"};
constexpr std::array<std::string_view, 6> total_names{"", "sum", "avg", "count", "max", "min"};

std::u32string_view trim(std::u32string_view text) {
    constexpr std::u32string_view whitespace = U" \t\r";
    const auto start = text.find_first_not_of(whitespace);
    if (start == text.npos) return {};
    return text.substr(start, text.find_last_not_of(whitespace) - start + 1);
}

Json read_metadata(std::u32string_view line) {
    line = trim(line);
    if (!line.starts_with(U"<!--") || !line.ends_with(U"-->")) return {};
    line = trim(line.substr(4, line.size() - 7));
    if (!line.starts_with(marker)) return {};
    line = trim(line.substr(marker.size()));
    if (!line.starts_with(U"{") || !line.ends_with(U"}")) return {};
    struct ExcessiveDepth {};
    try {
        return Json::parse(cps_to_utf8(line), [](int depth, auto, auto&) {
            if (depth > 128) throw ExcessiveDepth{};
            return true;
        }, false);
    } catch (const ExcessiveDepth&) {
        return {};
    }
}

const Json& value(const Json& object, std::string_view key) {
    static const Json missing;
    const auto found = object.find(key);
    return found == object.end() ? missing : *found;
}

std::string_view string(const Json& item) {
    const auto* text = item.get_ptr<const Json::string_t*>();
    return text ? std::string_view{*text} : std::string_view{};
}

double number(const Json& item, double fallback = 0) {
    return item.is_number() ? item.get<double>() : fallback;
}

float font_scale(const Json& item) {
    const auto scale = number(item, 1);
    return scale >= .5 && scale <= 3 ? static_cast<float>(scale) : 1.0f;
}

std::optional<TableColor> color(const Json& item) {
    const auto found = std::ranges::find(color_names, string(item));
    if (found == color_names.end()) return std::nullopt;
    return static_cast<TableColor>(found - color_names.begin());
}

std::optional<TableCoordinate> coordinate(std::string_view name) {
    constexpr auto maximum = (std::min)(
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()), std::uint64_t{9007199254740991});
    std::size_t column = 0;
    while (!name.empty() && name.front() >= 'A' && name.front() <= 'Z') {
        const auto digit = static_cast<std::size_t>(name.front() - 'A' + 1);
        if (column > (maximum - digit) / 26) return std::nullopt;
        column = column * 26 + digit;
        name.remove_prefix(1);
    }
    if (column == 0 || name.empty() || name.front() < '1' || name.front() > '9') return std::nullopt;
    std::size_t row = 0;
    const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), row);
    if (error != std::errc{} || end != name.data() + name.size() || row > maximum) return std::nullopt;
    return TableCoordinate{row - 1, column - 1};
}

std::string reference(TableCoordinate cell) {
    std::string column;
    for (auto index = cell.column + 1; index > 0; index = (index - 1) / 26)
        column.push_back(static_cast<char>('A' + (index - 1) % 26));
    std::ranges::reverse(column);
    return column + std::to_string(cell.row + 1);
}

std::optional<TableRegion> region(std::string_view name) {
    const auto colon = name.find(':');
    if (colon == name.npos) return std::nullopt;
    const auto from = coordinate(name.substr(0, colon));
    const auto to = coordinate(name.substr(colon + 1));
    if (!from || !to || from == to) return std::nullopt;
    return TableRegion{
        {(std::min)(from->row, to->row), (std::min)(from->column, to->column)},
        {(std::max)(from->row, to->row), (std::max)(from->column, to->column)}};
}

std::shared_ptr<const TableMetadata> decode(
    const Json& object, std::u32string line, std::u32string_view separator) {
    if (!object.is_object()) return {};
    auto output = std::make_shared<TableMetadata>();
    output->source = std::move(line);
    output->separator = separator;
    output->source_hash = std::hash<std::u32string_view>{}(output->source);
    if (const auto& widths = value(object, "w"); widths.is_array()) {
        for (const auto& item : widths) {
            const auto width = number(item);
            output->widths.push_back(width > 0 ? static_cast<float>(std::round((std::min)(width, 1000000.0))) : 0);
        }
    }
    if (const auto& alignments = value(object, "va"); alignments.is_array()) {
        for (const auto& item : alignments) {
            const auto alignment = string(item);
            output->vertical_alignments.push_back(alignment == "middle" ? TableVerticalAlignment::Middle
                : alignment == "bottom" ? TableVerticalAlignment::Bottom : TableVerticalAlignment::Top);
        }
    }
    if (const auto& totals = value(object, "calc"); totals.is_array()) {
        for (const auto& item : totals) {
            const auto found = std::ranges::find(total_names, string(item));
            output->totals.push_back(found == total_names.end() ? TableTotal::None
                : static_cast<TableTotal>(found - total_names.begin()));
        }
    }
    const auto& options = value(object, "opts");
    const auto& types = value(object, "ty");
    auto count = (std::max)(options.is_array() ? options.size() : 0, types.is_array() ? types.size() : 0);
    output->column_types.resize(count);
    if (options.is_array()) output->options.resize(options.size());
    for (std::size_t column = 0; column < count; ++column) {
        auto& type = output->column_types[column];
        if (options.is_array() && column < options.size() && options[column].is_object()) {
            type.kind = TableColumnKind::Status;
            for (auto item = options[column].begin(); item != options[column].end(); ++item)
                if (auto parsed = color(*item)) output->options[column][utf8_to_cps(item.key())] = *parsed;
        }
        if (!types.is_array() || column >= types.size() || !types[column].is_object()) continue;
        const auto& entry = types[column];
        const auto kind = string(value(entry, "t"));
        if (kind == "date") {
            type.kind = TableColumnKind::Date;
            const auto format = string(value(entry, "f"));
            if (format == "slash") type.date_format = TableDateFormat::Slash;
            if (format == "link") type.date_format = TableDateFormat::Link;
        } else if (kind == "num") {
            type.kind = TableColumnKind::Number;
            type.grouping = number(value(entry, "g")) == 1;
            type.percent = number(value(entry, "p")) == 1;
            auto currency = utf8_to_cps(string(value(entry, "u")));
            if (currency.size() == 1 && std::u32string_view{U"¥$€£"}.contains(currency.front()))
                type.currency = currency.front();
            const auto decimals = number(value(entry, "d"), -1);
            if (decimals >= 0 && decimals <= 4 && std::floor(decimals) == decimals)
                type.decimals = static_cast<unsigned>(decimals);
        } else if (kind == "bar") type.kind = TableColumnKind::Progress;
        else if (kind == "multi") type.kind = TableColumnKind::Multi;
        else if (kind == "enc") type.kind = TableColumnKind::Encrypted;
    }
    if (const auto& formulas = value(object, "fx"); formulas.is_array())
        for (const auto& entry : formulas) output->formulas.push_back(utf8_to_cps(string(entry)));
    if (const auto& decimals = value(object, "fxd"); decimals.is_array())
        for (const auto& entry : decimals) {
            const auto parsed = number(entry, -1);
            output->formula_decimals.push_back(parsed >= 0 && parsed <= 3 && std::floor(parsed) == parsed
                ? std::optional{static_cast<unsigned>(parsed)} : std::nullopt);
        }
    if (const auto& merges = value(object, "merge"); merges.is_array()) {
        for (const auto& item : merges)
            if (const auto range = region(string(item))) output->merges.push_back(*range);
    }
    auto read_cells = [&](std::string_view key, auto set) {
        const auto& items = value(object, key);
        if (!items.is_object()) return;
        for (auto it = items.begin(); it != items.end(); ++it)
            if (const auto cell = coordinate(it.key())) set(output->cells, *cell, *it);
    };
    read_cells("bg", [](auto& cells, auto at, const auto& item) {
        if (const auto parsed = color(item)) cells[at].background = parsed;
    });
    read_cells("fg", [](auto& cells, auto at, const auto& item) {
        if (const auto parsed = color(item)) cells[at].foreground = parsed;
    });
    read_cells("fz", [](auto& cells, auto at, const auto& item) {
        if (const auto scale = font_scale(item); scale != 1) cells[at].font_scale = scale;
    });
    const auto preset = string(value(object, "style"));
    if (preset == "zebra") output->preset = TablePreset::Zebra;
    else if (preset == "minimal") output->preset = TablePreset::Minimal;
    else if (preset == "academic") output->preset = TablePreset::Academic;
    const auto header_rows = number(value(object, "head"));
    if (header_rows == 2 || header_rows == 3) output->header_rows = static_cast<std::size_t>(header_rows);
    output->font_scale = font_scale(value(object, "fs"));
    output->header_column = value(object, "headerCol") == true;
    output->compact = string(value(object, "density")) == "compact";
    output->folded = number(value(object, "fold")) == 1;
    output->frozen = output->header_column && number(value(object, "freeze")) == 1;
    output->wide = number(value(object, "wide")) == 1;
    return output;
}

std::shared_ptr<const TableMetadata> encode(const Json& object, std::u32string_view separator) {
    if (object.empty()) return {};
    auto json = object.dump();
    std::string source = "<!-- gogoga-table: ";
    for (const char character : json) {
        // A pipe must never look like a table delimiter. Escaping '>' also
        // keeps an embedded HTML comment terminator inside its JSON string.
        if (character == '|') source += "\\u007c";
        else if (character == '>') source += "\\u003e";
        else source.push_back(character);
    }
    source += " -->";
    return decode(object, utf8_to_cps(source), separator);
}

std::optional<std::size_t> map_index(std::size_t index, TableAxisEdit edit) {
    using Kind = TableAxisEdit::Kind;
    if (edit.kind == Kind::Insert) return index >= edit.index ? index + edit.count : index;
    if (edit.kind == Kind::Remove) {
        if (index == edit.index) return std::nullopt;
        return index > edit.index ? index - 1 : index;
    }
    if (index == edit.index) return edit.destination;
    if (edit.index < index && index <= edit.destination) return index - 1;
    if (edit.destination <= index && index < edit.index) return index + 1;
    return index;
}

std::optional<std::pair<std::size_t, std::size_t>> map_interval(
    std::size_t first, std::size_t last, TableAxisEdit edit) {
    using Kind = TableAxisEdit::Kind;
    if (edit.kind == Kind::Insert) return std::pair{*map_index(first, edit), *map_index(last, edit)};
    if (edit.kind == Kind::Remove) {
        if (first == last && first == edit.index) return std::nullopt;
        return std::pair{*map_index(first + (first == edit.index), edit),
            *map_index(last - (last == edit.index), edit)};
    }
    // A move is piecewise monotone. The extrema of its continuous pieces
    // and the moved index suffice to determine whether a merge stays whole.
    auto low = (std::numeric_limits<std::size_t>::max)();
    std::size_t high = 0;
    auto include = [&](std::size_t at) {
        if (at < first || at > last) return;
        const auto mapped = *map_index(at, edit);
        low = (std::min)(low, mapped);
        high = (std::max)(high, mapped);
    };
    include(first);
    include(last);
    for (const auto edge : {edit.index, edit.destination}) {
        if (edge > 0) include(edge - 1);
        include(edge);
        include(edge + 1);
    }
    if (high - low != last - first) return std::nullopt;
    return std::pair{low, high};
}

}

std::shared_ptr<const TableMetadata> parse_table_metadata(
    std::u32string_view line, std::u32string_view separator) {
    return decode(read_metadata(line), std::u32string(line), separator);
}

std::shared_ptr<const TableMetadata> remap_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, TableAxisEdit edit) {
    if (!metadata) return {};
    using Kind = TableAxisEdit::Kind;
    if (edit.kind == Kind::Move && edit.index == edit.destination) return metadata;
    const auto original = read_metadata(metadata->source);
    auto object = original;
    const bool columns = edit.axis == TableAxisEdit::Axis::Column;
    for (const auto* key : {"bg", "fg", "fz"}) {
        const auto& from = value(object, key);
        if (!from.is_object()) continue;
        auto to = Json::object();
        for (auto item = from.begin(); item != from.end(); ++item) {
            auto at = coordinate(item.key());
            if (!at) { to[item.key()] = *item; continue; }
            auto& axis = columns ? at->column : at->row;
            const auto original_axis = axis;
            const auto mapped = map_index(axis, edit);
            if (!mapped) continue;
            axis = *mapped;
            to[reference(*at)] = *item;
            if (edit.duplicate_from && original_axis >= *edit.duplicate_from
                && original_axis - *edit.duplicate_from < edit.count) {
                axis = edit.index + original_axis - *edit.duplicate_from;
                to[reference(*at)] = *item;
            }
        }
        if (to.empty()) object.erase(key);
        else object[key] = std::move(to);
    }
    if (const auto& from = value(object, "merge"); from.is_array()) {
        auto to = Json::array();
        for (const auto& item : from) {
            auto area = region(string(item));
            if (!area) { to.push_back(item); continue; }
            auto& first = columns ? area->first.column : area->first.row;
            auto& last = columns ? area->last.column : area->last.row;
            const auto interval = map_interval(first, last, edit);
            if (!interval) continue;
            std::tie(first, last) = *interval;
            if (area->first != area->last)
                to.push_back(reference(area->first) + ":" + reference(area->last));
        }
        if (to.empty()) object.erase("merge");
        else object["merge"] = std::move(to);
    }
    if (columns) {
        for (const auto* key : {"w", "va", "calc", "fx", "fxd", "opts", "ty"}) {
            const auto& from = value(object, key);
            if (!from.is_array()) continue;
            auto to = Json::array();
            for (std::size_t i = 0; i < from.size(); ++i)
                if (const auto mapped = map_index(i, edit)) to[*mapped] = from[i];
            if (edit.duplicate_from && std::string_view{key} != "fx" && std::string_view{key} != "fxd")
                for (std::size_t i = 0; i < edit.count && *edit.duplicate_from + i < from.size(); ++i)
                    to[edit.index + i] = from[*edit.duplicate_from + i];
            if (to.empty()) object.erase(key);
            else object[key] = std::move(to);
        }
    } else {
        const auto header_rows = metadata->header_rows;
        if (edit.index < header_rows && edit.kind != Kind::Move) {
            const auto count = edit.kind == Kind::Insert ? (std::min)(header_rows + edit.count, std::size_t{3})
                : (std::max)(header_rows - 1, std::size_t{1});
            if (count > 1) object["head"] = count;
            else object.erase("head");
        }
    }
    if (object == original) return metadata;
    return encode(object, metadata->separator);
}

std::shared_ptr<const TableMetadata> slice_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, TableRegion area) {
    if (!metadata) return {};
    const auto original = read_metadata(metadata->source);
    auto output = Json::object();
    for (const auto* key : {"style", "density", "fs"})
        if (original.contains(key)) output[key] = original[key];
    if (area.first.column == 0)
        for (const auto* key : {"headerCol", "freeze"})
            if (original.contains(key)) output[key] = original[key];
    if (area.first.row < metadata->header_rows) {
        const auto shown = (std::min)(area.last.row + 1, metadata->header_rows) - area.first.row;
        if (shown > 1) output["head"] = shown;
    }
    for (const auto* key : {"w", "va", "opts", "ty"}) {
        const auto& from = value(original, key);
        if (!from.is_array()) continue;
        auto to = Json::array();
        for (auto column = area.first.column; column <= area.last.column && column < from.size(); ++column)
            to.push_back(from[column]);
        while (!to.empty() && to.back().is_null()) to.erase(to.size() - 1);
        if (!to.empty()) output[key] = std::move(to);
    }
    auto offset = [&](TableCoordinate cell) {
        return TableCoordinate{cell.row - area.first.row, cell.column - area.first.column};
    };
    for (const auto* key : {"bg", "fg", "fz"}) {
        const auto& from = value(original, key);
        if (!from.is_object()) continue;
        auto to = Json::object();
        for (auto item = from.begin(); item != from.end(); ++item)
            if (const auto cell = coordinate(item.key()); cell && area.contains(*cell))
                to[reference(offset(*cell))] = *item;
        if (!to.empty()) output[key] = std::move(to);
    }
    auto merges = Json::array();
    for (const auto& merge : metadata->merges)
        if (area.contains(merge.first) && area.contains(merge.last))
            merges.push_back(reference(offset(merge.first)) + ":" + reference(offset(merge.last)));
    if (!merges.empty()) output["merge"] = std::move(merges);
    return encode(output, U"\n");
}

std::shared_ptr<const TableMetadata> rename_table_formula_references(
    const std::shared_ptr<const TableMetadata>& metadata,
    const std::map<std::u32string, std::u32string, std::less<>>& names) {
    if (!metadata || names.empty()) return metadata;
    auto object = read_metadata(metadata->source);
    auto formulas = object.find("fx");
    if (formulas == object.end() || !formulas->is_array()) return metadata;
    bool changed = false;
    for (auto& item : *formulas) {
        if (!item.is_string()) continue;
        const auto source = utf8_to_cps(string(item));
        std::u32string rewritten;
        std::size_t start = 0;
        auto open = source.find(U'{');
        while (open != source.npos) {
            const auto close = source.find(U'}', open + 1);
            if (close == source.npos) break;
            const auto token = std::u32string_view{source}.substr(open + 1, close - open - 1);
            if (const auto name = names.find(trim(token)); name != names.end()) {
                rewritten.append(source, start, open + 1 - start);
                rewritten += name->second;
                start = close;
            }
            open = source.find(U'{', close + 1);
        }
        if (start == 0) continue;
        rewritten.append(source, start);
        item = cps_to_utf8(rewritten);
        changed = true;
    }
    return changed ? encode(object, metadata->separator) : metadata;
}

std::shared_ptr<const TableMetadata> paste_table_metadata(
    const std::shared_ptr<const TableMetadata>& destination,
    const TableMetadata& clipboard, TableRegion area, std::u32string_view separator) {
    const auto copied = read_metadata(clipboard.source);
    const auto original = destination ? read_metadata(destination->source) : Json::object();
    auto output = original;
    const TableRegion source{{0, 0}, {area.last.row - area.first.row, area.last.column - area.first.column}};
    auto translated = [&](TableCoordinate cell) {
        return TableCoordinate{area.first.row + cell.row, area.first.column + cell.column};
    };
    for (const auto* key : {"bg", "fg", "fz"}) {
        const auto& entries = value(copied, key);
        if (!entries.is_object()) continue;
        auto overrides = value(original, key);
        if (!overrides.is_object()) overrides = Json::object();
        for (auto item = entries.begin(); item != entries.end(); ++item)
            if (auto cell = coordinate(item.key()); cell && source.contains(*cell))
                overrides[reference(translated(*cell))] = *item;
        if (!overrides.empty()) output[key] = std::move(overrides);
    }
    for (const auto* key : {"w", "va", "ty", "opts"}) {
        const auto& entries = value(copied, key);
        if (!entries.is_array()) continue;
        auto overrides = value(original, key);
        if (!overrides.is_array()) overrides = Json::array();
        for (std::size_t i = 0; i < entries.size() && i <= source.last.column; ++i)
            if (!entries[i].is_null()) overrides[area.first.column + i] = entries[i];
        if (!overrides.empty()) output[key] = std::move(overrides);
    }
    if (!clipboard.merges.empty()) {
        auto merges = value(original, "merge");
        if (!merges.is_array()) merges = Json::array();
        for (const auto& item : clipboard.merges) {
            if (!source.contains(item.first) || !source.contains(item.last)) continue;
            const TableRegion inserted{translated(item.first), translated(item.last)};
            if (std::ranges::any_of(merges, [&](const auto& entry) {
                const auto existing = region(string(entry));
                return existing && inserted.intersects(*existing);
            })) continue;
            merges.push_back(reference(inserted.first) + ":" + reference(inserted.last));
        }
        if (!merges.empty()) output["merge"] = std::move(merges);
    }
    return output != original ? encode(output, destination ? destination->separator : separator) : destination;
}

std::shared_ptr<const TableMetadata> reorder_table_metadata_rows(
    const std::shared_ptr<const TableMetadata>& metadata, std::size_t start,
    std::span<const std::size_t> destinations) {
    if (!metadata) return {};
    const auto original = read_metadata(metadata->source);
    auto object = original;
    auto map = [&](std::size_t row) {
        return row >= start && row - start < destinations.size() ? start + destinations[row - start] : row;
    };
    for (const auto* key : {"bg", "fg", "fz"}) {
        const auto& entries = value(original, key);
        if (!entries.is_object()) continue;
        auto reordered = Json::object();
        for (auto item = entries.begin(); item != entries.end(); ++item) {
            auto coordinate_value = coordinate(item.key());
            if (!coordinate_value) { reordered[item.key()] = *item; continue; }
            coordinate_value->row = map(coordinate_value->row);
            reordered[reference(*coordinate_value)] = *item;
        }
        object[key] = std::move(reordered);
    }
    if (const auto& entries = value(original, "merge"); entries.is_array()) {
        auto reordered = Json::array();
        for (const auto& entry : entries) {
            auto merge = region(string(entry));
            if (merge && merge->first.row == merge->last.row) {
                merge->first.row = merge->last.row = map(merge->first.row);
                reordered.push_back(reference(merge->first) + ":" + reference(merge->last));
            } else reordered.push_back(entry);
        }
        object["merge"] = std::move(reordered);
    }
    return object == original ? metadata : encode(object, metadata->separator);
}

std::shared_ptr<const TableMetadata> set_table_column_width(
    const std::shared_ptr<const TableMetadata>& metadata, std::size_t column,
    std::size_t width, std::u32string_view separator) {
    auto object = metadata ? read_metadata(metadata->source) : Json::object();
    auto widths = value(object, "w");
    if (!widths.is_array()) widths = Json::array();
    if (width > 0 || column < widths.size())
        widths[column] = width > 0 ? Json(width) : Json();
    while (!widths.empty() && widths.back().is_null()) widths.erase(widths.size() - 1);
    if (widths.empty()) object.erase("w");
    else object["w"] = std::move(widths);
    if (!object.contains("w")) object.erase("wide");
    return encode(object, metadata ? std::u32string_view{metadata->separator} : separator);
}

std::shared_ptr<const TableMetadata> format_table_metadata(
    const std::shared_ptr<const TableMetadata>& metadata, const TableFormatEdit& edit,
    TableRegion selection, std::u32string_view separator) {
    const auto original = metadata ? read_metadata(metadata->source) : Json::object();
    auto object = original;
    auto set = [&](const char* key, Json item) {
        if (item.is_null()) object.erase(key);
        else object[key] = std::move(item);
    };
    auto style_cells = [&](const char* key, const Json& item) {
        auto cells = value(object, key);
        if (!cells.is_object()) cells = Json::object();
        for (auto row = selection.first.row; row <= selection.last.row; ++row)
            for (auto column = selection.first.column; column <= selection.last.column; ++column) {
                const auto name = reference({row, column});
                if (item.is_null()) cells.erase(name);
                else cells[name] = item;
            }
        set(key, cells.empty() ? Json() : std::move(cells));
    };
    auto style_columns = [&](const char* key, const Json& item) {
        auto entries = value(object, key);
        if (!entries.is_array()) entries = Json::array();
        for (auto column = selection.first.column; column <= selection.last.column; ++column)
            if (!item.is_null() || column < entries.size()) entries[column] = item;
        while (!entries.empty() && entries.back().is_null()) entries.erase(entries.size() - 1);
        set(key, entries.empty() ? Json() : std::move(entries));
    };
    using Kind = TableFormatKind;
    switch (edit.kind) {
        case Kind::Preset: {
            constexpr std::array<std::string_view, 4> presets{"default", "zebra", "minimal", "academic"};
            set("style", presets.at(static_cast<std::size_t>(edit.preset)));
            break;
        }
        case Kind::Density:
            set("density", edit.enabled ? "compact" : "comfortable");
            break;
        case Kind::HeaderColumn:
            set("headerCol", edit.enabled ? Json(true) : Json());
            if (!edit.enabled) object.erase("freeze");
            break;
        case Kind::Folded:
            set("fold", edit.enabled ? Json(1) : Json());
            break;
        case Kind::FreezeHeader:
            if (edit.enabled) object["headerCol"] = true;
            set("freeze", edit.enabled ? Json(1) : Json());
            break;
        case Kind::Wide:
            set("wide", edit.enabled ? Json(1) : Json());
            break;
        case Kind::ClearColors:
            object.erase("bg");
            object.erase("fg");
            break;
        case Kind::AutoWidths:
            object.erase("w");
            object.erase("wide");
            break;
        case Kind::ColumnWidths:
            if (!edit.widths.empty()) {
                auto widths = Json::array();
                for (auto width : edit.widths)
                    widths.push_back(std::isfinite(width) && width > 0
                        ? Json(std::clamp(std::round(width), 1.0f, 1000000.0f)) : Json());
                set("w", std::move(widths));
                set("wide", edit.enabled ? Json(1) : Json());
            }
            break;
        case Kind::EqualWidths:
            if (edit.column_width > 0) {
                auto widths = Json::array();
                for (auto column = selection.first.column; column <= selection.last.column; ++column)
                    widths.push_back(edit.column_width);
                set("w", std::move(widths));
            }
            break;
        case Kind::HeaderRows:
            set("head", edit.header_rows == 2 || edit.header_rows == 3 ? Json(edit.header_rows) : Json());
            break;
        case Kind::Background:
        case Kind::Foreground:
            style_cells(edit.kind == Kind::Background ? "bg" : "fg",
                edit.color ? Json(color_names.at(static_cast<std::size_t>(*edit.color))) : Json());
            break;
        case Kind::FontScale:
            style_cells("fz", edit.font_scale >= .5f && edit.font_scale <= 3 && edit.font_scale != 1
                ? Json(static_cast<double>(std::round(edit.font_scale * 100)) / 100) : Json());
            break;
        case Kind::TableFontScale:
            set("fs", edit.font_scale >= .5f && edit.font_scale <= 3 && edit.font_scale != 1
                ? Json(static_cast<double>(std::round(edit.font_scale * 100)) / 100) : Json());
            break;
        case Kind::Total: {
            const auto entry = edit.total == TableTotal::None ? Json()
                : Json(total_names.at(static_cast<std::size_t>(edit.total)));
            style_columns("calc", entry);
            break;
        }
        case Kind::Formula:
            style_columns("fx", edit.text.empty() ? Json() : Json(cps_to_utf8(edit.text)));
            if (edit.text.empty()) style_columns("fxd", Json());
            break;
        case Kind::FormulaDecimals:
            style_columns("fxd", edit.decimals && *edit.decimals <= 3 ? Json(*edit.decimals) : Json());
            break;
        case Kind::ColumnType: {
            auto type = edit.column_type;
            auto entry = Json::object();
            switch (type.kind) {
                case TableColumnKind::Text:
                case TableColumnKind::Status: entry = Json(); break;
                case TableColumnKind::Multi: entry["t"] = "multi"; break;
                case TableColumnKind::Progress: entry["t"] = "bar"; break;
                case TableColumnKind::Encrypted: entry["t"] = "enc"; break;
                case TableColumnKind::Date:
                    entry["t"] = "date";
                    if (type.date_format != TableDateFormat::Dash)
                        entry["f"] = type.date_format == TableDateFormat::Link ? "link" : "slash";
                    break;
                case TableColumnKind::Number:
                    entry["t"] = "num";
                    if (type.decimals && *type.decimals <= 4) entry["d"] = *type.decimals;
                    if (type.currency) entry["u"] = cps_to_utf8(std::u32string(1, type.currency));
                    if (type.grouping) entry["g"] = 1;
                    if (type.percent) entry["p"] = 1;
                    break;
            }
            style_columns("ty", entry);
            if (type.kind == TableColumnKind::Status || type.kind == TableColumnKind::Multi) {
                auto options = value(object, "opts");
                if (!options.is_array()) options = Json::array();
                for (auto column = selection.first.column; column <= selection.last.column; ++column)
                    if (column >= options.size() || !options[column].is_object()) options[column] = Json::object();
                set("opts", std::move(options));
            } else style_columns("opts", Json());
            break;
        }
        case Kind::OptionColor: {
            if (edit.text.empty()) return metadata;
            auto options = value(object, "opts");
            if (!options.is_array()) options = Json::array();
            const auto name = cps_to_utf8(edit.text);
            for (auto column = selection.first.column; column <= selection.last.column; ++column) {
                if (column >= options.size() || !options[column].is_object()) options[column] = Json::object();
                if (edit.color) options[column][name] = color_names.at(static_cast<std::size_t>(*edit.color));
                else options[column].erase(name);
            }
            set("opts", std::move(options));
            break;
        }
        case Kind::VerticalAlignment: {
            auto alignments = value(object, "va");
            if (!alignments.is_array()) alignments = Json::array();
            Json align;
            if (edit.alignment == TableVerticalAlignment::Middle) align = "middle";
            else if (edit.alignment == TableVerticalAlignment::Bottom) align = "bottom";
            for (auto column = selection.first.column; column <= selection.last.column; ++column)
                if (!align.is_null() || column < alignments.size()) alignments[column] = align;
            while (!alignments.empty() && alignments.back().is_null()) alignments.erase(alignments.size() - 1);
            set("va", alignments.empty() ? Json() : std::move(alignments));
            break;
        }
        case Kind::Merge:
        case Kind::Unmerge: {
            if (edit.kind == Kind::Merge && selection.first == selection.last) return metadata;
            auto merges = Json::array();
            for (const auto& item : value(object, "merge")) {
                const auto area = region(string(item));
                const auto overlaps = area && area->first.row <= selection.last.row
                    && area->last.row >= selection.first.row && area->first.column <= selection.last.column
                    && area->last.column >= selection.first.column;
                if (!overlaps) merges.push_back(item);
                else if (edit.kind == Kind::Merge
                    && (area->first.row < selection.first.row || area->last.row > selection.last.row
                        || area->first.column < selection.first.column || area->last.column > selection.last.column))
                    return metadata;
            }
            if (edit.kind == Kind::Merge)
                merges.push_back(reference(selection.first) + ":" + reference(selection.last));
            set("merge", merges.empty() ? Json() : std::move(merges));
            break;
        }
    }
    if (object == original) return metadata;
    return encode(object, metadata ? std::u32string_view{metadata->separator} : separator);
}

}
