#include "support/folia_test.hpp"
#include "math/support/text_fixtures.hpp"
import folia.math.input.tex.tex_error;
using namespace boost::ut;
using folia::math::tex::TexError;
namespace {
suite tex_error_source_cases = [] {
  "tex_error_matches_pinned_source_format_cases"_test = [] {
    const auto& cases = math_test::fixtures::focusedErrorCases();
    for (const auto& c : cases) {
      TexError error("SourceErrorId", c.format, std::span<std::u16string const>(c.args));
      expect(error.id() == "SourceErrorId");
      expect(error.message() == c.expected);
    }
  };
};
}

namespace {
suite tex_error_complete_original_format_corpus = [] {
    "tex_error_preserves_all_1981_original_source_format_messages"_test = [] {
        // The native API receives normalized text arguments. Source Array.join
        // normalizes null/undefined to empty, -0 to 0, arrays recursively.
        const auto& arguments = math_test::fixtures::exhaustiveErrorArguments();
        const auto& exhaustiveFormatCases = math_test::fixtures::exhaustiveErrorCases();
        expect(std::size(exhaustiveFormatCases) == 1981_u);
        std::size_t index = 0;
        for (auto const& row : exhaustiveFormatCases) {
            TexError error("ID", row.format, arguments);
            expect(static_cast<bool>(error.message() == row.expected)) << index;
            ++index;
        }
    };
};
}
