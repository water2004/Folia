#include <optional>
#include <string_view>

#include "support/folia_test.hpp"

import folia.markdown.markdown_html;
import folia.markdown.text_edit;

using namespace boost::ut;
using namespace folia;

namespace {

std::optional<MarkdownHtmlBlockStart> start(std::u32string_view source) {
    std::size_t end = 0;
    while (end < source.size() && source[end] != U'\r' && source[end] != U'\n') ++end;
    return scan_markdown_html_block_start(source, 0, end);
}

} // namespace

suite markdown_html_block_scanner_tests = [] {
    "official_type6_examples_recognize_indentation_closers_and_malformed_tails"_test = [] {
        for (const auto source : {
                 std::u32string_view{U" <div>"},       // CommonMark #150
                 std::u32string_view{U"</div>"},       // #151
                 std::u32string_view{U"<div id=\"foo\""}, // #156
                 std::u32string_view{U"<div class"},   // #157
                 std::u32string_view{U"<div *???-&&&-<---"}, // #158
                 std::u32string_view{U"  <div>"},      // #184
                 std::u32string_view{U"</p>"},
                 std::u32string_view{U"<basefont"},
                 std::u32string_view{U"<track>"},
             }) {
            const auto found = start(source);
            expect(fatal(found.has_value()));
            if (found) {
                expect(found->kind == MarkdownHtmlBlockKind::BlockTag);
                expect(found->tag_start.start == source.find(U'<'));
                expect(markdown_html_block_can_interrupt(found->kind));
            }
        }

        for (const auto source : {std::u32string_view{U"    <div>"}}) {
            expect(!start(source).has_value());
        }
        for (const auto source : {
                 std::u32string_view{U"<divx>"},
                 std::u32string_view{U"<custom>"},
                 std::u32string_view{U"<tableau>"},
             }) {
            const auto found = start(source);
            expect(fatal(found.has_value()));
            if (found) expect(found->kind == MarkdownHtmlBlockKind::OtherTag);
        }
    };

    "official_type1_to_type5_prefixes_are_recognized_without_complete_tag_validation"_test = [] {
        const auto checks = [](std::u32string_view source, MarkdownHtmlBlockKind kind) {
            const auto found = start(source);
            expect(fatal(found.has_value()));
            if (found) {
                expect(found->kind == kind);
                expect(found->tag_start.start == source.find(U'<'));
                expect(markdown_html_block_can_interrupt(kind));
            }
        };
        checks(U"<STYLE\n", MarkdownHtmlBlockKind::RawText); // #173
        checks(U"<textarea>", MarkdownHtmlBlockKind::RawText); // #171
        checks(U"<!-- Foo", MarkdownHtmlBlockKind::Comment); // #179
        checks(U"<?php", MarkdownHtmlBlockKind::ProcessingInstruction); // #180
        checks(U"<!DOCTYPE html>", MarkdownHtmlBlockKind::Declaration); // #181
        checks(U"<![CDATA[", MarkdownHtmlBlockKind::Cdata); // #182

        expect(!start(U"<script/>").has_value());
        const auto custom = start(U"<scripture>");
        expect(fatal(custom.has_value()));
        if (custom) expect(custom->kind == MarkdownHtmlBlockKind::OtherTag);
        const auto lowercase_doctype = start(U"<!doctype html>");
        expect(fatal(lowercase_doctype.has_value()));
        if (lowercase_doctype) {
            expect(lowercase_doctype->kind == MarkdownHtmlBlockKind::Declaration);
        }
        expect(!start(U"<![cdata[").has_value());
    };

    "type7_requires_a_complete_one_line_tag_and_only_trailing_html_whitespace"_test = [] {
        for (const auto source : {
                 std::u32string_view{U"<Warning>"}, // #163
                 std::u32string_view{U"</ins>"}, // #165
                 std::u32string_view{U"<del>"}, // #167
                 std::u32string_view{U"<a href=\"x\" /> \t"},
                 std::u32string_view{U"</script>"},
             }) {
            const auto found = start(source);
            expect(fatal(found.has_value()));
            if (found) {
                expect(found->kind == MarkdownHtmlBlockKind::OtherTag);
                expect(found->tag_start.start == source.find(U'<'));
                expect(!markdown_html_block_can_interrupt(found->kind));
            }
        }
        for (const auto source : {
                 std::u32string_view{U"<a"},
                 std::u32string_view{U"<a href=\"x\""},
                 std::u32string_view{U"<a> trailing"},
                 std::u32string_view{U"<a\n>"},
                 std::u32string_view{U"<a\v>"},
                 std::u32string_view{U"<custom>\f"},
                 std::u32string_view{U"<custom>\v"},
                 std::u32string_view{U"<widget\f data=x>"},
                 std::u32string_view{U"<widget\v data=x>"},
                 std::u32string_view{U"<script\f>"},
                 std::u32string_view{U"<script\v>"},
                 std::u32string_view{U"<div\f>"},
                 std::u32string_view{U"<div\v>"},
             }) {
            expect(!start(source).has_value());
        }

        for (const auto source : {
                 std::u32string_view{U"<widget data=x\fy>"},
                 std::u32string_view{U"<widget data=x\vy>"},
             }) {
            const auto found = start(source);
            expect(fatal(found.has_value()));
            if (found) expect(found->kind == MarkdownHtmlBlockKind::OtherTag);
        }
    };

    "type_specific_terminators_match_commonmark_rules"_test = [] {
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::RawText, U"x </ScRiPt> tail", 0, 16));
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::RawText, U"</textarea>", 0, 11));
        expect(!markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::RawText, U"</scripture>", 0, 12));
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::Comment, U"text --> rest", 0, 13));
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::ProcessingInstruction, U"?>", 0, 2));
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::Declaration, U"<!ELEMENT br EMPTY>", 0, 19));
        expect(markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::Cdata, U"data ]]>", 0, 8));
        expect(!markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::BlockTag, U"</div>", 0, 6));
        expect(!markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::OtherTag, U"<a>", 0, 3));
    };

    "ranges_are_checked_and_line_end_is_exclusive"_test = [] {
        const std::u32string source = U"  <div>tail";
        const auto found = scan_markdown_html_block_start(source, 0, 7);
        expect(fatal(found.has_value()));
        if (found) {
            expect(found->tag_start == SourceRange{2, 6});
            expect(source.substr(found->tag_start.start, found->tag_start.length()) == U"<div");
        }
        expect(!scan_markdown_html_block_start(source, 8, 7).has_value());
        expect(!scan_markdown_html_block_start(source, 0, source.size() + 1).has_value());
        expect(!scan_markdown_html_block_start(source, source.size(), source.size()).has_value());
        expect(!markdown_html_block_ends_on_line(
            MarkdownHtmlBlockKind::Comment, source, 8, source.size() + 1));
    };

    "unquoted_html_attribute_values_keep_form_feed_and_vertical_tab_literal"_test = [] {
        for (const auto source : {
                 std::u32string_view{U"<widget data=x\fy>"},
                 std::u32string_view{U"<widget data=x\vy>"},
             }) {
            const auto tag = scan_markdown_html_tag(source, 0, source.size());
            expect(fatal(tag.has_value()));
            if (tag) {
                expect(tag->attributes.size() == 1_u);
                if (!tag->attributes.empty() && tag->attributes.front().value) {
                    expect(source.substr(tag->attributes.front().value->start,
                        tag->attributes.front().value->length()).compare(source.substr(13, 3)) == 0);
                }
            }
        }
    };
};
