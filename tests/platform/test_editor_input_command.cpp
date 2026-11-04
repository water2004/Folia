#include "support/folia_test.hpp"

import folia.platform.editor_input_command;

using namespace boost::ut;
using namespace folia;
using namespace folia::platform::editor;

suite editor_input_command_tests = [] {

"completion keys have one owner and respect modifiers composition and local suggestions"_test = [] {
    using Owner = EditorCompletionKeyOwner;
    for (bool composing : {false, true})
    for (bool latex : {false, true})
    for (bool snippet : {false, true})
    for (bool ai : {false, true}) {
        EditorCompletionKeyState state{composing, latex, snippet, ai};
        auto tab = CompletionKeyOwner({.key = EditorKey::Tab}, state);
        auto expected = composing ? Owner::None : latex ? Owner::Latex : snippet ? Owner::Snippet : ai ? Owner::Inline : Owner::None;
        expect(tab == expected);
        expect(CompletionKeyOwner({.key = EditorKey::Tab, .control = true}, state) == Owner::None);
        expect(CompletionKeyOwner({.key = EditorKey::Tab, .shift = true}, state)
            == (!composing && snippet ? Owner::Snippet : Owner::None));
        expect(CompletionKeyOwner({.key = EditorKey::Tab, .alt = true}, state) == Owner::None);
        expect(CompletionKeyOwner({.key = EditorKey::Right, .control = true}, state)
            == (!composing && !latex && !snippet && ai ? Owner::Inline : Owner::None));
        expect(CompletionKeyOwner({.key = EditorKey::Enter}, state)
            == (!composing && latex ? Owner::Latex : Owner::None));
    }
};

"text navigation gestures preserve selection extension"_test = [] {
    auto left = TranslateEditorKeyGesture({
        .key = EditorKey::Left,
        .shift = true,
    });
    expect(left.kind == EditorInputActionKind::ExecuteCommand);
    expect(left.command.kind == CommandKind::MoveLeft);
    expect(left.command.extend_selection);

    auto up = TranslateEditorKeyGesture({
        .key = EditorKey::Up,
        .shift = true,
    });
    expect(up.kind == EditorInputActionKind::VisualLineUp);
    expect(up.command.extend_selection);
};

"enter and tab remain semantic structure actions"_test = [] {
    auto enter = TranslateEditorKeyGesture({.key = EditorKey::Enter});
    expect(enter.kind == EditorInputActionKind::ExecuteCommandIfApplied);
    expect(enter.command.kind == CommandKind::InsertNewline);

    auto softBreak = TranslateEditorKeyGesture({
        .key = EditorKey::Enter,
        .shift = true,
    });
    expect(softBreak.command.kind == CommandKind::InsertSoftBreak);
    expect(TranslateEditorKeyGesture({.key = EditorKey::Tab}).kind
        == EditorInputActionKind::TabForward);
    expect(TranslateEditorKeyGesture({.key = EditorKey::Tab, .shift = true}).kind
        == EditorInputActionKind::TabBackward);
};

"unknown and control-modified unsupported keys are not consumed"_test = [] {
    expect(!TranslateEditorKeyGesture({
        .key = static_cast<EditorKey>(0xffffu),
    }).Handled());
    expect(!TranslateEditorKeyGesture({
        .key = EditorKey::B,
        .control = true,
    }).Handled());
};

"semantic newline acknowledgements include consecutive empty lines"_test = [] {
    expect(IsAppliedSemanticNewlineUpdate(L"$$\n$$", 2));
    expect(IsAppliedSemanticNewlineUpdate(L"$$\n\n$$", 2));
    expect(IsAppliedSemanticNewlineUpdate(L"$$\n\n$$", 3));
    expect(IsAppliedSemanticNewlineUpdate(L"quote\n\nnext", 5));
    expect(IsAppliedSemanticNewlineUpdate(L"quote\n\nnext", 6));

    expect(!IsAppliedSemanticNewlineUpdate(L"$$\n\n$$", 1));
    expect(!IsAppliedSemanticNewlineUpdate(L"$$\n\n$$", 4));
    expect(!IsAppliedSemanticNewlineUpdate(L"plain", 5));
};

}; // suite editor_input_command_tests
