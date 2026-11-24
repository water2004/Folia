#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

// Both TeX arguments and MML attributes have a null/bool/double/text variant.
// Decode the observed tag explicitly; no production parser derives expectations.
template <class Variant>
Variant primitiveValue(std::string_view type, std::string_view value) {
    if (type == "null") {
        if (!value.empty()) throw std::runtime_error("Null fixture primitive carries a value");
        return std::variant_alternative_t<0, Variant>{};
    }
    if (type == "boolean") return boolean(value);
    if (type == "number") return std::bit_cast<double>(binary64(value));
    if (type == "string") return folia::test::data::decode_utf16(value);
    throw std::runtime_error("Unknown fixture primitive type: " + std::string{type});
}

template <class Variant>
std::vector<std::vector<Variant>> bindingArguments(
    std::string_view file, std::size_t count, std::size_t bindings) {
    std::vector<std::vector<Variant>> result(bindings);
    std::size_t previous{};
    for (const auto& r : rows(file, 4, count)) {
        const auto binding = integer<std::size_t>(r[0]);
        if (binding < previous || binding >= result.size() ||
            integer<std::size_t>(r[1]) != result[binding].size())
            throw std::runtime_error("Fixture binding argument order changed");
        result[binding].push_back(primitiveValue<Variant>(r[2], r[3]));
        previous = binding;
    }
    return result;
}

template <class Entry>
std::vector<std::vector<Entry>> bindingAttributes(
    std::string_view file, std::size_t count, std::size_t bindings) {
    std::vector<std::vector<Entry>> result(bindings);
    std::size_t previous{};
    for (const auto& r : rows(file, 5, count)) {
        const auto binding = integer<std::size_t>(r[0]);
        if (binding < previous || binding >= result.size() ||
            integer<std::size_t>(r[1]) != result[binding].size())
            throw std::runtime_error("Fixture binding attribute order changed");
        result[binding].emplace_back(folia::test::data::decode_utf16(r[2]),
                                     primitiveValue<typename Entry::second_type>(r[3], r[4]));
        previous = binding;
    }
    return result;
}

} // namespace math_test::fixtures
