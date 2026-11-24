#include "support/folia_test.hpp"
import folia.mermaid.treemap;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "mermaid_cpp/support/theme_fixture.hpp"

suite treemap_regressions = [] {
    "mermaid_treemap_numeric_formats_match_locked_d3"_test = [] {
        const auto override_dir = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(
            override_dir ? override_dir : FOLIA_MERMAID_FIXTURE_DIR) / "treemap-values.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto first = line.find('\t');
            const auto second = line.find('\t', first + 1);
            expect(fatal(first != line.npos && second != line.npos));
            double value = 0;
            const auto parsed = std::from_chars(line.data(), line.data() + first, value);
            expect(fatal(parsed.ec == std::errc{} && parsed.ptr == line.data() + first));
            const auto format = decode(line.substr(first + 1, second - first - 1));
            const auto expected = decode(line.substr(second + 1));
            const auto actual = format_treemap_value(value, format);
            expect(bool(actual == expected)) << line << " actual UTF-16 " << hex(actual);
            ++count;
        }
        expect(count == 1794_u);
    };
};
}
