#include "support/folia_test.hpp"

import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

suite numbers = [] {
    "mermaid_numbers_match_ecmascript_binary64_oracle"_test = [] {
        const auto fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{
            fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "number.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto tab = line.find('\t');
            expect(fatal(tab == 16_u));
            std::uint64_t bits{};
            const auto parsed = std::from_chars(line.data(), line.data() + tab, bits, 16);
            expect(fatal(parsed.ec == std::errc{} && parsed.ptr == line.data() + tab));
            const auto actual = format_number(std::bit_cast<double>(bits));
            const std::string expected = line.substr(tab + 1);
            expect(actual.has_value() == (expected != "-")) << line;
            if (actual) expect(*actual == expected) << line;
            ++count;
        }
        expect(count > 9000_u) << "all finite, non-finite and notation boundaries must execute";
    };
};
}
