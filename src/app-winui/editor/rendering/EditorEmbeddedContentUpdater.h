#pragma once

import folia.core.viewport_plan;

#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorInlineImageRenderer.h"
#include "editor/rendering/EditorPreparedDocument.h"
#include "editor/rendering/EditorRenderResources.h"
#include "editor/session/EditorRenderFrame.h"

namespace winrt::Folia
{
    // Refreshes completed asynchronous image/Mermaid work near the viewport and
    // unloads embedded resources after they leave the retention band.
    class EditorEmbeddedContentUpdater final
    {
    public:
        EditorEmbeddedContentUpdater(
            detail::EditorRenderFrame const& frame,
            EditorRenderResources& resources,
            EditorInlineImageRenderer& inlineImages,
            EditorDocumentBlockPreparer& blockPreparer,
            std::unique_ptr<EditorPreparedDocument>& preparedDocument);

        bool Update(
            float scrollOffset,
            bool printMode,
            bool scrollingForward,
            bool viewportActive,
            std::uint64_t embeddedGeneration,
            std::uint64_t remoteImageGeneration);

    private:
        detail::EditorRenderFrame const& frame;
        EditorRenderResources& resources;
        EditorInlineImageRenderer& inlineImages;
        EditorDocumentBlockPreparer& blockPreparer;
        std::unique_ptr<EditorPreparedDocument>& preparedDocument;
        static constexpr folia::EditorViewportPolicy viewportPolicy{};
    };
}
