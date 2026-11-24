#pragma once

import folia.markdown.text_edit;

#include "editor/rendering/layout/EditorDocumentBlockPreparer.h"
#include "editor/rendering/media/EditorInlineImageRenderer.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/session/EditorRenderFrame.h"

namespace winrt::Folia
{
    // Reconciles the retained render cache with a new render model and owns
    // construction of the geometry/owner indexes derived from that cache.
    // Viewport scheduling and embedded-resource work intentionally live in
    // separate preparation stages.
    class EditorPreparedDocumentCache final
    {
    public:
        EditorPreparedDocumentCache(
            detail::EditorRenderFrame const& frame,
            EditorStyleSheet const& styleSheet,
            EditorInlineImageRenderer& inlineImages,
            EditorDocumentBlockPreparer& blockPreparer,
            std::unique_ptr<EditorPreparedDocument>& preparedDocument);

        void Reconcile(
            folia::TextSelection selection,
            float documentWidth,
            std::uint64_t themeRevision);
        void EnsureGeometry(
            std::stop_token stop = {},
            std::function<void(std::size_t, std::size_t)> progress = {});
        void ReconcileTrailingPreview();

    private:
        void RemoveOwners(EditorPreparedDocument::Block const& block, std::size_t index);
        void MeasureGeometry(std::size_t index,
            std::optional<EditorFlowLayoutContext> flowContext = {});
        EditorPreparedDocument::FlowGroup* FindFlowGroup(std::size_t index);
        void BuildFlowGroups();
        void ReconcileFlowContexts(std::stop_token stop);
        void InitializeGeometry(
            std::stop_token stop,
            std::function<void(std::size_t, std::size_t)> const& progress);

        detail::EditorRenderFrame const& frame;
        EditorStyleSheet const& styleSheet;
        EditorInlineImageRenderer& inlineImages;
        EditorDocumentBlockPreparer& blockPreparer;
        std::unique_ptr<EditorPreparedDocument>& preparedDocument;
    };
}
