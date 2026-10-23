#pragma once
// Shared theme-oracle fixture codec; independent of native theme implementation.
std::u16string decode(std::string_view text) {
    if (text == "-") return {};
    if (text.size() % 4) throw std::runtime_error("Invalid UTF-16 fixture");
    std::u16string result;
    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned c{};
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, c, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 4) throw std::runtime_error("Invalid hex");
        result += static_cast<char16_t>(c);
    }
    return result;
}
std::string hex(std::u16string_view text) {
    if (text.empty()) return "-";
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto c : text) for (int shift = 12; shift >= 0; shift -= 4) result += digits[(c >> shift) & 15];
    return result;
}
std::string snapshot(const ConfigValue& value) {
    switch (value.kind) {
    case ConfigKind::null: return "N";
    case ConfigKind::boolean: return value.boolean ? "T" : "F";
    case ConfigKind::number:
        if (std::isnan(value.number)) return "DNaN;";
        if (std::isinf(value.number)) return value.number < 0 ? "D-Infinity;" : "DInfinity;";
        if (value.number == 0 && std::signbit(value.number)) return "D-0;";
        return "D" + *format_number(value.number) + ";";
    case ConfigKind::string: return "S" + hex(value.string) + ";";
    case ConfigKind::sequence: {
        std::string result = "[";
        for (const auto& item : value.sequence) result += snapshot(item);
        return result + "]";
    }
    case ConfigKind::mapping: {
        std::string result = "{";
        for (const auto& [key, item] : value.mapping) result += "S" + hex(key) + ";" + snapshot(item);
        return result + "}";
    }
    }
    throw std::runtime_error("Invalid config kind");
}
ConfigValue json(std::u16string_view source) {
    // Bare directive words (null/false/numbers) intentionally remain strings.
    // Wrap the fixture's JSON value in an array to enter the strict JSON branch.
    auto parsed = detect_directives(u"%%{init:[" + std::u16string{source} + u"]}%%");
    if (!parsed || parsed->size() != 1 || !parsed->front().type) throw std::runtime_error("Invalid fixture JSON");
    if (parsed->front().args.kind != ConfigKind::sequence || parsed->front().args.sequence.size() != 1)
        throw std::runtime_error("Invalid wrapped fixture JSON");
    return std::move(parsed->front().args.sequence.front());
}
const ConfigValue& field(const ConfigValue& value, std::u16string_view key) {
    const auto found = value.find(key);
    if (!found) throw std::runtime_error("Missing fixture property");
    return *found;
}
