#pragma once
#include "support/fixture.hpp"

namespace {

std::vector<std::u32string> editable_inline_cases() {
    std::vector<std::u32string> result;
    for (const auto& row : folia::test::data::read_tsv("editing/fixtures/inline_states.tsv", 2))
        result.push_back(folia::test::data::decode_utf32(row[1]));
    return result;
}

std::vector<std::string> editable_context_documents(std::string_view fixture, std::string_view text) {
    std::vector<std::string> result;
    for (const auto& row : folia::test::data::read_tsv(fixture, 3))
        result.push_back(folia::test::data::decode_bytes(row[1]) + std::string(text)
            + folia::test::data::decode_bytes(row[2]));
    return result;
}

} // namespace
