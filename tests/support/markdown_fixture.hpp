#pragma once
#include "support/fixture.hpp"

namespace folia::test::conformance {
struct MarkdownCase {
    int example;
    std::string section;
    std::string markdown;
    std::string html;
};

inline std::vector<MarkdownCase> markdown_fixture(
    std::string_view file, std::size_t expected_count) {
    const auto rows = data::read_tsv(file, 4);
    if (rows.size() != expected_count)
        throw std::runtime_error("Incomplete Markdown fixture: " + std::string(file));
    std::vector<MarkdownCase> cases;
    std::unordered_set<int> identifiers;
    for (const auto& row : rows) {
        int id{};
        const auto [end, error] = std::from_chars(row[0].data(), row[0].data() + row[0].size(), id);
        if (error != std::errc{} || end != row[0].data() + row[0].size()
            || id <= 0 || !identifiers.insert(id).second)
            throw std::runtime_error("Invalid/duplicate Markdown case ID: " + row[0]);
        cases.push_back({id, data::decode_bytes(row[1]), data::decode_bytes(row[2]), data::decode_bytes(row[3])});
    }
    return cases;
}
} // namespace folia::test::conformance
