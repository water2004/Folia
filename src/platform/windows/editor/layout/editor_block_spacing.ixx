module;
#include <dwrite.h>

export module folia.platform.editor_block_spacing;
import std;
import folia.markdown.utf;
import folia.platform.editor_inline_layout;

export namespace folia::platform::editor {

struct EditorBlockSpacing {
    std::uint32_t displayStart = 0;
    std::uint32_t displayLength = 0;
    float height = 0.0f;
};

inline void ApplyEditorBlockSpacing(
    IDWriteTextLayout* layout, std::u32string_view text,
    std::span<EditorBlockSpacing const> spaces) {
    if (!layout || spaces.empty()) return;
    layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, 0.0f);
    for (auto const& space : spaces) {
        auto length = space.displayLength;
        auto end = folia::utf16_to_char_index(text, space.displayStart + length);
        if (end < text.size() && text[end] == U'\n') ++length;
        // The entire spacer line, including its generated newline, must have
        // a small font so the surrounding body font cannot inflate its height.
        layout->SetFontSize(1.0f, DWRITE_TEXT_RANGE{space.displayStart, length});
        auto object = CreateInlinePlaceholder({0.0f, space.height, space.height, FALSE});
        if (object) layout->SetInlineObject(object.Get(), DWRITE_TEXT_RANGE{space.displayStart, 1});
    }
}

}
