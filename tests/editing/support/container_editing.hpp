#pragma once

namespace {

const BlockNode* gap_text(const Editor& editor, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Paragraph
            && block.inline_content().source == source) result = &block;
    });
    return result;
}

std::vector<std::u32string> gap_text_order(const EditorDocument& document) {
    std::vector<std::u32string> result;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (block.kind == BlockKind::Paragraph) result.push_back(block.inline_content().source);
    });
    return result;
}

void gap_history(Editor& editor, const std::string& before, const TextSelection& selection) {
    expect(fatal(validate_document(editor.document()).empty()));
    expect(fatal(document_indexes_are_exact(editor.document())));
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before) << editor.markdown_utf8() << " != " << before;
    expect(editor.selection() == selection);
    expect(fatal(document_indexes_are_exact(editor.document())));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(fatal(validate_document(editor.document()).empty()));
    expect(fatal(document_indexes_are_exact(editor.document())));
}

std::string gap_quote(std::string_view source) {
    std::string result = "> ";
    for (auto value : source) {
        result += value;
        if (value == '\n') result += "> ";
    }
    return result;
}

} // namespace
