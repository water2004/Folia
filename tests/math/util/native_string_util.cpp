// Focused native checks against the Source-mapped MathJax string utilities.
#include "support/folia_test.hpp"
import folia.math.util.string;

namespace {
using namespace boost::ut;
namespace strings = folia::math::string_util;

bool same16(std::u16string_view a, std::u16string_view b) {
    return a.compare(b) == 0;
}

bool rangeError(std::function<void()> const &call, std::string_view message) {
    try {
        call();
    } catch (std::range_error const &error) {
        return error.what() == message;
    } catch (...) {
    }
    return false;
}
} // namespace

suite native_string_util = [] {
    "sortLength_and_quotePattern_match_source_order_and_escaping"_test = [] {
        expect(strings::sortLength(u"bbb", u"aa") == -1);
        expect(strings::sortLength(u"a", u"b") == -1);
        expect(strings::sortLength(u"😀", u"a") == -1); // JS string length is UTF-16 units.
        expect(same16(strings::quotePattern(u"^$(){}.+*?-|[]:\\"),
                      u"\\^\\$\\(\\)\\{\\}\\.\\+\\*\\?\\-\\|\\[\\]\\:\\\\"));
    };
    "unicode_conversion_preserves_astral_and_lone_surrogates"_test = [] {
        const std::u16string input{u'A', 0xD83D, 0xDE00, u'x', 0xD800, u'y', 0xDC00};
        const auto points = strings::unicodeChars(input);
        expect(points == std::vector<double>{65, 0x1F600, 120, 0xD800, 121, 0xDC00});
        const std::array<double, 5> outputPoints{65, 0x1F600, 0xD800, 0xDC00, 0x10FFFF};
        const std::u16string expected{u'A', 0xD83D, 0xDE00, 0xD800, 0xDC00, 0xDBFF, 0xDFFF};
        expect(same16(strings::unicodeString(outputPoints), expected));
        for (const auto [value, message] :
             std::array{std::pair{-1.0, std::string_view("Invalid code point -1")},
                        std::pair{1114112.0, std::string_view("Invalid code point 1114112")},
                        std::pair{1.5, std::string_view("Invalid code point 1.5")},
                        std::pair{std::numeric_limits<double>::quiet_NaN(),
                                  std::string_view("Invalid code point NaN")},
                        std::pair{std::numeric_limits<double>::infinity(),
                                  std::string_view("Invalid code point Infinity")}}) {
            expect(rangeError(
                [&] {
                    const std::array<double, 1> one{value};
                    (void)strings::unicodeString(one);
                },
                message));
        }
    };
    "percent_and_split_use_ecmascript_whitespace"_test = [] {
        expect(strings::isPercent(u"50%\u00A0"));
        expect(strings::isPercent(u"% "));
        expect(!strings::isPercent(u"50% x"));
        expect(std::ranges::equal(strings::split(u" \u00A0alpha\u2003beta\uFEFF "),
                                  std::vector<std::u16string>{u"alpha", u"beta"}));
        expect(std::ranges::equal(strings::split(u""), std::vector<std::u16string>{u""}));
        expect(
            std::ranges::equal(strings::split(u"\u3000\u2028"), std::vector<std::u16string>{u""}));
    };
    "replaceUnicode_matches_source_escape_and_exception_edges"_test = [] {
        expect(same16(strings::replaceUnicode(u"x\\U0041/\\U{ 1F600 }/\\\\z"), u"xA/😀/\\z"));
        expect(same16(strings::replaceUnicode(u"\\U{1}"), u"\u0001"));
        expect(same16(strings::replaceUnicode(u"x\\\ny"), u"x\\\ny"));
        expect(rangeError([] { (void)strings::replaceUnicode(u"\\a"); }, "Invalid code point NaN"));
        expect(
            rangeError([] { (void)strings::replaceUnicode(u"\\Uq"); }, "Invalid code point NaN"));
        expect(rangeError([] { (void)strings::replaceUnicode(u"\\U{1234567}"); },
                          "Invalid code point NaN"));
    };
    "toEntity_uses_uppercase_codepoint_and_source_empty_error"_test = [] {
        expect(same16(strings::toEntity(u"a"), u"&#x61;"));
        expect(same16(strings::toEntity(u"😀"), u"&#x1F600;"));
        expect(same16(strings::toEntity(std::u16string{0xD800}), u"&#xD800;"));
        bool matched = false;
        try {
            (void)strings::toEntity(u"");
        } catch (std::invalid_argument const &error) {
            matched = error.what() ==
                      std::string_view("Cannot read properties of undefined (reading 'toString')");
        }
        expect(matched);
    };
};
