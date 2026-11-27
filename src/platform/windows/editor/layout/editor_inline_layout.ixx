module;
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/implements.h>

export module folia.platform.editor_inline_layout;
import std;
import folia.core.syntax_marker;
import folia.platform.editor_inline_projection;

namespace folia::platform::editor
{
    namespace
    {
        // DirectWrite reserves geometry. The surface paints the actual asset.
        class InlinePlaceholder final : public Microsoft::WRL::RuntimeClass<
            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
        {
        public:
            InlinePlaceholder(DWRITE_INLINE_OBJECT_METRICS metrics,
                DWRITE_BREAK_CONDITION before, DWRITE_BREAK_CONDITION after)
                : metrics(metrics), before(before), after(after) {}

            IFACEMETHODIMP Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) override
            {
                return S_OK;
            }

            IFACEMETHODIMP GetMetrics(DWRITE_INLINE_OBJECT_METRICS* result) override
            {
                if (!result) return E_POINTER;
                *result = metrics;
                return S_OK;
            }

            IFACEMETHODIMP GetOverhangMetrics(DWRITE_OVERHANG_METRICS* result) override
            {
                if (!result) return E_POINTER;
                *result = {};
                return S_OK;
            }

            IFACEMETHODIMP GetBreakConditions(DWRITE_BREAK_CONDITION* beforeResult,
                DWRITE_BREAK_CONDITION* afterResult) override
            {
                if (!beforeResult || !afterResult) return E_POINTER;
                *beforeResult = before;
                *afterResult = after;
                return S_OK;
            }

        private:
            DWRITE_INLINE_OBJECT_METRICS metrics;
            DWRITE_BREAK_CONDITION before;
            DWRITE_BREAK_CONDITION after;
        };
    }

    export Microsoft::WRL::ComPtr<IDWriteInlineObject> CreateInlinePlaceholder(
        DWRITE_INLINE_OBJECT_METRICS metrics,
        DWRITE_BREAK_CONDITION before = DWRITE_BREAK_CONDITION_NEUTRAL,
        DWRITE_BREAK_CONDITION after = DWRITE_BREAK_CONDITION_NEUTRAL)
    {
        return Microsoft::WRL::Make<InlinePlaceholder>(metrics, before, after);
    }

    export std::vector<DWRITE_HIT_TEST_METRICS> TextRangeContentMetrics(
        IDWriteTextLayout* layout, UINT32 start, UINT32 length, D2D1_POINT_2F origin)
    {
        if (!layout || length == 0) return {};
        UINT32 count = 0;
        if (layout->HitTestTextRange(start, length, origin.x, origin.y,
                nullptr, 0, &count) != E_NOT_SUFFICIENT_BUFFER || count == 0) return {};
        std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
        if (FAILED(layout->HitTestTextRange(start, length, origin.x, origin.y,
                metrics.data(), count, &count))) return {};
        metrics.resize(count);
        // Range hits include the full distance to the next line, including
        // leading. Point hits measure the actual shaped content in that row.
        for (auto& metric : metrics)
        {
            FLOAT x = 0.0f, y = 0.0f;
            DWRITE_HIT_TEST_METRICS content{};
            if (SUCCEEDED(layout->HitTestTextPosition(metric.textPosition, FALSE, &x, &y, &content)))
            {
                metric.top = origin.y + content.top;
                metric.height = content.height;
            }
        }
        return metrics;
    }

    static std::vector<D2D1_RECT_F> MergeRangeRectangles(std::vector<DWRITE_HIT_TEST_METRICS> metrics)
    {
        std::vector<D2D1_RECT_F> rectangles;
        // Style runs and bidi runs may split one visual line. Join touching
        // pieces only; never bridge a bidi gap or two wrapped lines.
        std::sort(metrics.begin(), metrics.end(), [](auto const& a, auto const& b) {
            return a.top != b.top ? a.top < b.top : a.left < b.left;
        });
        for (auto const& metric : metrics)
        {
            if (metric.width <= 0 || metric.height <= 0) continue;
            auto rect = D2D1::RectF(metric.left, metric.top,
                metric.left + metric.width, metric.top + metric.height);
            if (!rectangles.empty())
            {
                auto& last = rectangles.back();
                if (std::abs(last.top - rect.top) < 0.5f
                    && std::abs(last.bottom - rect.bottom) < 0.5f
                    && rect.left <= last.right + 0.5f)
                {
                    last.right = (std::max)(last.right, rect.right);
                    continue;
                }
            }
            rectangles.push_back(rect);
        }
        return rectangles;
    }

    export std::vector<D2D1_RECT_F> InlineRangeRectangles(
        IDWriteTextLayout* layout, UINT32 start, UINT32 length, D2D1_POINT_2F origin)
    {
        return MergeRangeRectangles(TextRangeContentMetrics(layout, start, length, origin));
    }

    export std::vector<D2D1_RECT_F> SyntaxIconRectangles(
        IDWriteTextLayout* layout, InlineStyleRange const& range, D2D1_POINT_2F origin)
    {
        if (range.icon == folia::SyntaxMarker::None || range.length == 0 || !layout) return {};
        ::Microsoft::WRL::ComPtr<IDWriteInlineObject> object;
        DWRITE_INLINE_OBJECT_METRICS metrics{};
        if (FAILED(layout->GetInlineObject(range.start, object.GetAddressOf())) || !object
            || FAILED(object->GetMetrics(&metrics))) return {};
        auto rectangles = InlineRangeRectangles(layout, range.start, range.length, origin);
        for (auto& rect : rectangles)
        {
            // Consume the actual inline object's allocation. Block delimiter
            // rows have no clearance intended for neighboring inline text.
            auto gap = (metrics.width - metrics.height) * .5f;
            rect.left += gap;
            rect.right -= gap;
            auto height = metrics.height;
            rect.top += (rect.bottom - rect.top - height) * .5f;
            rect.bottom = rect.top + height;
        }
        return rectangles;
    }
}
