#include "support/folia_test.hpp"
import folia.core.document_edit;
import folia.core.document_inline_format;
import folia.core.document_transaction;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

NodeId format_owner(const Editor& editor, std::u32string_view text) {
    for (const auto id : editor.document().cached_editable_order) {
        const auto* block = find_document_block(editor.document(), id);
        const auto* source = block ? editable_inline_document(*block) : nullptr;
        if (source && source->source == text) return id;
    }
    return {};
}

std::u32string format_source(const Editor& editor, NodeId id) {
    const auto* block = find_document_block(editor.document(), id);
    const auto* source = block ? editable_inline_document(*block) : nullptr;
    expect(fatal(source != nullptr));
    return source ? source->source : std::u32string{};
}

TextSelection format_selection(NodeId first, std::size_t start, NodeId last, std::size_t end, bool reversed) {
    const auto begin = TextPosition{first, start, TextAffinity::Upstream};
    const auto finish = TextPosition{last, end, TextAffinity::Downstream};
    return reversed ? TextSelection{finish, begin} : TextSelection{begin, finish};
}

void check_local_format_work() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_document_text_projections == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}

void check_format_history(Editor& editor, const std::string& before, TextSelection before_selection) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(editor.undo());
    expect(bool(editor.markdown_utf8() == before));
    expect(bool(editor.selection() == before_selection));
    expect(document_indexes_are_exact(editor.document()));
    expect(editor.redo());
    expect(bool(editor.markdown_utf8() == after));
    expect(bool(editor.selection() == after_selection));
    expect(document_indexes_are_exact(editor.document()));
    expect(validate_document(editor.document()).empty());
    Editor reloaded(after);
    expect(bool(reloaded.markdown_utf8() == after));
    expect(validate_document(reloaded.document()).empty());
}

suite inline_format_range_tests = [] {

"multi_owner_format_preserves_nested_context_selection_direction_and_one_step_history"_test = [] {
    const std::vector<std::string> sources{
        "alpha\n\nbeta",
        "# alpha\n\n## beta",
        "- alpha\n- beta",
        "> alpha\n>\n> beta",
        "> [!NOTE] alpha\n> beta",
        "[^n]: alpha\n\n    beta",
        "| alpha | beta |\n| --- | --- |",
        "<div><p>alpha</p><p>beta</p></div>",
    };
    for (const auto& source : sources)
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto first = format_owner(editor, U"alpha");
        const auto last = format_owner(editor, U"beta");
        expect(fatal(first.v != 0 && last.v != 0 && first != last)) << source;
        const auto html = find_document_block(editor.document(), first)->inline_content.syntax_mode == InlineSyntaxMode::HtmlText;
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(first, 2, last, 2, reversed);
        editor.set_selection(before_selection);
        reset_core_operation_counters();
        const auto transaction = editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong);
        expect(fatal(transaction.has_value())) << source;
        check_local_format_work();
        expect(bool(format_source(editor, first) == (html ? U"al<strong>pha</strong>" : U"al**pha**")));
        expect(bool(format_source(editor, last) == (html ? U"<strong>be</strong>ta" : U"**be**ta")));
        const auto offset = html ? 10u : 4u;
        expect(bool(editor.selection() == format_selection(first, offset, last, offset, reversed)));
        expect(document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Strong)) << source;
        check_format_history(editor, before, before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        expect(bool(editor.markdown_utf8() == before)) << source << editor.markdown_utf8();
        expect(bool(editor.selection() == before_selection)) << source;
    }
};

"multi_owner_format_applies_missing_format_without_unformatting_selected_existing_runs"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("__alpha__\n\nbeta");
        const auto first = format_owner(editor, U"__alpha__");
        const auto last = format_owner(editor, U"beta");
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(first, 0, last, 4, reversed);
        editor.set_selection(before_selection);
        expect(!document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Strong));
        const auto applied = editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong);
        expect(fatal(applied.has_value()));
        expect(bool(editor.markdown_utf8() == "__alpha__\n\n**beta**"));
        expect(bool(editor.selection() == format_selection(first, 0, last, 6, reversed)));
        expect(document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Strong));
        expect(std::ranges::count_if(applied->operations, [](const auto& operation) {
            return std::holds_alternative<DocumentTextOperation>(operation);
        }) == 1);
        check_format_history(editor, before, before_selection);

        const auto formatted = editor.markdown_utf8();
        const auto formatted_selection = editor.selection();
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        expect(bool(editor.markdown_utf8() == "alpha\n\nbeta"));
        expect(bool(editor.selection() == format_selection(first, 0, last, 4, reversed)));
        check_format_history(editor, formatted, formatted_selection);
    }
};

"multi_owner_format_removes_only_selected_parts_when_every_owner_is_formatted"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("__alpha__\n\n**beta**");
        const auto first = format_owner(editor, U"__alpha__");
        const auto last = format_owner(editor, U"**beta**");
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(first, 3, last, 4, reversed);
        editor.set_selection(before_selection);
        expect(document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Strong));
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        expect(bool(editor.markdown_utf8() == "__a__lpha\n\nbe**ta**"));
        expect(bool(editor.selection() == format_selection(first, 5, last, 2, reversed)));
        check_format_history(editor, before, before_selection);
    }
};

"multi_owner_format_skips_atomic_and_raw_owners_without_moving_their_endpoints"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("![alt](asset.png)\n\nfirst\n\n```txt\ncode\n```\n\n***\n\nsecond\n\n[TOC]");
        const auto first = format_owner(editor, U"first");
        const auto last = format_owner(editor, U"second");
        const auto image = editor.document().root.children.front().id;
        const auto toc = editor.document().root.children.back().id;
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(image, 1, toc, 0, reversed);
        editor.set_selection(before_selection);
        reset_core_operation_counters();
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        check_local_format_work();
        expect(bool(format_source(editor, first) == U"**first**"));
        expect(bool(format_source(editor, last) == U"**second**"));
        expect(bool(editor.selection() == before_selection));
        expect(document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Strong));
        expect(editor.markdown_utf8().find("```txt\ncode\n```") != std::string::npos);
        expect(find_document_block(editor.document(), image)->kind == BlockKind::ImageBlock);
        expect(find_document_block(editor.document(), toc)->kind == BlockKind::Toc);
        check_format_history(editor, before, before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        expect(bool(editor.markdown_utf8() == before));
    }
};

"multi_owner_format_omits_empty_segments_and_the_unselected_terminal_owner"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("alpha\n\n\nbeta");
        const auto first = format_owner(editor, U"alpha");
        const auto blank = format_owner(editor, U"");
        const auto last = format_owner(editor, U"beta");
        expect(fatal(blank.v != 0));
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(first, 1, last, 0, reversed);
        editor.set_selection(before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
        expect(bool(format_source(editor, first) == U"a**lpha**"));
        expect(format_source(editor, blank).empty());
        expect(bool(format_source(editor, last) == U"beta"));
        expect(bool(editor.selection() == format_selection(first, 3, last, 0, reversed)));
        check_format_history(editor, before, before_selection);
    }
};

"multi_owner_format_supports_each_inline_kind_and_mixed_markdown_html_owners"_test = [] {
    struct Case { InlineFormat format; std::u32string first; std::u32string last; };
    const std::vector<Case> cases{
        {InlineFormat::Emphasis, U"*alpha*", U"*beta*"},
        {InlineFormat::Strikethrough, U"~~alpha~~", U"~~beta~~"},
        {InlineFormat::Code, U"`alpha`", U"`beta`"},
        {InlineFormat::Math, U"$alpha$", U"$beta$"},
    };
    for (const auto& entry : cases) {
        Editor editor("alpha\n\nbeta");
        const auto first = format_owner(editor, U"alpha");
        const auto last = format_owner(editor, U"beta");
        const auto before_selection = format_selection(first, 0, last, 4, false);
        editor.set_selection(before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), entry.format).has_value()));
        expect(bool(format_source(editor, first) == entry.first));
        expect(bool(format_source(editor, last) == entry.last));
        expect(document_inline_format_active(editor.document(), editor.selection(), entry.format));
        check_format_history(editor, "alpha\n\nbeta", before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), entry.format).has_value()));
        expect(bool(editor.markdown_utf8() == "alpha\n\nbeta"));
    }

    Editor mixed("alpha\n\n<div><p>beta</p></div>\n\nomega");
    const auto first = format_owner(mixed, U"alpha");
    const auto html = format_owner(mixed, U"beta");
    const auto last = format_owner(mixed, U"omega");
    expect(fatal(first.v != 0 && html.v != 0 && last.v != 0));
    const auto before = mixed.markdown_utf8();
    const auto before_selection = format_selection(first, 2, last, 2, true);
    mixed.set_selection(before_selection);
    expect(fatal(mixed.execute_document_toggle_inline_format(mixed.selection(), InlineFormat::Strong).has_value()));
    expect(bool(format_source(mixed, first) == U"al**pha**"));
    expect(bool(format_source(mixed, html) == U"<strong>beta</strong>"));
    expect(bool(format_source(mixed, last) == U"**om**ega"));
    check_format_history(mixed, before, before_selection);
    expect(fatal(mixed.execute_document_toggle_inline_format(mixed.selection(), InlineFormat::Strong).has_value()));
    expect(bool(mixed.markdown_utf8() == before));
};

"multi_owner_format_code_padding_and_backticks_round_trip_without_text_loss"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("`left\n\nright`");
        const auto first = format_owner(editor, U"`left");
        const auto last = format_owner(editor, U"right`");
        expect(fatal(first.v != 0 && last.v != 0));
        const auto before = editor.markdown_utf8();
        const auto before_selection = format_selection(first, 0, last, 6, reversed);
        editor.set_selection(before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Code).has_value()));
        expect(bool(format_source(editor, first) == U"`` `left ``"));
        expect(bool(format_source(editor, last) == U"`` right` ``"));
        expect(document_inline_format_active(editor.document(), editor.selection(), InlineFormat::Code));
        check_format_history(editor, before, before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Code).has_value()));
        expect(bool(editor.markdown_utf8() == before));
        expect(bool(editor.selection() == before_selection));
    }
};

"multi_owner_format_html_single_owner_double_toggle_uses_semantic_tags"_test = [] {
    for (const auto format : {InlineFormat::Strong, InlineFormat::Emphasis,
        InlineFormat::Strikethrough, InlineFormat::Code}) {
        Editor editor("<p>alpha</p>");
        const auto owner = format_owner(editor, U"alpha");
        expect(fatal(owner.v != 0));
        const auto before_selection = format_selection(owner, 0, owner, 5, true);
        editor.set_selection(before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), format).has_value()));
        expect(document_inline_format_active(editor.document(), editor.selection(), format));
        check_format_history(editor, "<p>alpha</p>", before_selection);
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), format).has_value()));
        expect(bool(editor.markdown_utf8() == "<p>alpha</p>"));
        expect(bool(editor.selection() == before_selection));
    }
};

"multi_owner_format_html_aliases_preserve_unselected_attribute_spelling"_test = [] {
    const std::vector<std::pair<std::string, InlineFormat>> tags{
        {"strong", InlineFormat::Strong}, {"b", InlineFormat::Strong},
        {"em", InlineFormat::Emphasis}, {"i", InlineFormat::Emphasis},
        {"cite", InlineFormat::Emphasis}, {"var", InlineFormat::Emphasis},
        {"del", InlineFormat::Strikethrough}, {"s", InlineFormat::Strikethrough},
        {"strike", InlineFormat::Strikethrough},
        {"code", InlineFormat::Code}, {"kbd", InlineFormat::Code},
        {"samp", InlineFormat::Code}, {"tt", InlineFormat::Code},
    };
    for (const auto& [tag, format] : tags) {
        const auto opening = "<" + tag + " title = 'kept'>";
        const auto closing = "</" + tag + ">";
        const auto inline_source = opening + "alpha" + closing;
        const auto source = "<p>" + inline_source + "</p>";
        Editor editor(source);
        const auto owner = format_owner(editor, utf8_to_cps(inline_source));
        expect(fatal(owner.v != 0)) << tag;
        const auto before_selection = format_selection(owner, opening.size() + 1,
            owner, opening.size() + 4, true);
        editor.set_selection(before_selection);
        expect(document_inline_format_active(editor.document(), editor.selection(), format)) << tag;
        expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), format).has_value())) << tag;
        const auto expected = "<p>" + opening + "a" + closing + "lph" + opening + "a" + closing + "</p>";
        expect(bool(editor.markdown_utf8() == expected)) << tag << editor.markdown_utf8();
        check_format_history(editor, source, before_selection);
    }
};

"multi_owner_format_rejects_invalid_unsupported_and_empty_ranges_before_mutation"_test = [] {
    for (const auto scenario : {0, 1, 2, 3}) {
        Editor editor(scenario == 2 ? "alpha\n\n<div><p>beta</p></div>" : "alpha\n\nbeta");
        const auto first = format_owner(editor, U"alpha");
        const auto last = format_owner(editor, U"beta");
        const auto before = editor.markdown_utf8();
        const auto original_selection = editor.selection();
        const auto revision = editor.document().revision;
        const auto next_id = editor.document().next_node_id;
        auto selection = format_selection(first, 0, last, 4, false);
        auto format = InlineFormat::Strong;
        if (scenario == 0) selection.active.source_offset = 100;
        if (scenario == 1) selection.active.container_id = NodeId{next_id + 100};
        if (scenario == 2) format = InlineFormat::Math;
        if (scenario == 3) selection = format_selection(first, 5, last, 0, false);
        expect(!editor.execute_document_toggle_inline_format(selection, format).has_value());
        expect(bool(editor.markdown_utf8() == before));
        expect(bool(editor.selection() == original_selection));
        expect(editor.document().revision == revision);
        expect(editor.document().next_node_id == next_id);
        expect(!editor.undo());
        expect(validate_document(editor.document()).empty());
    }
    Editor raw("![alt](asset.png)\n\n```\ncode\n```\n\n[TOC]");
    const auto before = raw.markdown_utf8();
    const auto before_selection = format_selection(raw.document().root.children.front().id, 0,
        raw.document().root.children.back().id, 1, true);
    raw.set_selection(before_selection);
    expect(!raw.execute_document_toggle_inline_format(raw.selection(), InlineFormat::Strong).has_value());
    expect(bool(raw.markdown_utf8() == before));
    expect(bool(raw.selection() == before_selection));
    expect(!raw.undo());
};

"multi_owner_format_uses_indexed_tail_owners_without_a_document_projection"_test = [] {
    std::string source;
    for (std::size_t index = 0; index < 2000; ++index) {
        if (index != 0) source += '\n';
        source += "- item " + std::to_string(index);
    }
    Editor editor(source);
    const auto& order = editor.document().cached_editable_order;
    const auto first = order[order.size() - 2];
    const auto last = order.back();
    editor.set_selection(format_selection(first, 0, last, 4, true));
    const auto before_selection = editor.selection();
    reset_core_operation_counters();
    expect(fatal(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value()));
    check_local_format_work();
    expect(bool(format_source(editor, first) == U"**item 1998**"));
    expect(bool(format_source(editor, last) == U"**item** 1999"));
    check_format_history(editor, source, before_selection);
};

};
}
