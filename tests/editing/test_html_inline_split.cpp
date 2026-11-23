#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.document_html_split;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.html_cst;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

std::vector<std::u32string> html_inline_sources(const Editor& editor) {
    std::vector<std::u32string> result;
    for (const auto id : editor.document().cached_editable_order) {
        const auto* block = find_document_block(editor.document(), id);
        if (const auto* owner = editable_inline_document(*block)) result.push_back(owner->source);
    }
    return result;
}

void html_split_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}

void html_split_invariants(const Editor& editor) {
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        expect(block.kind != BlockKind::UnsupportedMarkup);
        if (const auto* owner = editable_inline_document(block)) {
            expect(tokens_partition_source(owner->tree, owner->source.size()));
            expect(bool(flatten_tokens(owner->tree, owner->source) == owner->source));
            if (owner->syntax_mode == InlineSyntaxMode::HtmlText)
                expect(!parse_html_cst(owner->source).has_error);
        }
    });
}

void html_split_history(Editor& editor, const std::string& before, TextSelection selected) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    html_split_invariants(editor);
    Editor reloaded(after);
    expect(reloaded.markdown_utf8() == after);
    expect(bool(html_inline_sources(reloaded) == html_inline_sources(editor)));
    html_split_invariants(reloaded);
    expect(fatal(editor.enter_source_mode()));
    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    html_split_locality();
    expect(editor.markdown_utf8() == before);
    expect(fatal(editor.exit_source_mode()));
    expect(editor.selection() == selected);
    html_split_invariants(editor);
    expect(!editor.has_undo());
    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    html_split_locality();
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    html_split_invariants(editor);
}

void expect_html_split(
    const std::string& before,
    const std::string& after,
    std::size_t offset,
    const std::u32string& left_source,
    const std::u32string& right_source,
    std::size_t right_caret_offset) {
    Editor editor(before);
    const auto selected = caret(first_text(editor), offset);
    editor.set_selection(selected);
    reset_core_operation_counters();
    expect(fatal(editor.execute_document_enter(selected).has_value())) << before << offset;
    html_split_locality();
    expect(editor.markdown_utf8() == after) << before << offset << editor.markdown_utf8();
    const auto sources = html_inline_sources(editor);
    expect(bool(sources == std::vector<std::u32string>{left_source, right_source})) << before << offset;
    expect(editor.selection().active.source_offset == right_caret_offset) << before << offset;
    expect(editor.selection().is_caret());
    expect(editor.selection().active.container_id != selected.active.container_id);
    html_split_history(editor, before, selected);
    // The first keystroke belongs inside the reopened formatting envelope.
    expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
    auto expected_right = right_source;
    expected_right.insert(right_caret_offset, U"X");
    const auto* right = find_document_block(editor.document(), editor.selection().active.container_id);
    expect(bool(right->inline_content.source == expected_right));
    html_split_invariants(editor);
    Editor typed_reload(editor.markdown_utf8());
    expect(bool(html_inline_sources(typed_reload) == html_inline_sources(editor)));
}

suite html_inline_split_tests = [] {

"html_inline_split_reopens_nested_elements_with_exact_attributes"_test = [] {
    struct Case { std::u32string opening; std::u32string closing; };
    for (const auto& item : std::array{
            Case{U"<strong>", U"</strong>"},
            Case{U"<SPAN class='tone' data-note=\"a&amp;b\"><em>", U"</em></SPAN>"},
            Case{U"<strong><a href='https://example.test/?a=1&amp;b=2' title=\"note\">", U"</a></strong>"},
            Case{U"<code data-x='one'>", U"</code>"}})
    for (const auto& envelope : std::array{
            std::pair{std::string{"<div>"}, std::string{"</div>"}},
            std::pair{std::string{"> <div>"}, std::string{"</div>"}},
            std::pair{std::string{"- <div>"}, std::string{"</div>"}}}) {
        const auto body = item.opening + U"abc" + item.closing;
        const auto left = item.opening + U"a" + item.closing;
        const auto right = item.opening + U"bc" + item.closing;
        expect_html_split(envelope.first + cps_to_utf8(body) + envelope.second,
            envelope.first + "<p>" + cps_to_utf8(left) + "</p><p>"
                + cps_to_utf8(right) + "</p>" + envelope.second,
            item.opening.size() + 1, left, right, item.opening.size());
    }
};

"html_inline_split_marker_edges_preserve_inside_and_outside_carets"_test = [] {
    const std::u32string opening = U"<span class='kept'>", closing = U"</span>";
    const auto body = opening + U"ab" + closing;
    struct Case { std::size_t offset; std::u32string left; std::u32string right; std::size_t caret; };
    for (const auto& item : std::array{
            Case{0, U"", body, 0},
            Case{1, opening + closing, body, opening.size()},
            Case{opening.size(), opening + closing, body, opening.size()},
            Case{opening.size() + 1, opening + U"a" + closing, opening + U"b" + closing, opening.size()},
            Case{opening.size() + 2, body, opening + closing, opening.size()},
            Case{body.size() - 1, body, opening + closing, opening.size()},
            Case{body.size(), body, U"", 0}}) {
        expect_html_split("<div>" + cps_to_utf8(body) + "</div>",
            "<div><p>" + cps_to_utf8(item.left) + "</p><p>"
                + cps_to_utf8(item.right) + "</p></div>",
            item.offset, item.left, item.right, item.caret);
    }
};

"html_inline_split_keeps_comments_void_tags_and_entities_whole"_test = [] {
    const std::u32string opening = U"<em>", closing = U"</em>";
    for (const auto atom : {U"<!-- comment -->", U"<img src='x.png' alt='picture'>", U"&amp;"}) {
        const std::u32string opaque = atom;
        const auto body = opening + U"a" + opaque + U"b" + closing;
        for (const auto after_atom : {false, true}) {
            const auto offset = opening.size() + 1 + (after_atom ? opaque.size() - 1 : 1);
            const auto left = opening + U"a" + (after_atom ? opaque : U"") + closing;
            const auto right = opening + (after_atom ? U"" : opaque) + U"b" + closing;
            expect_html_split("<div>" + cps_to_utf8(body) + "</div>",
                "<div><p>" + cps_to_utf8(left) + "</p><p>" + cps_to_utf8(right) + "</p></div>",
                offset, left, right, opening.size());
        }
    }
};

"html_inline_split_reopened_prefixes_keep_container_line_anchors"_test = [] {
    const std::u32string opening = U"<strong class='kept'>", closing = U"</strong>";
    for (const auto ending : {U"\n", U"\r\n", U"\r"})
    for (const auto in_list : {false, true}) {
        const std::u32string eol = ending;
        const std::u32string first_prefix = in_list ? U"- " : U"> ";
        const std::u32string next_prefix = in_list ? U"  " : U"> ";
        const auto body = opening + U"a" + eol + U"bc" + eol + U"de" + closing;
        const auto left = opening + U"a" + closing;
        const auto right = opening + eol + U"bc" + eol + U"de" + closing;
        auto with_prefix = [&](const std::u32string& value) {
            std::u32string result;
            for (std::size_t at = 0; at < value.size();) {
                if (std::u32string_view(value).substr(at, eol.size()) == eol) {
                    result += eol + next_prefix;
                    at += eol.size();
                } else result += value[at++];
            }
            return result;
        };
        expect_html_split(cps_to_utf8(first_prefix + U"<div>" + with_prefix(body) + U"</div>"),
            cps_to_utf8(first_prefix + U"<div><p>" + left + U"</p><p>"
                + with_prefix(right) + U"</p></div>"),
            opening.size() + 1, left, right, opening.size());
    }
};

};

} // namespace
