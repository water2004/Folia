#include <string>
#include <string_view>
#include "support/folia_test.hpp"
import folia.markdown.utf;
import folia.markdown.link_label;
import folia.markdown.html_entities;

using namespace folia;
using namespace boost::ut;

suite commonmark_unicode_tests = [] {
    "CommonMark classifies Unicode punctuation symbols and space separators"_test = [] {
        expect(unicode_punctuation_or_symbol(U'£'));
        expect(unicode_punctuation_or_symbol(U'€'));
        expect(unicode_punctuation_or_symbol(U'😀'));
        expect(!unicode_punctuation_or_symbol(U'Ａ'));
        expect(!unicode_punctuation_or_symbol(U'φ'));
        expect(is_commonmark_whitespace(U'\u2003'));
        expect(is_commonmark_whitespace(U'\f'));
        expect(!is_commonmark_whitespace(U'\u200b'));
        expect(!is_commonmark_whitespace(U'\v'));
    };
    "Reference labels use full Unicode folding without modifying source"_test = [] {
        const std::u32string label = U" \tStraße\u2003ΦΩΣ\r\n";
        const auto before = label;
        expect(normalize_markdown_link_label(label) == "strasse φωσ");
        expect(label == before);
        expect(normalize_markdown_link_label(U"SS") == normalize_markdown_link_label(U"ß"));
        expect(normalize_markdown_link_label(U"x&amp;y") != normalize_markdown_link_label(U"x&y"));
    };
    "Character references decode exact names and numeric scalars"_test = [] {
        expect(decode_html_character_reference(U"&copy;") == std::u32string{U"©"});
        expect(decode_html_character_reference(U"&ngE;") == std::u32string{U"\u2267\u0338"});
        expect(decode_html_character_reference(U"&#0;") == std::u32string{U"\ufffd"});
        expect(decode_html_character_reference(U"&#x80;") == std::u32string{U"\u0080"});
        expect(!decode_html_character_reference(U"&madeUp;"));
        expect(!decode_html_character_reference(U"&#12345678;"));
    };
};
