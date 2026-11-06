module folia.mermaid.html_named_reference;
import std;

namespace folia::mermaid {
namespace {
struct Reference {
    std::string_view spelling;
    char32_t first;
    char32_t second;
};
constexpr Reference references[]{
#include "../markdown/support/html_entity_data.inc"
};

void append(std::u16string& result, char32_t scalar) {
    if (scalar < 0x10000) {
        result += static_cast<char16_t>(scalar);
    } else {
        scalar -= 0x10000;
        result += static_cast<char16_t>(0xD800 + (scalar >> 10));
        result += static_cast<char16_t>(0xDC00 + (scalar & 0x3FF));
    }
}
}

std::optional<std::u16string> mermaid_html_named_reference(std::u16string_view name) {
    if (name.empty() || name.size() > 32) return std::nullopt;
    std::array<char, 34> spelling{};
    spelling.front() = '&';
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (name[i] > 0x7F) return std::nullopt;
        spelling[i + 1] = static_cast<char>(name[i]);
    }
    spelling[name.size() + 1] = ';';
    const std::string_view key{spelling.data(), name.size() + 2};
    const auto match = std::ranges::lower_bound(references, key, {}, &Reference::spelling);
    if (match == std::end(references) || match->spelling != key) return std::nullopt;
    std::u16string result;
    append(result, match->first);
    if (match->second) append(result, match->second);
    return result;
}
}
