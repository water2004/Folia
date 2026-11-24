export module folia.test.fixture_data;

export import std;

// Expected data is read independently of production parsers: malformed/missing fixtures are errors, never empty suites.
export namespace folia::test::data {

inline std::filesystem::path path(std::string_view relative) {
#ifndef FOLIA_TEST_DATA_ROOT
#error "Test targets must define FOLIA_TEST_DATA_ROOT to the tests directory"
#endif
    const std::filesystem::path name{relative};
    if (name.empty() || name.has_root_path())
        throw std::invalid_argument("Fixture path must be relative: " + std::string(relative));
    for (const auto& part : name)
        if (part == "..") throw std::invalid_argument("Fixture path escapes test data root");
    return std::filesystem::path{FOLIA_TEST_DATA_ROOT} / name;
}

inline std::string read_text(std::string_view relative) {
    const auto file = path(relative);
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open fixture: " + file.string());
    std::string value{std::istreambuf_iterator<char>{input}, {}};
    if (input.bad()) throw std::runtime_error("Cannot read fixture: " + file.string());
    return value;
}

inline std::vector<std::vector<std::string>> read_tsv(
    std::string_view relative, std::size_t columns) {
    if (!columns) throw std::invalid_argument("A fixture table must have columns");
    std::istringstream input{read_text(relative)};
    std::vector<std::vector<std::string>> rows;
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        std::vector<std::string> cells;
        std::size_t begin{};
        for (;;) {
            const auto end = line.find('\t', begin);
            cells.emplace_back(line.substr(begin, end == std::string::npos ? end : end - begin));
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        if (cells.size() != columns)
            throw std::runtime_error(std::string(relative) + ':' +
                std::to_string(line_number) + ": expected " + std::to_string(columns) +
                " columns, found " + std::to_string(cells.size()));
        rows.push_back(std::move(cells));
    }
    if (rows.empty()) throw std::runtime_error("Fixture has no records: " + std::string(relative));
    return rows;
}

namespace detail {
inline std::uint32_t hex(std::string_view text, std::size_t& cursor, std::size_t digits) {
    if (text.size() - cursor < digits) throw std::runtime_error("Truncated fixture escape");
    std::uint32_t result{};
    for (std::size_t index{}; index < digits; ++index) {
        const auto c = text[cursor++];
        const auto value = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
            c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (value < 0) throw std::runtime_error("Invalid hexadecimal fixture escape");
        result = (result << 4) | static_cast<std::uint32_t>(value);
    }
    return result;
}

inline std::uint32_t escaped(std::string_view text, std::size_t& cursor) {
    if (cursor == text.size()) throw std::runtime_error("Truncated fixture escape");
    switch (text[cursor++]) {
    case '\\': return '\\';
    case '"': return '"';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case '0': return 0;
    case 'b': return '\b';
    case 'f': return '\f';
    case 'x': return hex(text, cursor, 2);
    case 'u': return hex(text, cursor, 4);
    case 'U': return hex(text, cursor, 8);
    default: throw std::runtime_error("Unknown fixture escape");
    }
}

inline std::uint32_t utf8(std::string_view text, std::size_t& cursor) {
    const auto first = static_cast<unsigned char>(text[cursor++]);
    if (first < 0x80) return first;
    const auto count = first >= 0xc2 && first <= 0xdf ? 1 :
        first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
    if (!count || text.size() - cursor < static_cast<std::size_t>(count))
        throw std::runtime_error("Invalid UTF-8 in fixture");
    std::uint32_t value = first & ((1u << (6 - count)) - 1);
    for (int i{}; i < count; ++i) {
        const auto next = static_cast<unsigned char>(text[cursor++]);
        if ((next & 0xc0) != 0x80) throw std::runtime_error("Invalid UTF-8 continuation in fixture");
        value = (value << 6) | (next & 0x3f);
    }
    const std::uint32_t minimum[] = {0, 0x80, 0x800, 0x10000};
    if (value < minimum[count] || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        throw std::runtime_error("Invalid UTF-8 scalar in fixture");
    return value;
}
} // namespace detail

inline std::string decode_bytes(std::string_view text) {
    std::string result;
    for (std::size_t cursor{}; cursor < text.size();) {
        auto value = static_cast<unsigned char>(text[cursor++]);
        if (value == '\\') {
            const auto decoded = detail::escaped(text, cursor);
            if (decoded > 0xff) throw std::runtime_error("Non-byte fixture escape");
            value = static_cast<unsigned char>(decoded);
        }
        result.push_back(static_cast<char>(value));
    }
    return result;
}

// Escaped UTF-16 units preserve isolated surrogates in upstream JavaScript
// string fixtures. Literal UTF-8 is decoded strictly and yields valid pairs.
inline std::u16string decode_utf16(std::string_view text) {
    std::u16string result;
    for (std::size_t cursor{}; cursor < text.size();) {
        const auto escaped = text[cursor] == '\\';
        if (escaped) ++cursor;
        auto value = escaped ? detail::escaped(text, cursor) : detail::utf8(text, cursor);
        if (value > 0x10ffff) throw std::runtime_error("Fixture scalar exceeds Unicode range");
        if (value <= 0xffff) result.push_back(static_cast<char16_t>(value));
        else {
            value -= 0x10000;
            result.push_back(static_cast<char16_t>(0xd800 + (value >> 10)));
            result.push_back(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
        }
    }
    return result;
}

inline std::u32string decode_utf32(std::string_view text) {
    const auto units = decode_utf16(text);
    std::u32string result;
    for (std::size_t i{}; i < units.size(); ++i) {
        std::uint32_t value = units[i];
        if (value >= 0xd800 && value <= 0xdbff) {
            if (++i == units.size() || units[i] < 0xdc00 || units[i] > 0xdfff)
                throw std::runtime_error("Unpaired surrogate in scalar fixture");
            value = 0x10000 + ((value - 0xd800) << 10) + (units[i] - 0xdc00);
        } else if (value >= 0xdc00 && value <= 0xdfff) {
            throw std::runtime_error("Unpaired surrogate in scalar fixture");
        }
        result.push_back(static_cast<char32_t>(value));
    }
    return result;
}

} // namespace folia::test::data
