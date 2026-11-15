#pragma once

import folia.core.block_geometry;

#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorCaretPresentation.h"
#include "editor/rendering/EditorFlowLayoutContext.h"
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorInlineImageRenderer.h"
#include "editor/rendering/EditorTableBlockRenderer.h"

namespace winrt::Folia
{
    using folia::BlockGeometryIndex;

    struct EditorPreparedDocument
    {
        using MathPreview = EditorPreparedMathPreview;

        struct MermaidDraw
        {
            std::uint32_t displayStart = 0;
            MermaidSvg svg;
            float width = 0.0f;
            float height = 0.0f;
            float lineHeight = 0.0f;
            float advance = 0.0f;
        };

        struct MermaidPreview
        {
            MermaidSvg svg;
            float width = 0.0f;
            float height = 0.0f;
        };

        struct Block
        {
            DisplayInlineText display;
            EditorCaretPresentationState caretPresentation;
            ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
            folia::platform::editor::EditorLayoutLines layoutLines;
            std::shared_ptr<const folia::platform::editor::EditorTextPaintPlan> paintPlan;
            std::vector<EditorInlineImageRenderer::ImageDraw> images;
            std::vector<MathPreview> mathPreviews;
            std::vector<MermaidDraw> mermaids;
            std::vector<MermaidPreview> mermaidPreviews;
            std::optional<EditorTableBlockRenderer::PreparedTable> table;
            std::vector<folia::NodeId> owners;
            float textTop = 0.0f;
            float textHeight = 0.0f;
            float previewHeight = 0.0f;
            float height = 0.0f;
            folia::NodeId sourceId{};
            std::uint64_t presentationKey = 0;
            bool sourceMode = false;
            bool code = false;
            bool containsMath = false;
            bool containsImage = false;
            bool containsMermaid = false;
            bool embeddedRequested = false;
            bool pendingImage = false;
            bool pendingMermaid = false;
            bool pendingMath = false;
            // Geometry survives eviction; valid only describes resident visual
            // content. A measured fragment must not fall back to estimated
            // character widths when it scrolls out of the working set.
            bool geometryMeasured = false;
            bool hasFlowContext = false;
            EditorFlowLayoutContext intrinsicFlow;
            EditorFlowLayoutContext appliedFlow;
            bool valid = false;
            std::uint64_t embeddedGeneration = 0;
            std::uint64_t remoteImageGeneration = 0;

            // Drop heavyweight DirectWrite/D2D/image state while retaining
            // exact measured geometry and stable block metadata. PDF export
            // uses this after each page so its working set is page-local.
            void ReleaseVisualContent()
            {
                auto measuredHeight = height;
                auto retainedSourceId = sourceId;
                auto retainedPresentationKey = presentationKey;
                auto retainedOwners = std::move(owners);
                auto retainedSourceMode = sourceMode;
                auto retainedCode = code;
                auto retainedContainsMath = containsMath;
                auto retainedContainsImage = containsImage;
                auto retainedPendingImage = pendingImage;
                auto retainedContainsMermaid = containsMermaid;
                auto retainedGeometryMeasured = geometryMeasured;
                auto retainedHasFlowContext = hasFlowContext;
                auto retainedIntrinsicFlow = intrinsicFlow;
                auto retainedAppliedFlow = appliedFlow;
                auto retainedPreviewHeight = previewHeight;
                *this = {};
                height = measuredHeight;
                sourceId = retainedSourceId;
                presentationKey = retainedPresentationKey;
                owners = std::move(retainedOwners);
                sourceMode = retainedSourceMode;
                code = retainedCode;
                containsMath = retainedContainsMath;
                containsImage = retainedContainsImage;
                pendingImage = retainedPendingImage;
                containsMermaid = retainedContainsMermaid;
                geometryMeasured = retainedGeometryMeasured;
                hasFlowContext = retainedHasFlowContext;
                intrinsicFlow = retainedIntrinsicFlow;
                appliedFlow = retainedAppliedFlow;
                previewHeight = retainedPreviewHeight;
            }
        };

        std::uint64_t modelRevision = 0;
        std::uint64_t completionGeneration = 0;
        folia::TextSelection selection{};
        float documentWidth = 0.0f;
        float totalHeight = 0.0f;
        std::uint64_t themeRevision = 0;
        std::vector<Block> blocks;
        BlockGeometryIndex geometry;
        std::unordered_map<std::uint64_t, std::size_t> ownerBlockIndex;
        // Work counter for native regression gates. Count records visited,
        // including no-op lookups, so a hidden full-owner pass is observable.
        std::size_t ownerIndexEntriesVisited = 0;
        std::unordered_set<std::size_t> embeddedBlocks;
        std::unordered_set<std::size_t> layoutBlocks;
        struct FlowGroup
        {
            std::size_t begin = 0;
            std::size_t end = 0;
            EditorFlowLayoutSummary summary;
        };
        // Only semantic roots with multiple physical fragments need a shared
        // context. A local edit changes two small counters, not every owner.
        std::unordered_map<std::uint64_t, FlowGroup> flowGroups;
        std::unordered_set<std::uint64_t> pendingFlowGroups;
        bool flowContextsChanged = false;
        struct TrailingPreview
        {
            folia::NodeId owner{};
            std::size_t sourceIndex = 0;
            std::size_t targetIndex = 0;
            float height = 0.0f;
            bool resident = false;
            bool pending = false;
            std::uint64_t embeddedGeneration = 0;
            std::vector<MathPreview> mathPreviews;
            std::vector<MermaidPreview> mermaidPreviews;
        };
        // At most the active raw-source owner contributes a trailing editing
        // preview. Background preparation keeps only its identity and height.
        std::optional<TrailingPreview> trailingPreview;

        float GeometryHeight(std::size_t index) const
        {
            auto height = blocks[index].height;
            if (trailingPreview)
            {
                if (index == trailingPreview->sourceIndex) height -= blocks[index].previewHeight;
                if (index == trailingPreview->targetIndex) height += trailingPreview->height;
            }
            return height;
        }

        void ResetTrailingPreview()
        {
            if (!trailingPreview) return;
            if (geometry.Initialized())
            {
                for (auto index : {trailingPreview->sourceIndex, trailingPreview->targetIndex})
                    if (index < blocks.size()) geometry.UpdateHeight(index, blocks[index].height);
            }
            trailingPreview.reset();
        }
        float lastViewportOffset = 0.0f;
        bool hasLastViewportOffset = false;
    };

}
