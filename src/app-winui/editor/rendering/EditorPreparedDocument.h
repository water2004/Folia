#pragma once

import folia.core.block_geometry;

#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorCaretPresentation.h"
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
        std::unordered_set<std::size_t> embeddedBlocks;
        std::unordered_set<std::size_t> layoutBlocks;
        float lastViewportOffset = 0.0f;
        bool hasLastViewportOffset = false;
    };

}
