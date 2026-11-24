#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_clipboard_source_fragments_tests = [] {

"cross_block_selected_text_uses_tree_order_without_a_global_selection_offset"_test = [] {
    Editor editor("one\n\ntwo");
    const auto first = editor.document().root.children.front().id;
    const auto second = editor.document().root.children.back().id;
    editor.set_selection({
        {first, 2, TextAffinity::Downstream},
        {second, 1, TextAffinity::Upstream}});
    expect(fatal(bool(editor.selected_text_cps() == U"e\nt")));
    editor.set_selection({
        {second, 1, TextAffinity::Upstream},
        {first, 2, TextAffinity::Downstream}});
    expect(fatal(bool(editor.selected_text_cps() == U"e\nt")));

    Editor marked("**one**");
    const auto marked_id = first_text(marked).id;
    marked.set_selection({
        {marked_id, 1, TextAffinity::Downstream},
        {marked_id, 6, TextAffinity::Upstream}});
    reset_core_operation_counters();
    expect(fatal(bool(marked.selected_text_cps() == U"*one*")));
    expect(fatal(bool(read_core_operation_counters().full_document_text_projections == 0u)));
};

"clipboard_copy_preserves_exact_inline_source_and_recursive_block_structure"_test = [] {
    Editor marked("**one**");
    const auto marked_id = first_text(marked).id;
    marked.set_selection({
        {marked_id, 1, TextAffinity::Downstream},
        {marked_id, 6, TextAffinity::Upstream}});
    reset_core_operation_counters();
    expect(fatal(bool(marked.selected_markdown_cps() == U"*one*"))) << "same owner";
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));

    Editor into_list("foo\n\n- one\n- two");
    const auto& first = into_list.document().root.children.front();
    const auto& list = into_list.document().root.children.back();
    const auto& second_item_text = list.children[1].children.front();
    into_list.set_selection({
        {first.id, 1, TextAffinity::Downstream},
        {second_item_text.id, 1, TextAffinity::Upstream}});
    expect(fatal(bool(into_list.selected_markdown_cps() == U"oo\n\n- one\n- t"))) << "into list";

    Editor out_of_list("- bar\n\nfoo");
    const auto& leading_list = out_of_list.document().root.children.front();
    const auto& item_text = leading_list.children.front().children.front();
    const auto& trailing = out_of_list.document().root.children.back();
    out_of_list.set_selection({
        {item_text.id, 2, TextAffinity::Downstream},
        {trailing.id, 1, TextAffinity::Upstream}});
    const auto out_of_list_copy = out_of_list.selected_markdown_cps();
    expect(fatal(bool(out_of_list_copy == U"- r\n\nf")))
        << "out of list: " << cps_to_utf8(out_of_list_copy);

    Editor nested("> alpha\n>\n> - beta\n>   - gamma\n\nomega");
    const auto fragments = document_text_fragments(nested.document());
    expect(fatal(bool(fragments.size() >= 4u))) << "nested fragment count";
    if (fragments.size() < 4u) return;
    const auto gamma = std::ranges::find_if(fragments, [](const auto& fragment) {
        return fragment.text == U"gamma";
    });
    expect(fatal(bool(gamma != fragments.end()))) << "nested gamma owner";
    if (gamma == fragments.end()) return;
    nested.set_selection({
        {fragments[0].container_id, 2, TextAffinity::Downstream},
        {gamma->container_id, 3, TextAffinity::Upstream}});
    reset_core_operation_counters();
    const auto copied = nested.selected_markdown_cps();
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_text_projections == 0u)));
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(copied.starts_with(U"> ")))) << "nested quote prefix";
    expect(fatal(bool(copied.find(U"- beta") != std::u32string::npos))) << "nested list";
    expect(fatal(bool(copied.find(U"- gam") != std::u32string::npos))) << "nested list boundary";
    expect(fatal(bool(copied.find(U"gamma") == std::u32string::npos))) << "nested truncation";
};

"cross_owner_copy_fragments_reparse_truncated_inline_and_raw_sources"_test = [] {
    Editor editor("first **bold**\n\n```cpp\nvalue\n```");
    const auto& paragraph = editor.document().root.children.front();
    const auto& code = editor.document().root.children.back();
    const TextSelection selection{
        {paragraph.id, 6, TextAffinity::Downstream},
        {code.id, 8, TextAffinity::Upstream}};
    const auto ordered = document_copy_detail::order_selection(
        editor.document(),
        selection);
    expect(fatal(bool(ordered.has_value())));
    if (!ordered) return;

    auto next_copy_id = editor.document().next_node_id;
    auto paragraph_copy = document_copy_detail::slice_block(
        paragraph,
        *ordered,
        next_copy_id);
    auto code_copy = document_copy_detail::slice_block(
        code,
        *ordered,
        next_copy_id);
    expect(fatal(bool(paragraph_copy.has_value())));
    expect(fatal(bool(code_copy.has_value())));
    if (!paragraph_copy || !code_copy) return;

    const auto& inline_document = paragraph_copy->inline_content;
    expect(fatal(bool(inline_document.source == U"**bold**")));
    expect(fatal(bool(tokens_partition_source(
        inline_document.tree,
        inline_document.source.size()))));
    expect(fatal(bool(roots_partition_source(
        inline_document.tree,
        inline_document.source.size()))));
    expect(fatal(bool(flatten_tokens(inline_document.tree, inline_document.source)
        == inline_document.source)));
    expect(fatal(bool(serialize_lossless(inline_document.tree, inline_document.source)
        == inline_document.source)));

    expect(fatal(bool(code_copy->block_source.source() == U"```cpp\nv\n```")));
    expect(fatal(bool(block_source_tokens_partition(code_copy->block_source))));
    expect(fatal(bool(flatten_block_source_tokens(code_copy->block_source)
        == code_copy->block_source.source())));
};

};
