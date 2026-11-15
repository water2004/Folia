#pragma once

import folia.platform.editor_scroll_state;
import folia.core.viewport_plan;

#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorPreparedDocument.h"
#include "editor/rendering/EditorRenderResources.h"
#include "editor/session/EditorRenderFrame.h"

namespace winrt::Folia
{
    struct EditorViewportMaterializationResult
    {
        float scrollOffset = 0.0f;
        bool scrollingForward = true;
        bool viewportActive = false;
        bool invalidateRequested = false;
    };

    // Materializes visible/prefetch blocks under a frame budget. It owns the
    // scroll-anchor correction caused by newly measured block heights, but it
    // does not refresh completed media or evict retained resources.
    class EditorViewportMaterializer final
    {
    public:
        EditorViewportMaterializer(
            detail::EditorRenderFrame const& frame,
            EditorRenderResources& resources,
            EditorDocumentBlockPreparer& blockPreparer,
            EditorInlineImageRenderer& inlineImages,
            folia::platform::editor::EditorScrollState& scrollState,
            std::unique_ptr<EditorPreparedDocument>& preparedDocument);

        EditorViewportMaterializationResult Materialize(
            float scrollOffset,
            bool printMode);

        // A source scroll anchor may move to a different fragment after an
        // edit. Resolve that one fragment before choosing the new viewport.
        bool EnsurePrepared(std::size_t index, bool requestEmbedded, bool highPriority);

        // A raw block's editing preview follows its whole semantic flow. Only
        // that active source fragment is needed when the remote tail is visible.
        bool MaterializeTrailingPreview(float scrollOffset, bool printMode,
            std::uint64_t embeddedGeneration);

    private:
        bool RequestEmbeddedAt(
            float documentTop,
            float scrollOffset,
            bool printMode) const;

        detail::EditorRenderFrame const& frame;
        EditorRenderResources& resources;
        EditorDocumentBlockPreparer& blockPreparer;
        EditorInlineImageRenderer& inlineImages;
        folia::platform::editor::EditorScrollState& scrollState;
        std::unique_ptr<EditorPreparedDocument>& preparedDocument;
        static constexpr folia::EditorViewportPolicy viewportPolicy{};
    };
}
