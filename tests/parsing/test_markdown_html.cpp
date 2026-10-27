#include <optional>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"

import folia.markdown.markdown_html;
import folia.markdown.text_edit;

using namespace boost::ut;
using namespace folia;

suite markdown_html_scanner_tests = [] {
    "scans_open_tag_with_absolute_attribute_ranges"_test = [] {
        const std::u32string source = U"prefix <DIV foo=\"bar\" disabled/> tail";
        const auto tag = scan_markdown_html_tag(source, 7, source.size());
        expect(fatal(tag.has_value()));
        expect(tag->range.start == 7_u);
        expect(tag->range.end == 32_u);
        expect(tag->name == "div");
        expect(!tag->closing);
        expect(tag->self_closing);
        expect(tag->attributes.size() == 2_u);
        expect(tag->attributes[0].name == SourceRange{12, 15});
        expect(tag->attributes[0].value == SourceRange{17, 20});
        expect(tag->attributes[1].name == SourceRange{22, 30});
        expect(!tag->attributes[1].value.has_value());
        expect(source.substr(
            tag->attributes[0].value->start,
            tag->attributes[0].value->length()) == U"bar");
    };

    "scans_closing_tag_and_lowercases_only_tag_name"_test = [] {
        const std::u32string source = U"</SVG\t\r\n>";
        const auto tag = scan_markdown_html_tag(source, 0, source.size());
        expect(fatal(tag.has_value()));
        expect(tag->range == SourceRange{0, source.size()});
        expect(tag->name == "svg");
        expect(tag->closing);
        expect(!tag->self_closing);
        expect(tag->attributes.empty());
    };

    "allows_one_line_ending_per_separator_and_multiline_quoted_values"_test = [] {
        const std::u32string source = U"<A\r\n id=\"one\r\ntwo\nthree\" disabled \r\n/>";
        const auto tag = scan_markdown_html_tag(source, 0, source.size());
        expect(fatal(tag.has_value()));
        expect(tag->name == "a");
        expect(tag->self_closing);
        expect(tag->attributes.size() == 2_u);
        expect(tag->attributes[0].value.has_value());
        const auto value = *tag->attributes[0].value;
        expect(source.substr(value.start, value.length()) == U"one\r\ntwo\nthree");
        expect(!tag->attributes[1].value.has_value());
    };

    "keeps_backslash_in_quoted_value_literal"_test = [] {
        const std::u32string source = U"<a value=\"one\\\"/>";
        const auto tag = scan_markdown_html_tag(source, 0, source.size());
        expect(fatal(tag.has_value()));
        expect(tag->self_closing);
        expect(tag->attributes.size() == 1_u);
        const auto value = *tag->attributes[0].value;
        expect(source.substr(value.start, value.length()) == U"one\\");
    };

    "preserves_raw_attribute_ranges_without_decoding"_test = [] {
        const std::u32string source = U"<a _x:2=foo\\&amp;>";
        const auto tag = scan_markdown_html_tag(source, 0, source.size());
        expect(fatal(tag.has_value()));
        expect(tag->attributes.size() == 1_u);
        const auto attr_name = tag->attributes[0].name;
        const auto attr_value = *tag->attributes[0].value;
        expect(source.substr(attr_name.start, attr_name.length()) == U"_x:2");
        expect(source.substr(attr_value.start, attr_value.length()) == U"foo\\&amp;");
    };

    "rejects_illegal_names_values_and_missing_separators"_test = [] {
        for (const auto source : {
                 std::u32string{U"<33>"},
                 std::u32string{U"<_a>"},
                 std::u32string{U"<é>"},
                 std::u32string{U"<a h*#ref=hi>"},
                 std::u32string{U"<a x=>"},
                 std::u32string{U"<a x=hi'>"},
                 std::u32string{U"<a x=\"unterminated>"},
                 std::u32string{U"<a href='bar'title=title>"},
                 std::u32string{U"< a>"},
                 std::u32string{U"<bar/ >"},
                 std::u32string{U"</a href=\"foo\">"},
             }) {
            expect(!scan_markdown_html_tag(source, 0, source.size()).has_value());
        }
    };

    "rejects_multiple_line_endings_in_a_whitespace_group"_test = [] {
        for (const auto source : {
                 std::u32string{U"<a\n\n x>"},
                 std::u32string{U"<a x=v\n \n>"},
                 std::u32string{U"<a x \r\n \ny=v>"},
             }) {
            expect(!scan_markdown_html_tag(source, 0, source.size()).has_value());
        }
        const std::u32string quoted = U"<a x=\"first\n\nsecond\">";
        expect(scan_markdown_html_tag(quoted, 0, quoted.size()).has_value());
    };

    "rejects_non_tag_markup_kinds"_test = [] {
        for (const auto source : {
                 std::u32string{U"<!-- comment -->"},
                 std::u32string{U"<!DOCTYPE html>"},
                 std::u32string{U"<?xml version='1.0'?>"},
                 std::u32string{U"<![CDATA[data]]>"},
             }) {
            expect(!scan_markdown_html_tag(source, 0, source.size()).has_value());
        }
    };

    "respects_start_and_exclusive_limit_without_reading_past_it"_test = [] {
        const std::u32string source = U"x<a title='v'>tail";
        const auto tag = scan_markdown_html_tag(source, 1, 14);
        expect(fatal(tag.has_value()));
        expect(tag->range == SourceRange{1, 14});
        expect(!scan_markdown_html_tag(source, 1, 13).has_value());
        expect(!scan_markdown_html_tag(source, source.size(), source.size()).has_value());
        expect(!scan_markdown_html_tag(source, 2, 1).has_value());
    };
};
