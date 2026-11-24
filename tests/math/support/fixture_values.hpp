#pragma once

#include "support/fixture.hpp"

namespace math_test::fixtures {

template <class T>
T integer(std::string_view value, int base = 10) {
    T result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, base);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("Invalid integer in math fixture: " + std::string{value});
    return result;
}

inline std::uint64_t binary64(std::string_view value) {
    if (value.size() != 16)
        throw std::runtime_error("Math binary64 fixture must contain exactly 16 hex digits");
    return integer<std::uint64_t>(value, 16);
}

inline bool boolean(std::string_view value) {
    if (value != "0" && value != "1")
        throw std::runtime_error("Math boolean fixture must be 0 or 1");
    return value == "1";
}

// Byte columns use printable ASCII and \xHH escapes. Keep them distinct from
// UTF-16 columns: invalid UTF-8, NULs and lone surrogates are deliberate inputs.
inline std::string bytes(std::string_view value) {
    return folia::test::data::decode_bytes(value);
}

inline auto rows(std::string_view path, std::size_t columns, std::size_t expected) {
    auto result = folia::test::data::read_tsv(path, columns);
    if (result.size() != expected)
        throw std::runtime_error("Incomplete math fixture " + std::string{path} +
                                 ": expected " + std::to_string(expected) +
                                 " rows, found " + std::to_string(result.size()));
    return result;
}

} // namespace math_test::fixtures
