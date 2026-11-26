module folia.core.table_values;
import std;
import folia.markdown.utf;

namespace folia {

std::vector<std::u32string_view> table_tags(std::u32string_view value) {
    std::vector<std::u32string_view> tags;
    while (!value.empty()) {
        const auto separator = value.find_first_of(U",，、");
        auto tag = trim_table_value(value.substr(0, separator));
        if (!tag.empty() && !std::ranges::contains(tags, tag)) tags.push_back(tag);
        if (separator == value.npos) break;
        value.remove_prefix(separator + 1);
    }
    return tags;
}

std::optional<double> table_progress(std::u32string_view value) {
    value = trim_table_value(value);
    if (const auto slash = value.find(U'/'); slash != value.npos) {
        const auto numerator = table_number(value.substr(0, slash));
        const auto denominator = table_number(value.substr(slash + 1));
        if (numerator && denominator && *denominator > 0)
            return (std::clamp)(*numerator / *denominator, 0.0, 1.0);
        return std::nullopt;
    }
    auto number = table_number(value, true);
    if (!number) return std::nullopt;
    if (!value.ends_with(U"%") && (!value.contains(U'.') || std::fabs(*number) > 1)) *number /= 100;
    return (std::clamp)(*number, 0.0, 1.0);
}

std::optional<bool> table_checkbox(std::u32string_view value) {
    if (value.size() < 3 || value[0] != U'[' || value[2] != U']') return std::nullopt;
    const auto label = value.substr(3);
    if (!label.empty() && std::u32string_view(U" \t\r\n").find(label.front()) == label.npos
        && !label.starts_with(U"<br>") && !label.starts_with(U"<br/>") && !label.starts_with(U"<br />")) return std::nullopt;
    if (value[1] == U' ') return false;
    if (value[1] == U'x' || value[1] == U'X') return true;
    return std::nullopt;
}

std::u32string format_table_value(std::u32string_view value, const TableColumnType& type) {
    if (type.kind == TableColumnKind::Date) {
        if (auto date = table_date(value)) {
            auto result = table_date_text(date->date, type.date_format == TableDateFormat::Slash ? '/' : '-', true);
            return type.date_format == TableDateFormat::Link ? U"[[" + result + U"]]" : result;
        }
    } else if (type.kind == TableColumnKind::Number) {
        auto trimmed = trim_table_value(value);
        if (auto number = table_number(trimmed, true)) {
            auto shown = *number;
            if (type.percent && trimmed.ends_with(U"%")) shown *= 100;
            const auto point = trimmed.find(U'.');
            auto digits = point == trimmed.npos ? 0 : trimmed.size() - point - 1 - trimmed.ends_with(U"%");
            if (!type.percent && trimmed.ends_with(U"%")) digits += 2;
            auto precision = static_cast<unsigned>((std::min)(digits, std::size_t{15}));
            const bool pad = type.decimals.has_value();
            if (pad) precision = (std::max)(precision, *type.decimals);
            auto result = table_decimal(std::fabs(shown), precision, pad);
            if (type.grouping) {
                auto end = result.find(U'.');
                if (end == result.npos) end = result.size();
                while (end > 3) {
                    end -= 3;
                    result.insert(end, 1, U',');
                }
            }
            if (type.currency) result.insert(result.begin(), type.currency);
            if (shown < 0) result.insert(result.begin(), U'-');
            if (type.percent) result.push_back(U'%');
            return result;
        }
    } else if (type.kind == TableColumnKind::Multi) {
        std::u32string result;
        for (auto tag : table_tags(value)) {
            if (!result.empty()) result += U", ";
            result += tag;
        }
        return result;
    }
    return std::u32string(value);
}

std::u32string_view trim_table_value(std::u32string_view value) {
    const auto start = value.find_first_not_of(U" \t\r\n");
    return start == value.npos ? std::u32string_view{}
        : value.substr(start, value.find_last_not_of(U" \t\r\n") - start + 1);
}

std::optional<double> table_number(std::u32string_view value, bool formatted) {
    value = trim_table_value(value);
    if (value.empty()) return std::nullopt;
    auto is_currency = [](char32_t cp) { return cp == U'$' || cp == U'¥' || cp == U'€' || cp == U'£'; };
    const bool percent = formatted && value.ends_with(U"%");
    if (percent) value.remove_suffix(1);
    const bool signedCurrency = formatted && value.size() >= 2
        && (value.front() == U'-' || value.front() == U'+') && is_currency(value[1]);
    const bool signedNegative = signedCurrency && value.front() == U'-';
    if (signedCurrency) value.remove_prefix(1);
    if (formatted && !value.empty() && is_currency(value.front())) value.remove_prefix(1);
    auto bytes = cps_to_utf8(trim_table_value(value));
    if (formatted) std::erase(bytes, ',');
    std::string_view number = bytes;
    if (number.starts_with("+")) number.remove_prefix(1);
    if (number.empty()) return std::nullopt;
    double result{};
    auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), result, std::chars_format::fixed);
    if (error != std::errc{} || end != number.data() + number.size() || !std::isfinite(result)) return std::nullopt;
    if (signedNegative) result = -result;
    return percent ? result / 100 : result;
}



std::optional<TableDate> table_date(std::u32string_view value) {
    value = trim_table_value(value);
    if (value.starts_with(U"[[") && value.ends_with(U"]]")) value = value.substr(2, value.size() - 4);
    if (value.size() < 8 || value.size() > 10 || (value[4] != U'-' && value[4] != U'/')) return std::nullopt;
    const auto second = value.find(value[4], 5);
    if (second == value.npos || second < 6 || second > 7 || value.size() - second < 2 || value.size() - second > 3)
        return std::nullopt;
    auto integer = [](std::u32string_view digits) -> unsigned {
        unsigned result = 0;
        for (auto digit : digits) {
            if (digit < U'0' || digit > U'9') return 0;
            result = 10 * result + static_cast<unsigned>(digit - U'0');
        }
        return result;
    };
    auto date = std::chrono::year{static_cast<int>(integer(value.substr(0, 4)))}
        / integer(value.substr(5, second - 5)) / integer(value.substr(second + 1));
    if (!date.ok()) return std::nullopt;
    return TableDate{std::chrono::sys_days(date), static_cast<char>(value[4]), second == 7};
}

std::u32string table_decimal(double value, unsigned decimals, bool pad) {
    if (!std::isfinite(value)) return {};
    auto text = std::format("{:.{}f}", value == 0 ? 0 : value, decimals);
    if (!pad && text.contains('.')) {
        while (text.ends_with('0')) text.pop_back();
        if (text.ends_with('.')) text.pop_back();
    }
    if (text == "-0") text = "0";
    return utf8_to_cps(text);
}

std::u32string table_date_text(std::chrono::sys_days days, char separator, bool padded) {
    auto date = std::chrono::year_month_day(days);
    auto year = static_cast<int>(date.year());
    auto month = static_cast<unsigned>(date.month());
    auto day = static_cast<unsigned>(date.day());
    return utf8_to_cps(padded ? std::format("{:04}{}{:02}{}{:02}", year, separator, month, separator, day)
        : std::format("{:04}{}{}{}{}", year, separator, month, separator, day));
}

namespace table_values_detail {

bool digit(char32_t cp) { return cp >= U'0' && cp <= U'9'; }

struct NumberedText {
    std::u32string_view before;
    std::u32string_view after;
    double number;
    std::size_t padding;
};

std::optional<NumberedText> numbered_text(std::u32string_view text) {
    auto last = text.find_last_of(U"0123456789");
    if (last == text.npos) return std::nullopt;
    auto first = last;
    while (first > 0 && digit(text[first - 1])) --first;
    auto middle = text.substr(first, last + 1 - first);
    auto number = table_number(middle);
    if (!number || *number > 1'000'000'000'000.0) return std::nullopt;
    return NumberedText{text.substr(0, first), text.substr(last + 1), *number,
        middle.starts_with(U"0") ? middle.size() : 0};
}

std::optional<std::vector<std::u32string>> calendar_series(
    std::span<const std::u32string_view> values, std::size_t count, bool backwards) {
    using namespace std::literals::string_view_literals;
    static constexpr auto week = std::array{U"周一"sv,U"周二"sv,U"周三"sv,U"周四"sv,U"周五"sv,U"周六"sv,U"周日"sv};
    static constexpr auto long_week = std::array{U"星期一"sv,U"星期二"sv,U"星期三"sv,U"星期四"sv,U"星期五"sv,U"星期六"sv,U"星期日"sv};
    static constexpr auto english_week = std::array{U"Mon"sv,U"Tue"sv,U"Wed"sv,U"Thu"sv,U"Fri"sv,U"Sat"sv,U"Sun"sv};
    static constexpr auto months = std::array{U"一月"sv,U"二月"sv,U"三月"sv,U"四月"sv,U"五月"sv,U"六月"sv,U"七月"sv,U"八月"sv,U"九月"sv,U"十月"sv,U"十一月"sv,U"十二月"sv};
    static constexpr auto english_months = std::array{U"Jan"sv,U"Feb"sv,U"Mar"sv,U"Apr"sv,U"May"sv,U"Jun"sv,U"Jul"sv,U"Aug"sv,U"Sep"sv,U"Oct"sv,U"Nov"sv,U"Dec"sv};
    static constexpr auto stems = std::array{U"甲"sv,U"乙"sv,U"丙"sv,U"丁"sv,U"戊"sv,U"己"sv,U"庚"sv,U"辛"sv,U"壬"sv,U"癸"sv};
    const std::array<std::span<const std::u32string_view>, 6> vocabularies{
        week, long_week, english_week, months, english_months, stems};
    for (const auto list : vocabularies) {
        std::vector<std::ptrdiff_t> indices;
        for (auto value : values) {
            auto item = std::ranges::find(list, value);
            if (item == list.end()) break;
            indices.push_back(item - list.begin());
        }
        if (indices.size() != values.size()) continue;
        const auto step = indices.size() == 1 ? 1 : indices[1] - indices[0];
        if (std::ranges::adjacent_find(indices, [&](auto a, auto b) { return b - a != step; }) != indices.end()) continue;
        std::vector<std::u32string> result;
        auto current = backwards ? indices.front() : indices.back();
        const auto length = static_cast<std::ptrdiff_t>(list.size());
        for (std::size_t index = 0; index < count; ++index) {
            current = (current + (backwards ? -step : step) + length) % length;
            result.emplace_back(list[current]);
        }
        return result;
    }
    return std::nullopt;
}

} // namespace table_values_detail

std::vector<std::u32string> table_fill_series(
    std::span<const std::u32string_view> values, std::size_t count, bool backwards, bool copy) {
    std::vector<std::u32string> output;
    if (values.empty()) return output;
    output.reserve(count);
    auto distance = [&](std::size_t slot) {
        return backwards ? -static_cast<double>(slot + 1) : static_cast<double>(values.size() + slot);
    };
    const auto last_index = (std::max)(std::size_t{1}, values.size() - 1);
    if (!copy) {
        auto first_number = table_number(values.front());
        auto last_number = table_number(values.back());
        if (first_number && last_number && std::ranges::all_of(values, [](auto value) { return table_number(value).has_value(); })) {
            auto step = values.size() == 1 ? 1 : (*last_number - *first_number) / static_cast<double>(last_index);
            if (std::isfinite(step)) {
                for (std::size_t index = 0; index < count; ++index)
                    output.push_back(table_decimal(*first_number + step * distance(index), 6));
                return output;
            }
        }
        auto first_date = table_date(values.front());
        auto last_date = table_date(values.back());
        if (first_date && last_date && std::ranges::all_of(values, [](auto value) { return table_date(value).has_value(); })) {
            const auto step = values.size() == 1 ? 1.0
                : static_cast<double>((last_date->date - first_date->date).count()) / last_index;
            for (std::size_t index = 0; index < count; ++index) {
                auto days = std::chrono::days{static_cast<std::int64_t>(std::llround(step * distance(index)))};
                output.push_back(table_date_text(first_date->date + days, first_date->separator, first_date->padded));
            }
            return output;
        }
        std::vector<std::u32string_view> trimmed;
        for (auto value : values) trimmed.push_back(trim_table_value(value));
        if (auto result = table_values_detail::calendar_series(trimmed, count, backwards)) return std::move(*result);
        const auto first = table_values_detail::numbered_text(trimmed.front());
        const auto last = table_values_detail::numbered_text(trimmed.back());
        if (first && last) {
            const auto step = values.size() == 1 ? 1 : (last->number - first->number) / static_cast<double>(last_index);
            const bool arithmetic = step == std::round(step) && std::ranges::all_of(std::views::iota(std::size_t{0}, trimmed.size()), [&](auto index) {
                auto item = table_values_detail::numbered_text(trimmed[index]);
                return item && item->before == first->before && item->after == first->after
                    && item->number == first->number + step * static_cast<double>(index);
            });
            if (arithmetic) {
                for (std::size_t index = 0; index < count; ++index) {
                    auto number = table_decimal(first->number + step * distance(index), 0);
                    if (!number.starts_with(U"-") && number.size() < first->padding)
                        number.insert(0, first->padding - number.size(), U'0');
                    output.push_back(std::u32string(first->before) + number + std::u32string(first->after));
                }
                return output;
            }
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        auto slot = index % values.size();
        output.emplace_back(values[backwards ? values.size() - slot - 1 : slot]);
    }
    return output;
}

}
