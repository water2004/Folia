module folia.mermaid.number;
import std;

namespace folia::mermaid {

std::optional<std::string> format_number(double value) {
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    if (value == 0) {
        return "0";
    }

    // to_chars supplies the shortest round-trippable decimal digits. Reformat
    // their decimal-point position with ECMAScript Number::toString's fixed
    // interval [1e-6, 1e21), exponent sign, and absence of exponent zero padding.
    std::array<char, 128> buffer{};
    auto const converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
    if (converted.ec != std::errc{}) {
        return std::nullopt;
    }
    std::string_view input(buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data()));
    bool const negative = input.front() == '-';
    if (negative) {
        input.remove_prefix(1);
    }

    auto const exponent_position = input.find_first_of("eE");
    auto const mantissa = input.substr(0, exponent_position);
    int exponent{};
    if (exponent_position != std::string_view::npos) {
        auto exponent_text = input.substr(exponent_position + 1);
        if (!exponent_text.empty() && exponent_text.front() == '+') {
            exponent_text.remove_prefix(1);
        }
        auto const parsed = std::from_chars(
            exponent_text.data(), exponent_text.data() + exponent_text.size(), exponent);
        if (parsed.ec != std::errc{} || parsed.ptr != exponent_text.data() + exponent_text.size()) {
            return std::nullopt;
        }
    }
    auto const point = mantissa.find('.');
    auto decimal_position = static_cast<int>(
        point == std::string_view::npos ? mantissa.size() : point) + exponent;
    std::string digits;
    for (auto const character : mantissa) {
        if (character != '.') {
            digits.push_back(character);
        }
    }
    while (digits.size() > 1 && digits.front() == '0') {
        digits.erase(digits.begin());
        --decimal_position;
    }
    while (digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
    }

    std::string result = negative ? "-" : "";
    if (decimal_position > 0 && decimal_position <= 21) {
        auto const position = static_cast<std::size_t>(decimal_position);
        if (position >= digits.size()) {
            result += digits;
            result.append(position - digits.size(), '0');
        } else {
            result.append(digits, 0, position);
            result.push_back('.');
            result.append(digits, position);
        }
    } else if (decimal_position <= 0 && decimal_position > -6) {
        result += "0.";
        result.append(static_cast<std::size_t>(-decimal_position), '0');
        result += digits;
    } else {
        result.push_back(digits.front());
        if (digits.size() > 1) {
            result.push_back('.');
            result.append(digits, 1);
        }
        auto const scientific_exponent = decimal_position - 1;
        result.push_back('e');
        if (scientific_exponent >= 0) {
            result.push_back('+');
        }
        result += std::to_string(scientific_exponent);
    }
    return result;
}


}
