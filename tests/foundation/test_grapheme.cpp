#include "support/folia_test.hpp"
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {
#include "fixtures/unicode/grapheme_break.inc"
}

suite unicode_grapheme_tests = [] {

"unicode grapheme boundaries match all pinned official Unicode 15.1 cases"_test = [] {
    for (const auto& test : grapheme_cases) {
        const std::span expected(test.boundaries.data(), test.boundary_count);
        std::vector<std::size_t> forward{0};
        auto cursor = std::size_t{0};
        while (cursor < test.text.size()) {
            const auto next = next_grapheme_boundary_char(test.text, cursor);
            expect(fatal(next > cursor && next <= test.text.size())) << test.source_line;
            forward.push_back(next);
            cursor = next;
        }
        expect(fatal(std::ranges::equal(forward, expected))) << test.source_line;
        expect(count_graphemes(test.text) + 1 == test.boundary_count) << test.source_line;
        std::vector<std::size_t> reverse{test.text.size()};
        while (cursor != 0) {
            const auto previous = prev_grapheme_boundary_char(test.text, cursor);
            expect(fatal(previous < cursor)) << test.source_line;
            reverse.push_back(previous);
            cursor = previous;
        }
        std::ranges::reverse(reverse);
        expect(fatal(std::ranges::equal(reverse, expected))) << test.source_line;
        for (std::size_t offset = 0; offset <= test.text.size(); ++offset) {
            const auto next = std::ranges::upper_bound(expected, offset);
            const auto prior = std::ranges::lower_bound(expected, offset);
            expect(next_grapheme_boundary_char(test.text, offset)
                == (next == expected.end() ? test.text.size() : *next)) << test.source_line << offset;
            expect(prev_grapheme_boundary_char(test.text, offset)
                == (prior == expected.begin() ? 0 : *std::prev(prior))) << test.source_line << offset;
            expect(is_grapheme_boundary_char(test.text, offset)
                == std::ranges::binary_search(expected, offset)) << test.source_line << offset;
        }
    }
};

"unicode grapheme count keeps consecutive flags paired and controls separate"_test = [] {
    const std::u32string flags(4097, U'\U0001f1e8');
    expect(count_graphemes(flags) == 2049u);
    expect(prev_grapheme_boundary_char(flags, flags.size()) == 4096u);
    expect(prev_grapheme_boundary_char(flags, 4096) == 4094u);
    expect(next_grapheme_boundary_char(flags, 4094) == 4096u);
    expect(next_grapheme_boundary_char(flags, 4096) == 4097u);
    expect(count_graphemes(U"\r\n\u0301a") == 3u);
    expect(count_graphemes(U"a\u200Db") == 2u);
    expect(next_grapheme_boundary_char(U"\u0301\u0308a", 0) == 2u);
    expect(next_grapheme_boundary_char(U"\u0301\u0308a", 1) == 2u);
    expect(count_graphemes(U"\U0001f469\U0001f3fd\u200D\U0001f4bb") == 1u);
};

};
