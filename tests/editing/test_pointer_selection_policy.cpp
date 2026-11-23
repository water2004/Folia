#include "support/folia_test.hpp"

import folia.core.editor;
import folia.core.input_command;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.text_edit;
import folia.markdown.selection;

using namespace boost::ut;
using namespace folia;

suite pointer_selection_policy_tests = [] {

"shift pointer selection preserves the anchor across rendered owners and exact undo"_test = [] {
    for (bool reversed : {false, true}) {
        Editor editor("alpha\n\nbeta");
        auto first = editor.document().root.children.front().id;
        auto last = editor.document().root.children.back().id;
        TextPosition start{first, 2, TextAffinity::Downstream};
        TextPosition end{last, 2, TextAffinity::Downstream};
        const auto anchor = reversed ? end : start;
        const auto target = reversed ? start : end;
        editor.set_caret(anchor);
        auto extended = editor_pointer_selection(editor.selection(), target, true);
        expect(extended.anchor == anchor);
        expect(extended.active == target);
        editor.set_selection(extended);
        expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
        expect(editor.text_utf8() == "alXta");
        expect(fatal(editor.undo()));
        expect(editor.text_utf8() == "alpha\n\nbeta");
        expect(editor.selection() == extended);
        auto plain = editor_pointer_selection(extended, target, false);
        expect(plain == TextSelection::caret(target));
        auto shrunk = editor_pointer_selection(extended, anchor, true);
        expect(shrunk == TextSelection::caret(anchor));
    }
};

"shift pointer selection extends source line selections without changing their anchor"_test = [] {
    for (bool reversed : {false, true}) {
        Editor editor("alpha\r\nbeta");
        expect(fatal(editor.enter_source_mode()));
        auto anchorOffset = reversed ? 9u : 2u;
        auto targetOffset = reversed ? 2u : 9u;
        editor.set_source_selection(SourceSelection::caret(anchorOffset));
        auto current = editor.source_editor()->projected_selection();
        auto target = editor.source_editor()->position_from_source_offset(targetOffset);
        auto extended = editor_pointer_selection(current, target, true);
        auto a = editor.source_editor()->source_offset_from_position(extended.anchor);
        auto b = editor.source_editor()->source_offset_from_position(extended.active);
        expect(fatal(a.has_value() && b.has_value()));
        editor.set_source_selection({*a, *b, extended.anchor.affinity, extended.active.affinity});
        const auto before = editor.source_editor()->selection();
        expect(*a == anchorOffset && *b == targetOffset);
        expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
        expect(editor.text_utf8() == "alXta");
        expect(fatal(editor.undo()));
        expect(editor.text_utf8() == "alpha\r\nbeta");
        expect(editor.source_editor()->selection() == before);
    }
};

};
