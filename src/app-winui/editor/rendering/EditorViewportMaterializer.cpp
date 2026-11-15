#include "pch.h"
#include "editor/rendering/EditorViewportMaterializer.h"

namespace winrt::Folia
{
    using folia::BuildEditorViewportPlan;
    using folia::EditorViewportMoved;

    EditorViewportMaterializer::EditorViewportMaterializer(
        detail::EditorRenderFrame const& valueFrame,
        EditorRenderResources& valueResources,
        EditorDocumentBlockPreparer& valueBlockPreparer,
        EditorInlineImageRenderer& valueInlineImages,
        folia::platform::editor::EditorScrollState& valueScrollState,
        std::unique_ptr<EditorPreparedDocument>& valuePreparedDocument)
        : frame(valueFrame),
          resources(valueResources),
          blockPreparer(valueBlockPreparer),
          inlineImages(valueInlineImages),
          scrollState(valueScrollState),
          preparedDocument(valuePreparedDocument)
    {
    }

    bool EditorViewportMaterializer::RequestEmbeddedAt(
        float documentTop,
        float scrollOffset,
        bool printMode) const
    {
        if (printMode) return true;
        auto screenTop = documentTop - scrollOffset;
        return screenTop < resources.surfaceHeightDip + viewportPolicy.embeddedAfter
            && screenTop > -viewportPolicy.embeddedBefore;
    }

    bool EditorViewportMaterializer::EnsurePrepared(
        std::size_t index, bool requestEmbedded, bool highPriority)
    {
        if (index >= frame.renderModel.blocks.size()) return false;
        if (!frame.renderModel.blocks[index].materialized && frame.materializeBlocks)
            frame.materializeBlocks(index, index + 1);
        auto const& block = frame.renderModel.blocks[index];
        auto& prepared = preparedDocument->blocks[index];
        if (block.kind == folia::RenderBlockKind::ThematicBreak || prepared.valid) return false;
        auto previousHeight = preparedDocument->GeometryHeight(index);
        auto previousIntrinsic = prepared.intrinsicFlow;
        auto flowContext = prepared.hasFlowContext
            ? std::optional{prepared.appliedFlow} : std::nullopt;
        prepared = blockPreparer.Prepare(block, requestEmbedded, highPriority, flowContext);
        if (flowContext && prepared.intrinsicFlow != previousIntrinsic)
            preparedDocument->flowContextsChanged = true;
        preparedDocument->layoutBlocks.insert(index);
        if (prepared.embeddedRequested
            && (prepared.containsMath || prepared.containsImage || prepared.containsMermaid))
            preparedDocument->embeddedBlocks.insert(index);
        else
            preparedDocument->embeddedBlocks.erase(index);
        for (auto owner : prepared.owners)
            preparedDocument->ownerBlockIndex[owner.v] = index;
        auto height = preparedDocument->GeometryHeight(index);
        if (height == previousHeight) return false;
        preparedDocument->geometry.UpdateHeight(index, height);
        preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
        return true;
    }

    bool EditorViewportMaterializer::MaterializeTrailingPreview(
        float scrollOffset, bool printMode, std::uint64_t embeddedGeneration)
    {
        if (printMode || !preparedDocument || !preparedDocument->trailingPreview
            || !preparedDocument->geometry.Initialized()) return false;
        auto& preview = *preparedDocument->trailingPreview;
        if (preview.sourceIndex >= preparedDocument->blocks.size()
            || preview.targetIndex >= preparedDocument->blocks.size()) return false;
        auto plan = BuildEditorViewportPlan(preparedDocument->geometry, scrollOffset,
            resources.surfaceHeightDip, false, true, viewportPolicy);
        if (!plan.retention.Contains(preview.targetIndex))
        {
            preview.mathPreviews = {};
            preview.mermaidPreviews = {};
            preview.resident = false;
        }
        if (!plan.visible.Contains(preview.targetIndex)
            || (preview.resident
                && (!preview.pending || preview.embeddedGeneration == embeddedGeneration)))
            return false;

        auto sourceIndex = preview.sourceIndex;
        auto& source = preparedDocument->blocks[sourceIndex];
        auto previousPreviewHeight = preview.height;
        // Geometry-only background preparation did not queue Mermaid. A prior
        // pending request also needs a fresh projection after its completion.
        if (!source.embeddedRequested || preview.pending) source.valid = false;
        auto geometryChanged = EnsurePrepared(sourceIndex, true, true);
        preview.mathPreviews.clear();
        for (auto const& item : source.mathPreviews)
            if (!item.floating) preview.mathPreviews.push_back(item);
        preview.mermaidPreviews = source.mermaidPreviews;
        preview.resident = source.valid && source.embeddedRequested;
        preview.pending = source.pendingMath || source.pendingMermaid;
        preview.embeddedGeneration = embeddedGeneration;
        auto changed = geometryChanged || previousPreviewHeight != source.previewHeight
            || preparedDocument->flowContextsChanged;

        if (!plan.retention.Contains(sourceIndex))
        {
            auto imageSources = EditorDocumentBlockPreparer::ImageSources(source);
            source.ReleaseVisualContent();
            preparedDocument->layoutBlocks.erase(sourceIndex);
            preparedDocument->embeddedBlocks.erase(sourceIndex);
            for (auto const& imageSource : imageSources) inlineImages.ReleaseGif(imageSource);
            if (frame.releaseBlocksOutside)
                frame.releaseBlocksOutside(plan.retention.begin, plan.retention.end);
        }
        return changed;
    }

    EditorViewportMaterializationResult EditorViewportMaterializer::Materialize(
        float scrollOffset,
        bool printMode)
    {
        EditorViewportMaterializationResult result{
            .scrollOffset = scrollOffset,
        };
        if (frame.renderModel.blocks.empty()) return result;

        auto anchorIndex = (std::min)(
            preparedDocument->geometry.FirstIntersecting(scrollOffset),
            frame.renderModel.blocks.size() - 1);
        auto anchorTop = preparedDocument->geometry.At(anchorIndex).top;
        auto geometryChanged = false;
        auto needsAnotherFrame = false;
        auto preparedThisFrame = std::size_t{0};
        auto deadline = std::chrono::steady_clock::now()
            + (printMode ? std::chrono::hours{24} : std::chrono::milliseconds{2});

        auto prepareIndex = [&](std::size_t index, bool highPriority)
        {
            auto placement = preparedDocument->geometry.At(index);
            auto const& block = frame.renderModel.blocks[index];
            if (block.kind == folia::RenderBlockKind::ThematicBreak
                || preparedDocument->blocks[index].valid) return;
            geometryChanged = EnsurePrepared(index,
                RequestEmbeddedAt(placement.top, scrollOffset, printMode),
                highPriority) || geometryChanged;
            ++preparedThisFrame;
        };
        auto withinBudget = [&]
        {
            return printMode || preparedThisFrame == 0
                || std::chrono::steady_clock::now() < deadline;
        };
        auto prepareForward = [&](std::size_t begin, std::size_t end, bool highPriority)
        {
            constexpr std::size_t materializationBatch = 16;
            for (auto cursor = begin; cursor < end;)
            {
                if (!withinBudget()) { needsAnotherFrame = true; break; }
                auto batchEnd = (std::min)(end, cursor + materializationBatch);
                if (frame.materializeBlocks) frame.materializeBlocks(cursor, batchEnd);
                for (; cursor < batchEnd; ++cursor)
                {
                    prepareIndex(cursor, highPriority);
                    if (!withinBudget() && cursor + 1 < end)
                    {
                        ++cursor;
                        needsAnotherFrame = true;
                        break;
                    }
                }
                if (needsAnotherFrame) break;
            }
        };
        auto prepareBackward = [&](std::size_t begin, std::size_t end, bool highPriority)
        {
            constexpr std::size_t materializationBatch = 16;
            auto cursor = end;
            while (cursor > begin)
            {
                if (!withinBudget()) { needsAnotherFrame = true; break; }
                auto batchBegin = cursor > begin + materializationBatch
                    ? cursor - materializationBatch
                    : begin;
                if (frame.materializeBlocks) frame.materializeBlocks(batchBegin, cursor);
                while (cursor > batchBegin)
                {
                    --cursor;
                    prepareIndex(cursor, highPriority);
                    if (!withinBudget() && cursor > begin)
                    {
                        needsAnotherFrame = true;
                        break;
                    }
                }
                if (needsAnotherFrame) break;
            }
        };

        auto viewportMoved = EditorViewportMoved(
            preparedDocument->hasLastViewportOffset,
            preparedDocument->lastViewportOffset,
            scrollOffset);
        result.viewportActive = viewportMoved || scrollState.Animating();
        result.scrollingForward = !preparedDocument->hasLastViewportOffset
            || scrollOffset >= preparedDocument->lastViewportOffset;
        auto viewportPlan = BuildEditorViewportPlan(
            preparedDocument->geometry,
            scrollOffset,
            resources.surfaceHeightDip,
            printMode,
            result.scrollingForward,
            viewportPolicy);
        prepareForward(viewportPlan.visible.begin, viewportPlan.visible.end, true);
        if (!printMode
            && !result.viewportActive
            && !needsAnotherFrame
            && !viewportPlan.prefetch.Empty())
        {
            if (result.scrollingForward)
                prepareForward(viewportPlan.prefetch.begin, viewportPlan.prefetch.end, false);
            else
                prepareBackward(viewportPlan.prefetch.begin, viewportPlan.prefetch.end, false);
        }
        if (!printMode)
        {
            preparedDocument->lastViewportOffset = scrollOffset;
            preparedDocument->hasLastViewportOffset = true;
        }

        if (geometryChanged)
        {
            preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
            if (!printMode && anchorIndex < preparedDocument->geometry.Size())
            {
                auto shift = preparedDocument->geometry.At(anchorIndex).top - anchorTop;
                if (shift != 0.0f)
                {
                    scrollState.Shift(shift, (std::numeric_limits<float>::max)());
                    result.scrollOffset = scrollState.Offset();
                }
            }
            needsAnotherFrame = !printMode;
        }
        result.invalidateRequested = needsAnotherFrame || preparedDocument->flowContextsChanged;
        return result;
    }
}
