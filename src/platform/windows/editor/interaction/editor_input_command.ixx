// folia.platform.editor_input_command — deterministic Windows key gesture mapping.
export module folia.platform.editor_input_command;
import std;
export import folia.core.input_command;

export namespace folia::platform::editor
{
    // Values intentionally match Windows.System.VirtualKey without importing
    // WinRT into the deterministic input model.
    enum class EditorKey : std::uint32_t
    {
        Back = 8,
        Tab = 9,
        Enter = 13,
        Escape = 27,
        Space = 32,
        PageUp = 33,
        PageDown = 34,
        End = 35,
        Home = 36,
        Left = 37,
        Up = 38,
        Right = 39,
        Down = 40,
        DeleteKey = 46,
        Number0 = 48,
        Number1 = 49,
        Number2 = 50,
        Number3 = 51,
        Number4 = 52,
        Number5 = 53,
        Number6 = 54,
        Number7 = 55,
        Number8 = 56,
        Number9 = 57,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        F1 = 112,
        F2 = 113,
        F3 = 114,
        F4 = 115,
        F5 = 116,
        F6 = 117,
        F7 = 118,
        F8 = 119,
        F9 = 120,
        F10 = 121,
        F11 = 122,
        F12 = 123,
    };

    struct EditorKeyGesture
    {
        EditorKey key{};
        bool control = false;
        bool shift = false;
        bool alt = false;

        bool operator==(EditorKeyGesture const&) const = default;
    };

    enum class EditorCompletionKeyOwner { None, Latex, Snippet, Inline };
    struct EditorCompletionKeyState {
        bool composing = false, latex = false, snippet = false, inline_edit = false;
    };
    inline EditorCompletionKeyOwner CompletionKeyOwner(EditorKeyGesture key, EditorCompletionKeyState state) {
        if (state.composing || key.alt) return EditorCompletionKeyOwner::None;
        // Visible ghost text owns acceptance. An automatically opened command
        // list or an active snippet must not make the displayed AI edit unusable.
        if (state.inline_edit && !key.shift
            && ((!key.control && (key.key == EditorKey::Tab || key.key == EditorKey::Escape))
                || (key.control && key.key == EditorKey::Right))) return EditorCompletionKeyOwner::Inline;
        if (!key.control && !key.shift && state.latex
            && (key.key == EditorKey::Tab || key.key == EditorKey::Enter || key.key == EditorKey::Escape
                || key.key == EditorKey::Up || key.key == EditorKey::Down)) return EditorCompletionKeyOwner::Latex;
        if (!key.control && state.snippet && (key.key == EditorKey::Tab || (!key.shift && key.key == EditorKey::Escape)))
            return EditorCompletionKeyOwner::Snippet;
        return EditorCompletionKeyOwner::None;
    }

    // Hardware Enter is executed by the semantic block editor before
    // CoreText reports the corresponding text update.  Recognize that the
    // insertion is already present in the text service's UTF-16 projection
    // so it is not executed a second time.  The caller owns the ordered
    // ledger of outstanding hardware updates; therefore the caret may have
    // advanced through later edits by the time an earlier acknowledgement
    // arrives.
    inline bool IsAppliedSemanticNewlineUpdate(
        std::wstring_view text,
        std::size_t updateStart) noexcept
    {
        return updateStart < text.size() && text[updateStart] == L'\n';
    }

    inline EditorInputAction TranslateEditorKeyGesture(EditorKeyGesture gesture)
    {
        auto key = KeyEvent{.modifiers = {.ctrl = gesture.control,
            .shift = gesture.shift, .alt = gesture.alt}};
        switch (gesture.key) {
            case EditorKey::Back: key.key_code = KeyCode::Backspace; break;
            case EditorKey::DeleteKey: key.key_code = KeyCode::Delete; break;
            case EditorKey::Enter: key.key_code = KeyCode::Enter; break;
            case EditorKey::Left: key.key_code = KeyCode::Left; break;
            case EditorKey::Right: key.key_code = KeyCode::Right; break;
            case EditorKey::Up: key.key_code = KeyCode::Up; break;
            case EditorKey::Down: key.key_code = KeyCode::Down; break;
            case EditorKey::Home: key.key_code = KeyCode::Home; break;
            case EditorKey::End: key.key_code = KeyCode::End; break;
            case EditorKey::PageUp: key.key_code = KeyCode::PageUp; break;
            case EditorKey::PageDown: key.key_code = KeyCode::PageDown; break;
            case EditorKey::Tab: key.key_code = KeyCode::Tab; break;
            default: return {};
        }
        return translate_editor_editing_key(key);
    }
}
