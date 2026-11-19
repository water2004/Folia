module folia.mermaid.treemap;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {

// Default-locale d3-format 3.1.0, as used by the pinned Treemap renderer.
// Keep specifier parsing, decimal conversion, and presentation separate: the
// renderer's currency wrapper is applied only after the D3 format is selected.
struct ValueFormat {
    char16_t fill = u' ', align = u'>', sign = u'-', symbol = 0, type = 0;
    bool zero = false, comma = false, trim = false;
    std::size_t width = 0;
    std::optional<unsigned> precision;
};

bool digit(char16_t c) { return c >= u'0' && c <= u'9'; }
bool alignment(char16_t c) { return c == u'<' || c == u'>' || c == u'=' || c == u'^'; }
std::optional<ValueFormat> parse_format(std::u16string_view text) {
    ValueFormat result;
    std::size_t position = 0;
    if (text.size() >= 2 && alignment(text[1]) && std::u16string_view{u"\n\r\u2028\u2029"}.find(text[0]) == std::u16string_view::npos) {
        result.fill = text[0]; result.align = text[1]; position = 2;
    } else if (!text.empty() && alignment(text[0])) result.align = text[position++];
    const auto take = [&](std::u16string_view characters) {
        return position < text.size() && characters.find(text[position]) != characters.npos;
    };
    if (take(u"+-( ")) result.sign = text[position++];
    if (take(u"$#")) result.symbol = text[position++];
    if (take(u"0")) { result.zero = true; ++position; }
    while (position < text.size() && digit(text[position])) {
        const auto next = static_cast<unsigned>(text[position++] - u'0');
        if (result.width > (std::numeric_limits<std::size_t>::max() - next) / 10)
            throw std::runtime_error("Treemap format width is too large");
        result.width = result.width * 10 + next;
    }
    if (take(u",")) { result.comma = true; ++position; }
    if (take(u".")) {
        ++position;
        if (position == text.size() || !digit(text[position])) return {};
        unsigned precision = 0;
        while (position < text.size() && digit(text[position]))
            precision = std::min(100u, precision * 10 + static_cast<unsigned>(text[position++] - u'0'));
        result.precision = precision;
    }
    if (take(u"~")) { result.trim = true; ++position; }
    if (position < text.size()) {
        const auto type = text[position++];
        if (!(type == u'%' || (type >= u'a' && type <= u'z') || (type >= u'A' && type <= u'Z')))
            return {};
        result.type = type;
    }
    if (position != text.size()) return {};
    if (result.type == u'n') { result.comma = true; result.type = u'g'; }
    else if (!result.type || std::u16string_view{u"%bcdefgoprsXx"}.find(result.type) == std::u16string_view::npos) {
        if (!result.precision) result.precision = 12;
        result.trim = true; result.type = u'g';
    }
    if (result.zero || (result.fill == u'0' && result.align == u'=')) {
        result.zero = true; result.fill = u'0'; result.align = u'=';
    }
    const bool significant = std::u16string_view{u"gprs"}.find(result.type) != std::u16string_view::npos;
    result.precision = result.precision ? std::clamp(*result.precision, significant ? 1u : 0u,
        significant ? 21u : 20u) : 6u;
    return result;
}

// JS decimal conversions resolve an exact halfway value away from zero, while
// C++ resolves it to even. Detect the rational midpoint of the original binary64
// value, avoiding a second rounding through value * pow(10, precision).
bool rounded_down_at_midpoint(double value, int decimal_scale) {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto exponent_bits = (bits >> 52) & 0x7ff;
    auto significand = (bits & ((std::uint64_t{1} << 52) - 1)) |
        (exponent_bits ? std::uint64_t{1} << 52 : 0);
    const int exponent = exponent_bits ? static_cast<int>(exponent_bits) - 1075 : -1074;
    if (!significand) return false;
    for (int i = decimal_scale; i < 0; ++i) {
        if (significand % 5) return false;
        significand /= 5;
    }
    const auto shift = -exponent - decimal_scale;
    if (shift <= 0 || shift >= 64 || std::countr_zero(significand) != shift - 1) return false;
    // Only low bits through shift are needed; unsigned overflow is intentional.
    for (int i = 0; i < decimal_scale; ++i) significand *= 5;
    return ((significand >> shift) & 1) == 0;
}

struct DecimalParts { std::string coefficient; int exponent; };
DecimalParts decimal_parts(double value, unsigned precision = 0) {
    std::array<char, 128> buffer{};
    const auto converted = precision
        ? std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
            std::chars_format::scientific, static_cast<int>(precision) - 1)
        : std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
    if (converted.ec != std::errc{}) throw std::runtime_error("Invalid Treemap decimal conversion");
    const std::string_view text{buffer.data(), converted.ptr};
    const auto separator = text.find('e');
    auto exponent_text = text.substr(separator + 1);
    if (exponent_text.starts_with('+')) exponent_text.remove_prefix(1);
    DecimalParts result;
    const auto parsed = std::from_chars(exponent_text.data(), exponent_text.data() + exponent_text.size(), result.exponent);
    if (parsed.ec != std::errc{}) throw std::runtime_error("Invalid Treemap decimal exponent");
    for (const auto c : text.substr(0, separator)) if (c != '.') result.coefficient += c;
    if (precision && rounded_down_at_midpoint(value, static_cast<int>(precision) - 1 - result.exponent))
        ++result.coefficient.back();
    return result;
}

std::string place_decimal(const std::string& coefficient, int position) {
    if (position <= 0) return "0." + std::string(static_cast<std::size_t>(-position), '0') + coefficient;
    if (static_cast<std::size_t>(position) >= coefficient.size())
        return coefficient + std::string(static_cast<std::size_t>(position) - coefficient.size(), '0');
    return coefficient.substr(0, position) + '.' + coefficient.substr(position);
}
std::string exponential(const DecimalParts& parts) {
    std::string result(1, parts.coefficient.front());
    if (parts.coefficient.size() > 1) result += '.' + parts.coefficient.substr(1);
    return result + 'e' + (parts.exponent >= 0 ? "+" : "") + std::to_string(parts.exponent);
}
std::string fixed_value(double value, unsigned precision) {
    if (!std::isfinite(value)) return "Infinity";
    if (value >= 1e21) return *format_number(value);
    std::array<char, 128> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::fixed, static_cast<int>(precision));
    if (converted.ec != std::errc{}) throw std::runtime_error("Invalid Treemap fixed conversion");
    std::string result(buffer.data(), converted.ptr);
    if (rounded_down_at_midpoint(value, static_cast<int>(precision))) ++result.back();
    return result;
}
double rounded_integer(double value) {
    const auto integral = std::floor(value);
    return value - integral >= .5 ? integral + 1 : integral;
}
std::string integer_radix(double value, unsigned base, bool uppercase) {
    value = rounded_integer(value);
    if (!value) return "0";
    const auto bits = std::bit_cast<std::uint64_t>(value);
    auto significand = (bits & ((std::uint64_t{1} << 52) - 1)) | (std::uint64_t{1} << 52);
    const auto shift = static_cast<int>((bits >> 52) & 0x7ff) - 1075;
    if (shift < 0) significand >>= -shift;
    std::array<char, 64> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), significand, 2);
    std::string binary(buffer.data(), converted.ptr);
    if (shift > 0) binary.append(shift, '0');
    const auto group = base == 2 ? 1u : base == 8 ? 3u : 4u;
    const std::string_view digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string result;
    auto count = binary.size() % group;
    if (!count) count = group;
    for (std::size_t offset = 0; offset < binary.size(); count = group) {
        unsigned digit = 0;
        for (std::size_t i = 0; i < count; ++i) digit = digit * 2 + binary[offset++] - '0';
        result += digits[digit];
    }
    return result;
}

std::string numeric_value(double value, const ValueFormat& format, int& prefix_exponent) {
    const auto precision = *format.precision;
    switch (format.type) {
    case u'%': return fixed_value(value * 100, precision);
    case u'f': return fixed_value(value, precision);
    case u'e': return exponential(decimal_parts(value, precision + 1));
    case u'g': {
        const auto parts = decimal_parts(value, precision);
        return parts.exponent < -6 || parts.exponent >= static_cast<int>(precision)
            ? exponential(parts) : place_decimal(parts.coefficient, parts.exponent + 1);
    }
    case u'p': value *= 100; [[fallthrough]];
    case u'r': {
        if (!std::isfinite(value)) return "Infinity";
        const auto parts = decimal_parts(value, precision);
        return place_decimal(parts.coefficient, parts.exponent + 1);
    }
    case u's': {
        const auto parts = decimal_parts(value, precision);
        prefix_exponent = std::clamp(static_cast<int>(std::floor(parts.exponent / 3.0)), -8, 8) * 3;
        const auto position = parts.exponent - prefix_exponent + 1;
        if (position > 0) return place_decimal(parts.coefficient, position);
        const auto smaller = decimal_parts(value, static_cast<unsigned>(std::max(0,
            static_cast<int>(precision) + position - 1)));
        return place_decimal(smaller.coefficient, position);
    }
    case u'd': {
        value = rounded_integer(value);
        if (value < 1e21) return *format_number(value);
        const auto parts = decimal_parts(value);
        return place_decimal(parts.coefficient, parts.exponent + 1);
    }
    case u'b': return integer_radix(value, 2, false);
    case u'o': return integer_radix(value, 8, false);
    case u'x': return integer_radix(value, 16, false);
    case u'X': return integer_radix(value, 16, true);
    default: return *format_number(value); // c
    }
}

void trim_fraction(std::string& value) {
    const auto dot = value.find('.');
    if (dot == value.npos) return;
    const auto exponent = value.find('e');
    const auto end = exponent == value.npos ? value.size() : exponent;
    auto keep = end;
    while (keep > dot + 1 && value[keep - 1] == '0') --keep;
    if (keep == dot + 1) --keep;
    value.erase(keep, end - keep);
}
std::u16string group_digits(std::u16string_view value, std::optional<std::size_t> width = {}) {
    std::vector<std::u16string_view> groups;
    auto position = static_cast<std::ptrdiff_t>(value.size());
    std::size_t length = 0;
    while (position > 0) {
        std::size_t count = 3;
        if (width && length + count + 1 > *width) count = *width > length ? *width - length : 1;
        const auto start = std::max<std::ptrdiff_t>(0, position - static_cast<std::ptrdiff_t>(count));
        groups.push_back(value.substr(start, position - start));
        position -= static_cast<std::ptrdiff_t>(count);
        length += count + 1;
        if (width && length > *width) break;
    }
    std::u16string result;
    for (auto i = groups.rbegin(); i != groups.rend(); ++i) {
        if (!result.empty()) result += u',';
        result += *i;
    }
    return result;
}

std::u16string present_value(double value, const ValueFormat& format) {
    int prefix_exponent = 0;
    auto number = numeric_value(value, format, prefix_exponent);
    if (format.trim) trim_fraction(number);
    std::u16string prefix, suffix;
    if (format.symbol == u'$') prefix = u"$";
    else if (format.symbol == u'#' && std::u16string_view{u"boxX"}.find(format.type) != std::u16string_view::npos) {
        prefix = u"0"; prefix += format.type == u'X' ? u'x' : format.type;
    }
    if (format.type == u'%' || format.type == u'p') suffix = u"%";
    std::u16string integral;
    if (format.type == u'c') suffix.insert(suffix.begin(), number.begin(), number.end());
    else {
        if (format.sign == u'+' || format.sign == u' ') prefix.insert(prefix.begin(), format.sign);
        if (format.type == u's') {
            constexpr std::u16string_view prefixes[] = {u"y",u"z",u"a",u"f",u"p",u"n",u"µ",u"m",u"",u"k",u"M",u"G",u"T",u"P",u"E",u"Z",u"Y"};
            suffix.insert(0, prefixes[8 + prefix_exponent / 3]);
        }
        auto split = number.size();
        if (std::u16string_view{u"defgprs%"}.find(format.type) != std::u16string_view::npos)
            split = number.find_first_not_of("0123456789");
        if (split == number.npos) split = number.size();
        integral.assign(number.begin(), number.begin() + split);
        suffix.insert(suffix.begin(), number.begin() + split, number.end());
    }
    if (format.comma && !format.zero) integral = group_digits(integral);
    const auto length = prefix.size() + integral.size() + suffix.size();
    std::u16string padding(format.width > length ? format.width - length : 0, format.fill);
    if (format.comma && format.zero) {
        integral = group_digits(padding + integral, padding.empty() ? std::nullopt :
            std::optional<std::size_t>{format.width - suffix.size()});
        padding.clear();
    }
    switch (format.align) {
    case u'<': return prefix + integral + suffix + padding;
    case u'=': return prefix + padding + integral + suffix;
    case u'^': return padding.substr(0, padding.size() / 2) + prefix + integral + suffix + padding.substr(padding.size() / 2);
    default: return padding + prefix + integral + suffix;
    }
}
}

std::u16string format_treemap_value(double value, std::u16string_view specification) {
    if (value == 0) return {}; // Upstream leaves zero-value sections/cells unlabelled.
    if (!std::isfinite(value) || value < 0) throw std::runtime_error("Invalid Treemap value");
    std::u16string format{specification.empty() ? u"," : specification};
    bool currency = format.starts_with(u'$');
    if (currency) {
        if (format == u"$0,0") format = u",";
        else if (format.find(u',') != format.npos) {
            auto dot = format.find(u'.');
            while (dot != format.npos && (dot + 1 == format.size() || !digit(format[dot + 1])))
                dot = format.find(u'.', dot + 1);
            auto end = dot == format.npos ? format.size() : dot + 1;
            while (end < format.size() && digit(format[end])) ++end;
            format = u"," + (dot != format.npos && end > dot + 1 ? format.substr(dot, end - dot) : u"");
        } else format.erase(0, 1);
    }
    auto parsed = parse_format(format);
    if (!parsed) { parsed = parse_format(u","); currency = false; }
    auto result = present_value(value, *parsed);
    if (currency) result.insert(result.begin(), u'$');
    return result;
}
}
