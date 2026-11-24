#pragma once

namespace {

Command insert_link_command() {
    Command command;
    command.kind = CommandKind::InsertLink;
    command.href = U"new";
    return command;
}

const BlockNode* link_owner(const Editor& editor, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* owner = editable_inline_document(block); owner && owner->source == source)
            result = &block;
    });
    return result;
}

std::vector<const InlineCstNode*> link_nodes(const InlineDocument& owner) {
    std::vector<const InlineCstNode*> result;
    std::vector<const InlineCstNode*> pending;
    for (const auto& node : owner.tree.nodes) pending.push_back(&node);
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        if (node->kind == InlineCstKind::Link || node->kind == InlineCstKind::Autolink
            || (node->kind == InlineCstKind::HtmlElement && node->semantics().html_tag == "a"))
            result.push_back(node);
        for (const auto& child : node->children) pending.push_back(&child);
    }
    return result;
}

void link_insert_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
}

void link_insert_history(Editor& editor, const std::string& before, TextSelection selected) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* owner = editable_inline_document(block))
            expect(bool(flatten_tokens(owner->tree, owner->source) == owner->source));
    });
    Editor reloaded(after);
    expect(reloaded.markdown_utf8() == after);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selected);
    expect(document_indexes_are_exact(editor.document()));
    expect(!editor.has_undo());
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

} // namespace
