module;
#include <dwrite.h>
#include <wrl/implements.h>

export module folia.platform.editor_block_spacing;
import std;
import folia.markdown.utf;

export namespace folia::platform::editor {

struct EditorBlockSpacing {
    std::uint32_t displayStart = 0;
    std::uint32_t displayLength = 0;
    float height = 0.0f;
};

class BlockSpacingObject final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject> {
public:
    explicit BlockSpacingObject(float height) : height(height) {}
    IFACEMETHODIMP Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) override {
        return S_OK;
    }
    IFACEMETHODIMP GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) override {
        if (!metrics) return E_POINTER;
        *metrics = {0.0f, height, height, FALSE};
        return S_OK;
    }
    IFACEMETHODIMP GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) override {
        if (!metrics) return E_POINTER;
        *metrics = {};
        return S_OK;
    }
    IFACEMETHODIMP GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) override {
        if (!before || !after) return E_POINTER;
        *before = *after = DWRITE_BREAK_CONDITION_NEUTRAL;
        return S_OK;
    }
private:
    float height;
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
        auto object = Microsoft::WRL::Make<BlockSpacingObject>(space.height);
        if (object) layout->SetInlineObject(object.Get(), DWRITE_TEXT_RANGE{space.displayStart, 1});
    }
}

}
