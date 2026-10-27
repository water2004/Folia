#pragma once

#include "editor/rendering/EditorImageCache.h"

import folia.platform.editor_svg_drawing;

namespace winrt::Folia
{
    struct EditorRenderCache
    {
        EditorImageCache images;

        ~EditorRenderCache();

        void Attach(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher, std::function<void()> invalidate);
        void Detach();
        void ClearTextLayouts();
        void ClearSvgDrawings();
        void ClearDeviceResources();
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> FindTextLayout(std::uint64_t key);
        void StoreTextLayout(std::uint64_t key, ::Microsoft::WRL::ComPtr<IDWriteTextLayout> const& layout, std::size_t bytes);
        ::Microsoft::WRL::ComPtr<ID2D1CommandList> FindOrCreateSvgDrawing(
            ID2D1DeviceContext5* context, std::uint64_t renderId,
            std::string const& source, float width, float height);

    private:
        struct CachedTextLayout
        {
            ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
            std::size_t bytes = 0;
        };

        std::unordered_map<std::uint64_t, CachedTextLayout> textLayouts;
        std::deque<std::uint64_t> textLayoutOrder;
        std::size_t textLayoutBytes = 0;
        folia::platform::editor::SvgDrawingCache svgDrawings;

    };
}
