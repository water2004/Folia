module;
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

export module folia.platform.editor_text_paint;
import std;

export namespace folia::platform::editor {

struct EditorTextPaintStatistics {
    std::size_t visitedRuns = 0;
    std::size_t paintedRuns = 0;
    std::size_t paintedGlyphs = 0;
};

// Geometry belongs to the immutable DirectWrite layout. Keep only run ink
// bounds: DirectWrite continues to own shaping, glyph storage and hit testing.
class EditorTextPaintPlan {
public:
    static std::shared_ptr<const EditorTextPaintPlan> Prepare(IDWriteTextLayout* layout);
    HRESULT Draw(ID2D1DeviceContext* context, D2D1_POINT_2F origin,
        ID2D1Brush* foreground, D2D1_RECT_F viewport,
        EditorTextPaintStatistics* statistics = nullptr) const;

private:
    struct InkBounds { float top; float bottom; };
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    std::vector<InkBounds> ink;
};

}
