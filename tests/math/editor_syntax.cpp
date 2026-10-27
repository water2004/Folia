#include "support/folia_test.hpp"
import folia.math.editor.syntax;

using namespace boost::ut;
using namespace folia::math::editor;

namespace {
    bool Has(SyntaxAnalysis const& analysis, std::u32string_view source, TokenKind kind, std::u32string_view text) {
        return std::ranges::any_of(analysis.tokens, [&](auto const& token) {
            return token.kind == kind && source.substr(token.range.start, token.range.end - token.range.start) == text;
        });
    }
    suite editor_math_syntax = [] {
        "math editing syntax classifies commands groups numbers and text"_test = [] {
            SyntaxSession parser;
            std::u32string source = U"\\frac{a_1+2}{\\sqrt{x}}+\\text{mass}";
            auto const& analysis = parser.Update(source);
            expect(!analysis.hasErrors);
            expect(Has(analysis, source, TokenKind::Command, U"\\frac"));
            expect(Has(analysis, source, TokenKind::Command, U"\\sqrt"));
            expect(Has(analysis, source, TokenKind::Number, U"2"));
            expect(Has(analysis, source, TokenKind::Operator, U"_"));
            expect(Has(analysis, source, TokenKind::Text, U"mass"));
            expect(analysis.pairs.size() == 4_u);
            expect(analysis.unmatched.empty());
            auto match = analysis.MatchAt(source.find(U'{'));
            expect(match.has_value() && match->mate.has_value());
            if (match && match->mate) expect(source.substr(match->mate->start, 1) == U"}");
        };
        "math editing syntax retains incomplete commands and unclosed groups"_test = [] {
            SyntaxSession parser;
            for (std::u32string source : {U"\\frac{a+", U"\\sqrt{\\", U"a+(b", U"x_{1"}) {
                auto const& analysis = parser.Update(source);
                expect(analysis.hasErrors);
                expect(!analysis.tokens.empty());
                expect(!analysis.unmatched.empty());
                for (auto const& range : analysis.unmatched) {
                    auto match = analysis.MatchAt(range.start);
                    expect(match.has_value());
                    if (match) expect(!match->mate.has_value());
                }
            }
            auto const& partial = parser.Update(U"\\");
            expect(Has(partial, U"\\", TokenKind::Command, U"\\"));
            expect(parser.Update(U"\\frac{a}{b}").unmatched.empty());
        };
        "math editing syntax pairs sized delimiters and named environments"_test = [] {
            SyntaxSession parser;
            for (auto source : {U"\\left(x\\right]", U"\\left\\{x\\right.", U"\\bigl(x\\bigr)"}) {
                auto const& analysis = parser.Update(source);
                expect(analysis.unmatched.empty());
                expect(analysis.pairs.size() == 1_u);
            }
            std::u32string source = U"\\begin{aligned}a&=b\\\\c&=d\\end{aligned}";
            auto const& analysis = parser.Update(source);
            expect(!analysis.hasErrors);
            expect(Has(analysis, source, TokenKind::Environment, U"aligned"));
            auto match = analysis.MatchAt(0);
            expect(match.has_value() && match->mate.has_value());
            if (match && match->mate) expect(source.substr(match->mate->start, 4) == U"\\end");
            expect(parser.Update(U"\\begin{a}x\\end{b}").unmatched.size() == 2_u);
        };
        "math editing syntax excludes escaped braces and comments from grouping"_test = [] {
            SyntaxSession parser;
            std::u32string source = U"\\{a\\}+\\% % {[( not a group\nx+1";
            auto const& analysis = parser.Update(source);
            expect(analysis.pairs.empty());
            expect(analysis.unmatched.empty());
            expect(Has(analysis, source, TokenKind::Comment, U"% {[( not a group"));
            expect(Has(analysis, source, TokenKind::Command, U"\\%"));
        };
        "math editing syntax uses scalar source offsets and reuses unchanged trees"_test = [] {
            SyntaxSession parser;
            std::u32string source = U"😀α\\beta{中}";
            auto const& analysis = parser.Update(source);
            auto command = std::ranges::find(analysis.tokens, TokenKind::Command, &SyntaxToken::kind);
            expect(command != analysis.tokens.end());
            if (command != analysis.tokens.end()) expect(command->range == SourceRange{2, 7});
            expect(analysis.pairs.size() == 1_u);
            if (!analysis.pairs.empty()) expect(analysis.pairs.front().open == SourceRange{7, 8});
            for (std::size_t caret = 0; caret <= source.size(); ++caret) {
                parser.Update(source).MatchAt(caret);
                expect(parser.ParseCount() == 1_u);
            }
            parser.Update(U"😁α\\beta{中}");
            expect(parser.ParseCount() == 2_u);
        };
        "math editing incremental recovery agrees with fresh parsing after random source edits"_test = [] {
            SyntaxSession parser;
            std::mt19937 random(20261028);
            std::u32string alphabet = U"abα中😀\u0301\\{}()[]_^+%012\n";
            std::u32string source;
            for (std::size_t edit = 0; edit < 400; ++edit) {
                auto at = random() % (source.size() + 1);
                if (!source.empty() && random() % 3 == 0) source.erase(random() % source.size(), 1);
                else source.insert(source.begin() + at, alphabet[random() % alphabet.size()]);
                auto const& actual = parser.Update(source);
                SyntaxSession fresh;
                auto const& expected = fresh.Update(source);
                expect(actual.tokens == expected.tokens) << "tokens at edit " << edit;
                expect(actual.pairs == expected.pairs) << "pairs at edit " << edit;
                expect(actual.unmatched == expected.unmatched) << "unmatched at edit " << edit;
                expect(actual.hasErrors == expected.hasErrors) << "recovery at edit " << edit;
                for (auto const& token : actual.tokens)
                    expect(token.range.start < token.range.end && token.range.end <= source.size());
            }
        };
    };
}
