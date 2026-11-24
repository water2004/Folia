#pragma once

import folia.markdown.text_edit;
import folia.platform.editor_scroll_state;
import folia.core.viewport_plan;

#include "editor/rendering/layout/EditorDocumentBlockPreparer.h"
#include "editor/rendering/media/EditorEmbeddedContentUpdater.h"
#include "editor/rendering/layout/EditorPreparedDocumentCache.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"
#include "editor/rendering/layout/EditorPreparedDocumentRetention.h"
#include "editor/rendering/EditorRenderResources.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/layout/EditorViewportMaterializer.h"
#include "editor/session/EditorRenderFrame.h"

namespace winrt::Folia
{
    struct EditorDocumentPreparationResult
    {
        float scrollOffset = 0.0f;
        folia::EditorIndexRange visible;
        bool invalidateRequested = false;
    };

    // Executes the platform preparation plan against WinUI render resources.
    // Planning stays in pure platform modules; this class owns only the cache
    // reconciliation and resource-materialization side effects.
    class EditorDocumentPreparationPass final
    {
    public:
        EditorDocumentPreparationPass(
            detail::EditorRenderFrame const& frame,
            EditorRenderResources& resources,
            EditorStyleSheet const& styleSheet,
            EditorInlineImageRenderer& inlineImages,
            EditorDocumentBlockPreparer& blockPreparer,
            folia::platform::editor::EditorScrollState& scrollState,
            std::unique_ptr<EditorPreparedDocument>& preparedDocument,
            folia::TextSelection selection,
            float documentWidth,
            std::uint64_t themeRevision,
            std::uint64_t embeddedGeneration,
            std::uint64_t remoteImageGeneration,
            float scrollOffset,
            bool printMode);

        EditorDocumentPreparationResult Prepare();

    private:
        detail::EditorRenderFrame const& frame;
        EditorPreparedDocumentCache cache;
        EditorViewportMaterializer viewportMaterializer;
        EditorEmbeddedContentUpdater embeddedContentUpdater;
        EditorPreparedDocumentRetention retention;
        folia::platform::editor::EditorScrollState& scrollState;
        EditorRenderResources& resources;
        std::unique_ptr<EditorPreparedDocument>& preparedDocument;
        folia::TextSelection selection;
        float documentWidth = 0.0f;
        std::uint64_t themeRevision = 0;
        std::uint64_t embeddedGeneration = 0;
        std::uint64_t remoteImageGeneration = 0;
        float scrollOffset = 0.0f;
        bool printMode = false;
        bool invalidateRequested = false;
        static constexpr folia::EditorViewportPolicy viewportPolicy{};
    };
}
