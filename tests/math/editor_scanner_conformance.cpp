#include "support/folia_test.hpp"
import folia.math.editor.syntax;
import folia.math.input.tex.tex_scanner;

using namespace boost::ut;

namespace {
    suite editor_scanner_conformance = [] {
        "math editing command boundaries agree with MathJax TexScanner"_test = [] {
            using namespace folia::math;
            for (std::u32string source : {U"\\alpha beta", U"\\frac{a}{b}", U"\\foo123", U"\\foo*",
                    U"\\%text", U"\\{a", U"\\\\x", U"\\ ", U"\\😀x", U"\\中x", U"\\left(", U"\\text{abc}"}) {
                std::u16string utf16;
                for (char32_t c : source) {
                    if (c < 0x10000) utf16.push_back(static_cast<char16_t>(c));
                    else { c -= 0x10000; utf16.push_back(static_cast<char16_t>(0xD800 + (c >> 10)));
                        utf16.push_back(static_cast<char16_t>(0xDC00 + (c & 1023))); }
                }
                tex::TexScanner scanner(utf16);
                scanner.setPosition(1);
                auto command = scanner.getCS();
                editor::SyntaxSession parser;
                auto const& analysis = parser.Update(source);
                auto found = std::ranges::find(analysis.tokens, editor::TokenKind::Command, &editor::SyntaxToken::kind);
                expect(found != analysis.tokens.end());
                if (found == analysis.tokens.end()) continue;
                expect(found->range.start == 0_u);
                auto utf16Length = command.size() + 1;
                // The scanner optionally consumes one trailing space after a
                // control word; that space is trivia, not part of its name.
                std::size_t scalarLength = 1;
                for (std::size_t i = 1; i < utf16Length; ++i) {
                    if (utf16[i] >= 0xD800 && utf16[i] <= 0xDBFF && i + 1 < utf16Length) ++i;
                    ++scalarLength;
                }
                expect(found->range.end == scalarLength);
            }
        };
    };
}
