#pragma once

namespace {

void text_command_local_work() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}

void text_command_history(Editor& editor, const std::string& before, TextSelection selection) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

std::vector<std::pair<BlockKind, std::u32string>> text_command_visible_owners(const Editor& editor) {
    std::vector<std::pair<BlockKind, std::u32string>> result;
    for (const auto id : editor.document().cached_editable_order) {
        const auto& block = *find_document_block(editor.document(), id);
        if (editable_raw_block_source(block)) result.emplace_back(block.kind, block.block_source.tree().content);
        else if (const auto* inlines = editable_inline_document(block)) result.emplace_back(block.kind, inlines->source);
        else result.emplace_back(block.kind, U"\ufffc");
    }
    return result;
}

void text_command_reload_consistency(const Editor& editor) {
    Editor reloaded(editor.markdown_utf8());
    expect(reloaded.markdown_utf8() == editor.markdown_utf8());
    expect(bool(text_command_visible_owners(reloaded) == text_command_visible_owners(editor)));
}

TextPosition text_command_content_position(const BlockNode& block, std::size_t offset) {
    const auto source_offset = editable_raw_block_source(block)
        ? block.block_source.tree().content_to_source[offset] : offset;
    return {block.id, source_offset, TextAffinity::Downstream};
}

} // namespace
