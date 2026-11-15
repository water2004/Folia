#include "pch.h"
#include "editor/rendering/EditorEmbeddedContentUpdater.h"

namespace winrt::Folia
{
    using folia::BuildEditorPrioritizedTraversal;
    using folia::BuildEditorViewportPlan;

    EditorEmbeddedContentUpdater::EditorEmbeddedContentUpdater(
        detail::EditorRenderFrame const& valueFrame,
        EditorRenderResources& valueResources,
        EditorInlineImageRenderer& valueInlineImages,
        EditorDocumentBlockPreparer& valueBlockPreparer,
        std::unique_ptr<EditorPreparedDocument>& valuePreparedDocument)
        : frame(valueFrame),
          resources(valueResources),
          inlineImages(valueInlineImages),
          blockPreparer(valueBlockPreparer),
          preparedDocument(valuePreparedDocument)
    {
    }

    bool EditorEmbeddedContentUpdater::Update(
        float scrollOffset,
        bool printMode,
        bool scrollingForward,
        bool viewportActive,
        std::uint64_t embeddedGeneration,
        std::uint64_t remoteImageGeneration)
    {
        auto embeddedPlan = BuildEditorViewportPlan(
            preparedDocument->geometry,
            scrollOffset,
            resources.surfaceHeightDip,
            printMode,
            true,
            viewportPolicy);
        auto geometryChanged = false;
        auto deadline = std::chrono::steady_clock::now()
            + (printMode ? std::chrono::hours{24} : std::chrono::milliseconds{1});
        constexpr std::size_t maximumRefreshes = 4;
        auto refreshes = std::size_t{0};
        auto workRemaining = false;
        auto withinBudget = [&]
        {
            return printMode || refreshes == 0
                || (refreshes < maximumRefreshes
                    && std::chrono::steady_clock::now() < deadline);
        };
        auto refreshIndex = [&](std::size_t index)
        {
            auto const& block = frame.renderModel.blocks[index];
            if (block.kind == folia::RenderBlockKind::ThematicBreak) return true;
            auto& prepared = preparedDocument->blocks[index];
            if (!prepared.valid) return true;
            auto refreshForImages = prepared.containsImage
                && prepared.embeddedRequested
                && (prepared.remoteImageGeneration != remoteImageGeneration
                    || (prepared.pendingImage
                        && prepared.embeddedGeneration != embeddedGeneration));
            auto highPriority = index >= embeddedPlan.visible.begin
                && index < embeddedPlan.visible.end;
            auto refreshForMath = prepared.pendingMath && (printMode || highPriority)
                && (prepared.embeddedGeneration != embeddedGeneration || viewportActive);
            auto refreshForMermaid = prepared.containsMermaid
                && prepared.embeddedRequested
                && prepared.pendingMermaid
                && prepared.embeddedGeneration != embeddedGeneration;
            auto enteredEmbeddedBand = !prepared.embeddedRequested
                && (prepared.containsImage
                    || prepared.containsMermaid)
                && (!viewportActive || highPriority);
            if (!refreshForMath
                && !refreshForImages
                && !refreshForMermaid
                && !enteredEmbeddedBand)
                return true;
            if (!withinBudget())
            {
                workRemaining = true;
                return false;
            }
            auto previousHeight = prepared.height;
            // Compact geometry can outlive the source projection. A retained
            // visual block does not imply that its render fragment is resident.
            if (!block.materialized && frame.materializeBlocks)
                frame.materializeBlocks(index, index + 1);
            auto previousIntrinsic = prepared.intrinsicFlow;
            auto flowContext = prepared.hasFlowContext
                ? std::optional{prepared.appliedFlow} : std::nullopt;
            prepared = blockPreparer.Prepare(block, true, highPriority, flowContext);
            if (flowContext && prepared.intrinsicFlow != previousIntrinsic)
                preparedDocument->flowContextsChanged = true;
            ++refreshes;
            preparedDocument->layoutBlocks.insert(index);
            preparedDocument->embeddedBlocks.insert(index);
            if (prepared.height != previousHeight)
            {
                preparedDocument->geometry.UpdateHeight(index, preparedDocument->GeometryHeight(index));
                geometryChanged = true;
            }
            return true;
        };
        auto traversal = BuildEditorPrioritizedTraversal(
            embeddedPlan.visible,
            embeddedPlan.embedded,
            scrollingForward);
        for (std::size_t segmentIndex = 0;
             segmentIndex < traversal.count && !workRemaining;
             ++segmentIndex)
        {
            auto const& segment = traversal.segments[segmentIndex];
            if (segment.reverse)
            {
                for (auto index = segment.range.end; index > segment.range.begin;)
                {
                    --index;
                    if (!refreshIndex(index)) break;
                }
            }
            else
            {
                for (auto index = segment.range.begin; index < segment.range.end; ++index)
                    if (!refreshIndex(index)) break;
            }
        }

        auto activeEmbedded = std::vector<std::size_t>(
            preparedDocument->embeddedBlocks.begin(),
            preparedDocument->embeddedBlocks.end());
        for (auto index : activeEmbedded)
        {
            if (printMode) break;
            if (index >= frame.renderModel.blocks.size()) continue;
            auto placement = preparedDocument->geometry.At(index);
            if (placement.bottom >= embeddedPlan.embeddedKeepTop
                && placement.top <= embeddedPlan.embeddedKeepBottom) continue;
            auto const& block = frame.renderModel.blocks[index];
            auto& prepared = preparedDocument->blocks[index];
            auto imageSources = EditorDocumentBlockPreparer::ImageSources(prepared);
            auto previousHeight = prepared.height;
            if (!block.materialized && frame.materializeBlocks)
                frame.materializeBlocks(index, index + 1);
            auto previousIntrinsic = prepared.intrinsicFlow;
            auto flowContext = prepared.hasFlowContext
                ? std::optional{prepared.appliedFlow} : std::nullopt;
            prepared = blockPreparer.Prepare(block, false, false, flowContext);
            if (flowContext && prepared.intrinsicFlow != previousIntrinsic)
                preparedDocument->flowContextsChanged = true;
            preparedDocument->layoutBlocks.insert(index);
            preparedDocument->embeddedBlocks.erase(index);
            for (auto const& source : imageSources) inlineImages.ReleaseGif(source);
            if (prepared.height != previousHeight)
            {
                preparedDocument->geometry.UpdateHeight(index, preparedDocument->GeometryHeight(index));
                geometryChanged = true;
            }
        }
        if (geometryChanged)
            preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
        return workRemaining || preparedDocument->flowContextsChanged;
    }
}
