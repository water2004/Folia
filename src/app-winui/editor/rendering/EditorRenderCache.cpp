#include "pch.h"
#include "editor/rendering/EditorRenderCache.h"

namespace winrt::Folia
{
    EditorRenderCache::~EditorRenderCache()
    {
        Detach();
    }

    void EditorRenderCache::Attach(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
                                   std::function<void()> invalidate)
    {
        images.Attach(dispatcher, std::move(invalidate));
    }
    void EditorRenderCache::Detach()
    {
        images.Detach();
        ClearTextLayouts();
        ClearSvgDrawings();
    }

    void EditorRenderCache::ClearTextLayouts()
    {
        textLayouts.clear();
        textLayoutOrder.clear();
        textLayoutBytes = 0;
    }

    void EditorRenderCache::ClearSvgDrawings()
    {
        svgDrawings.Clear();
    }

    void EditorRenderCache::ClearDeviceResources()
    {
        ClearTextLayouts();
        ClearSvgDrawings();
        images.ClearDeviceResources();
    }
}
