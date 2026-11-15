#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.document;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void code_selection(Editor& editor, bool source_mode, std::size_t anchor, std::size_t active) {
    if (source_mode) editor.set_source_selection({anchor, active});
    else {
        const auto owner = editor.document().root.children.front().id;
        editor.set_selection({{owner, anchor}, {owner, active}});
    }
}

std::pair<std::size_t, std::size_t> code_selection(const Editor& editor, bool source_mode) {
    if (source_mode) return {editor.source_editor()->selection().anchor, editor.source_editor()->selection().active};
    return {editor.selection().anchor.source_offset, editor.selection().active.source_offset};
}

void code_toggle(Editor& editor) {
    reset_core_operation_counters();
    expect(fatal(editor.execute_command({.kind = CommandKind::ToggleInlineCode})));
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0u);
    expect(counters.full_document_serializations == 0u);
    expect(counters.full_tree_transaction_diffs == 0u);
    expect(fatal(validate_document(editor.document()).empty()));
}

}

suite inline_code_padding_tests = [] {

"code_padding toggling twice preserves backticks spaces and selection in both views"_test = [] {
    for (const auto source_mode : {false, true})
    for (const auto reversed : {false, true})
    for (const auto content : {U"a`b", U"`a", U"a`", U"a``b", U" padded ", U"   ", U"`", U"a`b\nc",
        U"\nword ", U"word\r\n ", U" \n "}) {
        const auto text = std::u32string(content);
        const auto original = U"before " + text + U" after";
        Editor editor(cps_to_utf8(original));
        if (source_mode) expect(fatal(editor.enter_source_mode()));
        const auto begin = 7u;
        const auto end = begin + text.size();
        code_selection(editor, source_mode, reversed ? end : begin, reversed ? begin : end);
        const auto selected = code_selection(editor, source_mode);

        code_toggle(editor);
        const auto wrapped = editor.markdown_utf8();
        const auto wrapped_selection = code_selection(editor, source_mode);
        const auto& source = editor.document().root.children.front().inline_content.source;
        const auto lower = (std::min)(wrapped_selection.first, wrapped_selection.second);
        const auto upper = (std::max)(wrapped_selection.first, wrapped_selection.second);
        expect(source.substr(lower, upper - lower) == text);
        code_toggle(editor);
        expect(editor.markdown_utf8() == cps_to_utf8(original));
        expect(code_selection(editor, source_mode) == selected);
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == wrapped);
        expect(code_selection(editor, source_mode) == wrapped_selection);
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == cps_to_utf8(original));
        expect(code_selection(editor, source_mode) == selected);
        expect(fatal(editor.redo()));
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == cps_to_utf8(original));
        expect(code_selection(editor, source_mode) == selected);
    }
};

"code_padding removing an existing span clamps caret and raw content selections"_test = [] {
    struct Case { std::size_t anchor; std::size_t active; std::size_t after_anchor; std::size_t after_active; };
    for (const auto source_mode : {false, true})
    for (const auto& test : std::vector<Case>{
        {9, 9, 7, 7}, {10, 10, 7, 7}, {11, 11, 8, 8}, {12, 12, 9, 9}, {13, 13, 9, 9},
        {7, 15, 7, 9}, {15, 7, 9, 7}, {9, 13, 7, 9}, {13, 9, 9, 7}, {10, 12, 7, 9},
    }) {
        Editor editor("before `` `a `` after");
        if (source_mode) expect(fatal(editor.enter_source_mode()));
        code_selection(editor, source_mode, test.anchor, test.active);
        code_toggle(editor);
        expect(editor.markdown_utf8() == "before `a after");
        expect(code_selection(editor, source_mode) == std::pair{test.after_anchor, test.after_active});
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == "before `` `a `` after");
        expect(code_selection(editor, source_mode) == std::pair{test.anchor, test.active});
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == "before `a after");
    }
};

"code_padding partial removal preserves remaining all-space code fragments"_test = [] {
    for (const auto source_mode : {false, true}) {
        Editor editor("before `  ab  ` after");
        if (source_mode) expect(fatal(editor.enter_source_mode()));
        code_selection(editor, source_mode, 10, 12);
        code_toggle(editor);
        expect(editor.markdown_utf8() == "before ` `ab` ` after");
        expect(code_selection(editor, source_mode) == std::pair<std::size_t, std::size_t>{10, 12});
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == "before `  ab  ` after");
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == "before ` `ab` ` after");
    }
};

"code_padding paired marker deletion removes padding with the code delimiters"_test = [] {
    for (const auto backward : {false, true}) {
        Editor editor("before `` `a `` after");
        code_selection(editor, false, backward ? 8 : 7, backward ? 8 : 7);
        const auto before_selection = editor.selection();
        expect(fatal(editor.execute_command({.kind = backward ? CommandKind::DeleteBackward : CommandKind::DeleteForward})));
        expect(editor.markdown_utf8() == "before `a after");
        expect(editor.selection().active.source_offset == 7u);
        expect(fatal(validate_document(editor.document()).empty()));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == "before `` `a `` after");
        expect(editor.selection() == before_selection);
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == "before `a after");
    }
};

};
