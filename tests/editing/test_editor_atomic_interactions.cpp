#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
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

const BlockNode* atomic_owner(const Editor& editor, BlockKind kind) {
    const BlockNode* found = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!found && block.kind == kind) found = &block;
    });
    return found;
}

void check_local_atomic_work(std::string_view context = {}) {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u) << context;
    expect(counts.full_document_serializations == 0_u) << context;
    expect(counts.full_tree_transaction_diffs == 0_u) << context;
    expect(counts.full_document_block_index_scans == 0_u) << context;
}

void check_atomic_history(Editor& editor, const std::string& before,
    TextSelection before_selection, const std::string& after, TextSelection after_selection) {
    expect(editor.undo());
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == before_selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(editor.redo());
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(validate_document(editor.document()).empty());
}

suite editor_atomic_interaction_tests = [] {

"atomic_enter_creates_reachable_paragraph_at_both_object_boundaries"_test = [] {
    const std::vector<std::pair<std::string, BlockKind>> cases{
        {"![alt](asset.png)", BlockKind::ImageBlock},
        {"***", BlockKind::ThematicBreak},
        {"[TOC]", BlockKind::Toc},
        {"---\ntitle: value\n---\n", BlockKind::Frontmatter},
        {"[ref]: https://example.com\n", BlockKind::LinkDefinition},
        {"<script>alert(1)</script>\n", BlockKind::UnsupportedMarkup},
        {"> ![alt](asset.png)", BlockKind::ImageBlock},
        {"- ![alt](asset.png)", BlockKind::ImageBlock},
        {"- [ ] ![alt](asset.png)", BlockKind::ImageBlock},
        {"[^n]: ![alt](asset.png)", BlockKind::ImageBlock},
        {"before\n\n![alt](asset.png)\n\nafter", BlockKind::ImageBlock},
    };
    for (const auto& [source, kind] : cases)
    for (const auto offset : {0u, 1u})
    for (const auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream}) {
        Editor editor(source);
        const auto* owner = atomic_owner(editor, kind);
        expect(fatal(owner != nullptr)) << source;
        if (!owner) continue;
        const auto owner_id = owner->id;
        const auto before_path = *document_block_path(editor.document(), owner_id);
        const auto before = editor.markdown_utf8();
        const auto before_selection = TextSelection::caret({owner_id, offset, affinity});
        editor.set_selection(before_selection);
        reset_core_operation_counters();
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter))) << source << offset;
        check_local_atomic_work();

        const auto after_selection = editor.selection();
        const auto* paragraph = find_document_block(editor.document(), after_selection.active.container_id);
        expect(fatal(paragraph != nullptr));
        if (!paragraph) continue;
        expect(paragraph->kind == BlockKind::Paragraph);
        expect(paragraph->inline_content().source.empty());
        expect(after_selection.active.source_offset == 0_u);
        const auto paragraph_path = *document_block_path(editor.document(), paragraph->id);
        const auto atom_path = *document_block_path(editor.document(), owner_id);
        expect(paragraph_path.size() == before_path.size());
        expect(paragraph_path.back() == before_path.back() + (offset == 0 ? 0u : 1u));
        expect(atom_path.back() == before_path.back() + (offset == 0 ? 1u : 0u));

        const auto after = editor.markdown_utf8();
        check_atomic_history(editor, before, before_selection, after, after_selection);
        expect(fatal(editor.execute_command(Command::InsertText(U"next\U0001F642"))));
        const auto* typed = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(typed != nullptr));
        if (typed) {
            expect(typed->inline_content().source == U"next\U0001F642");
            expect(flatten_tokens(typed->inline_content().tree, typed->inline_content().source)
                == typed->inline_content().source);
        }
        expect(atomic_owner(editor, kind) != nullptr);
        expect(validate_document(editor.document()).empty());
    }
};

"atomic_pasted_image_can_enter_continue_typing_and_replay_history"_test = [] {
    Editor editor;
    Command paste;
    paste.kind = CommandKind::Paste;
    paste.text = U"![alt](clipboard-image.png)";
    expect(fatal(editor.execute_command(paste)));
    const auto* image = atomic_owner(editor, BlockKind::ImageBlock);
    expect(fatal(image != nullptr));
    if (!image) return;
    const auto image_id = image->id;
    expect(editor.selection().active.container_id == image_id);
    expect(editor.selection().active.source_offset == 1_u);
    const auto before = editor.markdown_utf8();
    const auto before_selection = editor.selection();
    Command enter;
    enter.kind = CommandKind::InsertNewline;
    reset_core_operation_counters();
    expect(fatal(editor.execute_command(enter)));
    check_local_atomic_work();
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    check_atomic_history(editor, before, before_selection, after, after_selection);
    expect(editor.execute_command(Command::InsertText(U"caption")));
    expect(editor.editable_source(editor.selection().active.container_id)
        == std::optional<std::u32string>{U"caption"});
    expect(find_document_block(editor.document(), image_id) != nullptr);
};

"atomic_boundary_paragraph_inherits_html_container_syntax"_test = [] {
    for (const auto source : {"<div><hr></div>", "<blockquote><hr></blockquote>", "<ul><li><hr></li></ul>"}) {
        Editor editor(source);
        const auto* rule = atomic_owner(editor, BlockKind::ThematicBreak);
        expect(fatal(rule != nullptr)) << source;
        if (!rule) continue;
        editor.set_selection(TextSelection::caret({rule->id, 1}));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter)));
        const auto owner_id = editor.selection().active.container_id;
        const auto* paragraph = find_document_block(editor.document(), owner_id);
        expect(fatal(paragraph != nullptr));
        if (!paragraph) continue;
        expect(paragraph->inline_content().syntax_mode == InlineSyntaxMode::HtmlText);
        expect(editor.execute_command(Command::InsertText(U"# literal")));
        expect(find_document_block(editor.document(), owner_id)->kind == BlockKind::Paragraph);
        expect(editor.markdown_utf8().find("# literal") != std::string::npos);
        expect(validate_document(editor.document()).empty());
    }
    Editor standalone("<hr>");
    const auto* rule = atomic_owner(standalone, BlockKind::ThematicBreak);
    expect(fatal(rule != nullptr));
    if (!rule) return;
    standalone.set_selection(TextSelection::caret({rule->id, 1}));
    Command enter;
    enter.kind = CommandKind::InsertNewline;
    expect(fatal(standalone.execute_command(enter)));
    const auto* paragraph = find_document_block(standalone.document(), standalone.selection().active.container_id);
    expect(fatal(paragraph != nullptr));
    if (paragraph) expect(paragraph->inline_content().syntax_mode == InlineSyntaxMode::Markdown);
};

"atomic_thematic_markers_activate_on_enter_and_leave_editable_following_text"_test = [] {
    for (const auto marker : {U"---", U"----", U"***", U"___", U"  ---  ", U"* * *", U"- - -", U"_\t_\t_"}) {
        Editor editor;
        for (const auto value : std::u32string_view(marker)) {
            expect(fatal(editor.execute_command(Command::InsertText(std::u32string(1, value)))))
                << cps_to_utf8(marker);
        }
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter))) << cps_to_utf8(marker);
        check_local_atomic_work(cps_to_utf8(marker));
        const auto* rule = atomic_owner(editor, BlockKind::ThematicBreak);
        expect(rule != nullptr) << cps_to_utf8(marker);
        if (!rule) continue;
        expect(rule->text_special().opening_marker == marker) << cps_to_utf8(marker);
        expect(editor.document().root.children.front().kind == BlockKind::ThematicBreak);
        const auto* following = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(following != nullptr));
        if (!following) continue;
        expect(following->kind == BlockKind::Paragraph);
        expect(following->inline_content().source.empty());
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        check_atomic_history(editor, before, before_selection, after, after_selection);
        expect(editor.execute_command(Command::InsertText(U"after rule")));
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().root.children.front().kind == BlockKind::ThematicBreak);
        expect(reloaded.document().root.children.back().inline_content().source == U"after rule");
    }
};

"atomic_thematic_input_does_not_reclassify_incomplete_or_literal_content"_test = [] {
    for (const auto marker : {U"--", U"**", U"__", U"-*-", U"---x", U"    ---", U"\\---"}) {
        Editor editor;
        expect(fatal(editor.execute_command(Command::InsertText(marker))));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(editor.execute_command(enter));
        expect(atomic_owner(editor, BlockKind::ThematicBreak) == nullptr) << cps_to_utf8(marker);
        expect(validate_document(editor.document()).empty());
    }
    for (const auto source : {"# ---", "```\n---\n```", "| --- |\n| --- |\n| body |"}) {
        Editor editor(source);
        auto owner_id = editor.selection().active.container_id;
        const auto* owner = find_document_block(editor.document(), owner_id);
        expect(fatal(owner != nullptr));
        if (!owner) continue;
        const auto range = editable_block_source_range(*owner);
        expect(fatal(range.has_value()));
        if (!range) continue;
        editor.set_selection(TextSelection::caret({owner_id, range->end}));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(editor.execute_command(enter));
        expect(atomic_owner(editor, BlockKind::ThematicBreak) == nullptr) << source;
    }
};

"atomic_thematic_recognition_does_not_join_list_marker_lines"_test = [] {
    for (const auto source : {"-\n  --", "- \n  --", "*\n  **", "* \r\n  **"}) {
        Editor editor(source);
        const auto& paragraph = first_text(editor);
        editor.set_selection(caret(paragraph, paragraph.inline_content().source.size()));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(editor.execute_command(enter));
        expect(atomic_owner(editor, BlockKind::ThematicBreak) == nullptr) << source;
        expect(editor.document().root.children.front().kind == BlockKind::List) << source;
    }
};

"atomic_input_markers_remain_literal_in_html_text_owners"_test = [] {
    for (const auto marker : {U"# ", U"> ", U"- ", U"1. "}) {
        Editor editor("<p>body</p>");
        const auto owner_id = first_text(editor).id;
        const auto before = editor.markdown_utf8();
        const auto before_selection = TextSelection::caret({owner_id, 0});
        editor.set_selection(before_selection);
        expect(fatal(editor.execute_command(Command::InsertText(marker))));
        const auto* edited = find_document_block(editor.document(), owner_id);
        expect(fatal(edited != nullptr));
        if (!edited) continue;
        expect(edited->kind == BlockKind::Paragraph);
        expect(edited->inline_content().syntax_mode == InlineSyntaxMode::HtmlText);
        expect(edited->inline_content().source == std::u32string(marker) + U"body");
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        check_atomic_history(editor, before, before_selection, after, after_selection);
    }
    for (const auto marker : {"```", "~~~", "$$", "---"}) {
        Editor editor(std::string("<p>") + marker + "</p>");
        const auto owner_id = first_text(editor).id;
        editor.set_selection(caret(first_text(editor), first_text(editor).inline_content().source.size()));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter)));
        expect(atomic_owner(editor, BlockKind::CodeBlock) == nullptr);
        expect(atomic_owner(editor, BlockKind::MathBlock) == nullptr);
        expect(atomic_owner(editor, BlockKind::ThematicBreak) == nullptr);
        expect(find_document_block(editor.document(), owner_id)->kind == BlockKind::Paragraph);
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        check_atomic_history(editor, before, before_selection, after, after_selection);
    }
};

"atomic_setext_heading_input_preserves_markers_and_exact_history"_test = [] {
    struct Case {
        std::u32string source;
        std::u32string title;
        std::uint8_t level;
    };
    const std::vector<Case> cases{
        {U"title\n---", U"title", 2},
        {U"*title*\n===", U"*title*", 1},
        {U"  title  \r\n  --- \t", U"title", 2},
        {U"first\nsecond\n=", U"first\nsecond", 1},
    };
    for (const auto& entry : cases) {
        Editor editor;
        expect(fatal(editor.execute_command(Command::InsertText(entry.source))));
        const auto owner_id = editor.selection().active.container_id;
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter)));
        check_local_atomic_work();
        const auto* heading = find_document_block(editor.document(), owner_id);
        expect(fatal(heading != nullptr));
        if (!heading) continue;
        expect(heading->kind == BlockKind::Heading);
        expect(heading->text_special().level == entry.level);
        expect(bool(heading->inline_content().source == entry.title));
        expect(bool(heading->text_special().opening_marker + heading->inline_content().source
            + heading->text_special().closing_marker == entry.source));
        const auto* following = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(following != nullptr));
        if (following) expect(following->kind == BlockKind::Paragraph);
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        check_atomic_history(editor, before, before_selection, after, after_selection);
        expect(editor.execute_command(Command::InsertText(U"body")));
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().root.children.front().kind == BlockKind::Heading);
        expect(bool(reloaded.document().root.children.front().inline_content().source == entry.title));
        expect(reloaded.document().root.children.back().inline_content().source == U"body");
    }
};

"atomic_setext_heading_input_keeps_quote_list_task_and_footnote_context"_test = [] {
    for (const auto initial : {"> ", "- ", "- [ ] ", "[^n]: "}) {
        Editor editor(initial);
        const auto root_kind = editor.document().root.children.front().kind;
        const auto owner_id = first_text(editor).id;
        editor.set_selection(caret(first_text(editor)));
        expect(fatal(editor.execute_command(Command::InsertText(U"title\n---"))));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter))) << initial;
        const auto* heading = find_document_block(editor.document(), owner_id);
        expect(fatal(heading != nullptr));
        if (heading) {
            expect(heading->kind == BlockKind::Heading);
            expect(heading->inline_content().source == U"title");
        }
        expect(editor.document().root.children.front().kind == root_kind);
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        check_atomic_history(editor, before, before_selection, after, after_selection);
        expect(editor.execute_command(Command::InsertText(U"following")));
        expect(editor.editable_source(editor.selection().active.container_id)
            == std::optional<std::u32string>{U"following"});
    }
};

"atomic_setext_input_requires_a_complete_final_underline"_test = [] {
    for (const auto source : {U"\n---", U"title\n---x", U"title\n= =", U"title\n    ---", U"title\n\\---"}) {
        Editor editor;
        expect(fatal(editor.execute_command(Command::InsertText(source))));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(editor.execute_command(enter));
        expect(atomic_owner(editor, BlockKind::Heading) == nullptr) << cps_to_utf8(source);
        expect(validate_document(editor.document()).empty());
    }
};

"atomic_raw_fence_activation_preserves_existing_sibling_separator"_test = [] {
    Editor editor("before\r\n \t\r\nplaceholder");
    const auto& paragraph = editor.document().root.children.back();
    const auto owner_id = paragraph.id;
    const auto separator = paragraph.separator_before;
    editor.set_selection(range(paragraph, 0, paragraph.inline_content().source.size()));
    expect(fatal(editor.execute_command(Command::InsertText(U"```cpp"))));
    const auto before = editor.markdown_utf8();
    const auto before_selection = editor.selection();
    Command enter;
    enter.kind = CommandKind::InsertNewline;
    expect(fatal(editor.execute_command(enter)));
    const auto* code = find_document_block(editor.document(), owner_id);
    expect(fatal(code != nullptr));
    if (!code) return;
    expect(code->kind == BlockKind::CodeBlock);
    expect(bool(code->separator_before == separator));
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    check_atomic_history(editor, before, before_selection, after, after_selection);
};

};
}
