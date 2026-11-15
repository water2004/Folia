#include "pch.h"
#include "editor/rendering/EditorDocumentPreparationPass.h"

namespace winrt::Folia
{
    using folia::BuildEditorViewportPlan;

    namespace
    {
        struct SourceScrollAnchor
        {
            folia::TextPosition position;
            float screenY = 0.0f;
        };

        std::optional<SourceScrollAnchor> CaptureSourceScrollAnchor(
            EditorPreparedDocument::Block const& block,
            float documentTop,
            float scrollOffset,
            folia::RenderModel const& model)
        {
            if (!block.valid) return {};
            std::optional<SourceScrollAnchor> result;
            auto closest = (std::numeric_limits<float>::max)();
            auto visit = [&](folia::platform::editor::EditorLayoutLines const& lines, float textTop)
            {
                if (!lines) return;
                for (auto const& line : *lines)
                {
                    auto top = documentTop + textTop + line.rect.top;
                    auto bottom = documentTop + textTop + line.rect.bottom;
                    if (bottom <= scrollOffset) continue;
                    auto distance = (std::max)(0.0f, top - scrollOffset);
                    if (distance >= closest) continue;
                    for (auto const& span : line.SourceSpans())
                    {
                        if (!model.editable_index.contains(span.container_id.v)) continue;
                        result = SourceScrollAnchor{
                            {span.container_id, span.source_range.start, folia::TextAffinity::Downstream},
                            top - scrollOffset,
                        };
                        closest = distance;
                        break;
                    }
                }
            };
            if (block.table)
                for (auto const& cell : block.table->visual.cells)
                    visit(cell.layoutLines, cell.textOrigin.y);
            else
                visit(block.layoutLines, block.textTop);
            return result;
        }

        std::optional<float> SourceScrollAnchorTop(
            EditorPreparedDocument::Block const& block,
            folia::TextPosition position)
        {
            auto locate = [&](IDWriteTextLayout* layout, EditorDisplayMapping const& mapping,
                              float textTop) -> std::optional<float>
            {
                if (!layout || mapping.empty()) return {};
                bool containsOwner = false;
                mapping.for_each_run(0, mapping.size(), [&](auto, auto, auto first, auto)
                {
                    containsOwner = first.container_id == position.container_id;
                    return !containsOwner;
                });
                if (!containsOwner) return {};
                auto displayPosition = folia::platform::editor::DisplayPositionForSource(mapping, position);
                FLOAT x = 0.0f, y = 0.0f;
                DWRITE_HIT_TEST_METRICS hit{};
                if (FAILED(layout->HitTestTextPosition(static_cast<UINT32>(displayPosition),
                        FALSE, &x, &y, &hit))) return {};
                return textTop + y;
            };
            if (block.table)
            {
                for (auto const& cell : block.table->visual.cells)
                    if (auto top = locate(cell.layout.Get(), cell.displayToSource, cell.textOrigin.y))
                        return top;
                return {};
            }
            return locate(block.layout.Get(), block.display.displayToSource, block.textTop);
        }
    }

    EditorDocumentPreparationPass::EditorDocumentPreparationPass(
        detail::EditorRenderFrame const& valueFrame,
        EditorRenderResources& valueResources,
        EditorStyleSheet const& valueStyleSheet,
        EditorInlineImageRenderer& valueInlineImages,
        EditorDocumentBlockPreparer& valueBlockPreparer,
        folia::platform::editor::EditorScrollState& valueScrollState,
        std::unique_ptr<EditorPreparedDocument>& valuePreparedDocument,
        folia::TextSelection valueSelection,
        float valueDocumentWidth,
        std::uint64_t valueThemeRevision,
        std::uint64_t valueEmbeddedGeneration,
        std::uint64_t valueRemoteImageGeneration,
        float valueScrollOffset,
        bool valuePrintMode)
        : frame(valueFrame),
          cache(
              valueFrame,
              valueStyleSheet,
              valueInlineImages,
              valueBlockPreparer,
              valuePreparedDocument),
          viewportMaterializer(
              valueFrame,
              valueResources,
              valueBlockPreparer,
              valueScrollState,
              valuePreparedDocument),
          embeddedContentUpdater(
              valueFrame,
              valueResources,
              valueInlineImages,
              valueBlockPreparer,
              valuePreparedDocument),
          retention(
              valueFrame,
              valueResources,
              valueInlineImages,
              valuePreparedDocument),
          scrollState(valueScrollState),
          resources(valueResources),
          preparedDocument(valuePreparedDocument),
          selection(valueSelection),
          documentWidth(valueDocumentWidth),
          themeRevision(valueThemeRevision),
          embeddedGeneration(valueEmbeddedGeneration),
          remoteImageGeneration(valueRemoteImageGeneration),
          scrollOffset(valueScrollOffset),
          printMode(valuePrintMode)
    {
    }

    EditorDocumentPreparationResult EditorDocumentPreparationPass::Prepare()
    {
        // A fragment can hold many source owners and wrapped lines. Preserve
        // the first visible source boundary and its screen Y, so reflow above
        // it inside the same fragment cannot move the user's reading position.
        std::optional<SourceScrollAnchor> sourceAnchor;
        std::optional<folia::NodeId> anchorOwner;
        std::size_t anchorIndex = 0;
        float anchorTop = 0.0f;
        auto initialScrollOffset = scrollOffset;
        if (!printMode && preparedDocument && preparedDocument->geometry.Initialized()
            && !preparedDocument->blocks.empty())
        {
            anchorIndex = (std::min)(preparedDocument->geometry.FirstIntersecting(scrollOffset),
                preparedDocument->blocks.size() - 1);
            auto const& anchor = preparedDocument->blocks[anchorIndex];
            anchorOwner = anchor.sourceId;
            for (auto owner : anchor.owners)
                if (frame.renderModel.editable_index.contains(owner.v))
                {
                    anchorOwner = owner;
                    break;
                }
            anchorTop = preparedDocument->geometry.At(anchorIndex).top;
            sourceAnchor = CaptureSourceScrollAnchor(anchor, anchorTop, scrollOffset, frame.renderModel);
        }
        cache.Reconcile(selection, documentWidth, themeRevision);
        cache.EnsureGeometry();
        auto restoreAnchor = [&]
        {
            if (!anchorOwner || preparedDocument->blocks.empty()) return;
            std::optional<float> anchoredOffset;
            if (sourceAnchor)
            {
                auto found = preparedDocument->ownerBlockIndex.find(sourceAnchor->position.container_id.v);
                if (found != preparedDocument->ownerBlockIndex.end())
                {
                    auto index = found->second;
                    viewportMaterializer.EnsurePrepared(index, true, true);
                    if (auto localTop = SourceScrollAnchorTop(
                            preparedDocument->blocks[index], sourceAnchor->position))
                        anchoredOffset = preparedDocument->geometry.At(index).top
                            + *localTop - sourceAnchor->screenY;
                }
            }
            auto found = preparedDocument->ownerBlockIndex.find(anchorOwner->v);
            auto currentIndex = found != preparedDocument->ownerBlockIndex.end()
                ? found->second : (std::min)(anchorIndex, preparedDocument->blocks.size() - 1);
            auto target = anchoredOffset.value_or(initialScrollOffset
                + preparedDocument->geometry.At(currentIndex).top - anchorTop);
            auto shift = target - scrollOffset;
            if (shift != 0.0f)
            {
                scrollState.Shift(shift, (std::numeric_limits<float>::max)());
                scrollOffset = scrollState.Offset();
            }
        };
        restoreAnchor();
        auto viewport = viewportMaterializer.Materialize(scrollOffset, printMode);
        scrollOffset = viewport.scrollOffset;
        invalidateRequested = invalidateRequested || viewport.invalidateRequested;
        invalidateRequested = embeddedContentUpdater.Update(
                scrollOffset,
                printMode,
                viewport.scrollingForward,
                viewport.viewportActive,
                embeddedGeneration,
                remoteImageGeneration)
            || invalidateRequested;
        auto offsetBeforeEmbeddedAnchor = scrollOffset;
        restoreAnchor();
        if (scrollOffset != offsetBeforeEmbeddedAnchor) invalidateRequested = true;
        retention.ReleaseOutside(scrollOffset, printMode);

        auto paintPlan = BuildEditorViewportPlan(
            preparedDocument->geometry,
            scrollOffset,
            resources.surfaceHeightDip,
            printMode,
            true,
            viewportPolicy);
        return {
            .scrollOffset = scrollOffset,
            .visible = paintPlan.visible,
            .invalidateRequested = invalidateRequested,
        };
    }

}
