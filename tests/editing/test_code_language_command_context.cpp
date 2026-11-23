#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite code_language_command_context_tests = [] {

"source code language commands use parsed owners inside all containers"_test = [] {
    const std::vector<std::u32string> sources{
        U"```cpp extra\nvalue\n```",
        U"> ```cpp extra\n> value\n> ```",
        U"- ```cpp extra\n  value\n  ```",
        U"1. ```cpp extra\n   value\n   ```",
        U"> [!NOTE]\r\n> ```cpp extra\r\n> value\r\n> ```",
        U"- > ```cpp extra\n  > value\n  > ```",
        U"[^n]:\n    ```cpp extra\n    value\n    ```",
    };
    for (const auto& source : sources) {
        auto expected = source;
        expected.replace(source.find(U"cpp"), 3, U"python");
        for (const auto offset : {source.find(U"cpp") + 1, source.find(U"value") + 2,
                source.rfind(U"```") + 1}) {
            Editor editor(cps_to_utf8(source));
            expect(fatal(editor.enter_source_mode()));
            expect(fatal(editor.source_editor()->source() == source));
            const auto selected = SourceSelection::caret(offset);
            editor.set_source_selection(selected);
            reset_core_operation_counters();
            expect(fatal(editor.execute_command({.kind = CommandKind::SetCodeBlockLanguage, .lang = U"python"})))
                << cps_to_utf8(source) << offset;
            const auto counters = read_core_operation_counters();
            expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
            expect(editor.source_editor()->source() == expected);
            const auto after = editor.source_editor()->selection();
            expect(editor.undo());
            expect(editor.source_editor()->source() == source);
            expect(editor.source_editor()->selection() == selected);
            expect(editor.redo());
            expect(editor.source_editor()->source() == expected);
            expect(editor.source_editor()->selection() == after);
            expect(editor.exit_source_mode());
            expect(editor.markdown_utf8() == cps_to_utf8(expected));
        }
    }
};

"source code language commands clear auxiliary metadata with the language"_test = [] {
    Editor editor("> ~~~  cpp {.numberLines}\r\n> value\r\n> ~~~");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(editor.source_editor()->source().find(U"value")));
    expect(editor.execute_command({.kind = CommandKind::SetCodeBlockLanguage, .lang = U""}));
    expect(editor.source_editor()->source() == U"> ~~~\r\n> value\r\n> ~~~");
    expect(editor.undo());
    expect(editor.source_editor()->source() == U"> ~~~  cpp {.numberLines}\r\n> value\r\n> ~~~");
};

"code language commands preserve fence length in both views"_test = [] {
    for (const auto source_mode : {false, true}) {
        Editor editor("~~~cpp extra\nvalue\n~~~");
        if (source_mode) {
            expect(fatal(editor.enter_source_mode()));
            editor.set_source_selection(SourceSelection::caret(15));
        }
        expect(editor.execute_command({.kind = CommandKind::SetCodeBlockLanguage, .lang = U"~~next"}));
        expect(editor.markdown_utf8() == "~~~ ~~next extra\nvalue\n~~~");
        expect(editor.undo());
        expect(editor.markdown_utf8() == "~~~cpp extra\nvalue\n~~~");
    }
};

"code language commands reject invalid info tokens without changing content"_test = [] {
    for (const auto source_mode : {false, true}) {
        Editor editor("```cpp\nvalue\n```");
        if (source_mode) {
            expect(fatal(editor.enter_source_mode()));
            editor.set_source_selection(SourceSelection::caret(9));
        }
        for (const auto invalid : {U"a b", U"a\tb", U"a\nb", U"a`b"}) {
            expect(!editor.execute_command({.kind = CommandKind::SetCodeBlockLanguage, .lang = invalid}));
            expect(editor.markdown_utf8() == "```cpp\nvalue\n```");
            expect(!editor.has_undo());
        }
    }
    Editor editor("```cpp\nvalue\n```\n\nordinary");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(editor.source_editor()->source().size()));
    expect(!editor.execute_command({.kind = CommandKind::SetCodeBlockLanguage, .lang = U"python"}));
    expect(!editor.has_undo());
};

};
