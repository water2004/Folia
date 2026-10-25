// XML attribute ordering/escaping follows the behavior of MathJax 4.1.3
// ts/adaptors/lite/Parser.ts at fb987178c2d279a99b0db5ea02e751691703955e.
// Copyright (c) 2018-2026 The MathJax Consortium; Apache-2.0. See README.md.
module folia.math.output.svg.writer;
import std;

namespace folia::math::output::svg::detail {
using Attribute = folia::math::output::scene::SemanticAttribute;

std::string escape_text(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        default: result += ch; break;
        }
    }
    return result;
}

bool is_event_attribute(std::string_view name) noexcept {
    if (name.size() < 2 || name[0] != 'o' || name[1] != 'n') return false;
    return std::ranges::all_of(name, [](unsigned char c) {
        return c >= 'a' && c <= 'z';
    });
}

void append_attribute(std::string& out, std::string_view name,
    std::string_view value) {
    out += ' ';
    out += name;
    out += "=\"";
    // Append plain spans directly, especially the long Source glyph paths.
    // Escaping never requires a second complete copy of an attribute value.
    while (!value.empty()) {
        const auto next = value.find_first_of("&\"<>");
        if (next == std::string_view::npos) {
            out.append(value);
            break;
        }
        out.append(value.substr(0, next));
        switch (value[next]) {
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        }
        value.remove_prefix(next + 1);
    }
    out += '"';
}

void append_metadata(std::string& out,
    folia::math::output::scene::SemanticMetadata const& metadata,
    std::span<std::string_view const> reserved) {
    auto append_if_allowed = [&](Attribute const& attribute) {
        if (attribute.name.empty() || is_event_attribute(attribute.name)) return;
        if (std::ranges::find(reserved, std::string_view(attribute.name)) != reserved.end()) return;
        append_attribute(out, attribute.name, attribute.value);
    };
    if (!metadata.ordered_attributes.empty()) {
        for (auto const& attribute : metadata.ordered_attributes) append_if_allowed(attribute);
    }
    if (!metadata.math_role.empty() &&
        std::ranges::find(reserved, std::string_view{"data-math-role"}) == reserved.end())
        append_attribute(out, "data-math-role", metadata.math_role);
}
}
