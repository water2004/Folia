#include "pch.h"
#include "editor/rendering/layout/EditorPreparedDocumentCache.h"

import folia.core.preparation_plan;

namespace winrt::Folia
{
    using folia::BuildEditorPreparationInvalidationPlan;
    using folia::BlockGeometryIndex;
    using folia::EditorPreparationCacheView;

    EditorPreparedDocumentCache::EditorPreparedDocumentCache(
        detail::EditorRenderFrame const& valueFrame,
        EditorStyleSheet const& valueStyleSheet,
        EditorInlineImageRenderer& valueInlineImages,
        EditorDocumentBlockPreparer& valueBlockPreparer,
        std::unique_ptr<EditorPreparedDocument>& valuePreparedDocument)
        : frame(valueFrame),
          styleSheet(valueStyleSheet),
          inlineImages(valueInlineImages),
          blockPreparer(valueBlockPreparer),
          preparedDocument(valuePreparedDocument)
    {
    }

    void EditorPreparedDocument::UseNavigation(folia::RenderModel const& model)
    {
        editableOrder = model.editable_order;
        editableIndex = model.editable_index;
        ownerIntervals = model.owner_intervals;
        sourceRows = !model.blocks.empty() && model.blocks.front().source_mode;
    }

    std::optional<std::size_t> EditorPreparedDocument::FindOwnerBlock(folia::NodeId owner) const
    {
        if (auto rank = editableIndex.find(owner.v))
        {
            if (sourceRows) return rank->second;
            if (ownerIntervals) return folia::render_fragment_at(*ownerIntervals, rank->second);
        }
        if (auto association = structuralOwnerIndex.find(owner.v)) return association->second;
        return std::nullopt;
    }

    std::optional<folia::NodeId> EditorPreparedDocument::FirstSurvivingOwner(
        std::size_t index, folia::OwnerIndex const& current) const
    {
        if (sourceRows && index < editableOrder.size())
        {
            auto owner = editableOrder[index];
            if (current.contains(owner.v)) return owner;
        }
        if (ownerIntervals)
        {
            auto interval = std::ranges::lower_bound(*ownerIntervals, index, {},
                &folia::RenderOwnerInterval::fragment);
            if (interval != ownerIntervals->end() && interval->fragment == index)
                for (auto ordinal = interval->begin; ordinal < interval->end; ++ordinal)
                {
                    auto owner = editableOrder[ordinal];
                    if (current.contains(owner.v)) return owner;
                }
        }
        return std::nullopt;
    }

    void EditorPreparedDocumentCache::RemoveOwners(
        EditorPreparedDocument::Block const& block, std::size_t index)
    {
        auto& lookup = preparedDocument->structuralOwnerIndex;
        for (auto owner : block.structuralOwners)
        {
            ++preparedDocument->ownerIndexEntriesVisited;
            if (auto found = lookup.find(owner.v); found && found->second == index)
                lookup.erase(owner.v);
        }
    }

    void EditorPreparedDocument::RegisterStructuralOwners(std::size_t index)
    {
        auto& block = blocks[index];
        // Thematic breaks and source rows resolve through the model even when
        // they have no text layout. Geometry-only structural owners are extra.
        if (block.sourceId.v != 0 && !editableIndex.contains(block.sourceId.v)
            && std::ranges::find(block.structuralOwners, block.sourceId) == block.structuralOwners.end())
            block.structuralOwners.push_back(block.sourceId);
        for (auto owner : block.structuralOwners)
        {
            ++ownerIndexEntriesVisited;
            structuralOwnerIndex.set(owner.v, index);
        }
    }

    void EditorPreparedDocumentCache::Reconcile(
        folia::TextSelection selection,
        float documentWidth,
        std::uint64_t themeRevision)
    {
        EditorPreparationCacheView cache;
        std::optional<std::size_t> previousActiveBlock;
        std::optional<std::size_t> activeBlock;
        const auto caret = EditorPresentationCaret(selection);
        if (preparedDocument)
        {
            cache = {
                .available = true,
                .blockCount = preparedDocument->blocks.size(),
                .modelRevision = preparedDocument->modelRevision,
                .documentWidth = preparedDocument->documentWidth,
                .themeRevision = preparedDocument->themeRevision,
                .active = EditorPresentationCaret(preparedDocument->selection),
            };
            previousActiveBlock = preparedDocument->FindOwnerBlock(
                cache.active.container_id);
            activeBlock = preparedDocument->FindOwnerBlock(caret.container_id);
        }
        auto sourceMode = !frame.renderModel.blocks.empty()
            && frame.renderModel.blocks.front().source_mode;
        auto layoutAffectedByCaret = [&](std::optional<std::size_t> index) -> std::optional<std::size_t> {
            if (!index || !preparedDocument || cache.active == caret) return {};
            if (*index >= frame.renderModel.blocks.size() || *index >= preparedDocument->blocks.size()) return index;
            auto const& prepared = preparedDocument->blocks[*index];
            if (!prepared.valid) return index;
            if (!frame.renderModel.blocks[*index].materialized && frame.materializeBlocks)
                frame.materializeBlocks(*index, *index + 1);
            auto const& source = frame.renderModel.blocks[*index];
            if (!source.materialized || prepared.Visuals().caretPresentation != CaretPresentationState(
                    source, caret, frame.renderModel.editable_index)) return index;
            return {};
        };
        auto invalidationPlan = BuildEditorPreparationInvalidationPlan(
            cache,
            frame.renderModel.blocks.size(),
            frame.renderModel.revision,
            documentWidth,
            themeRevision,
            caret,
            frame.renderModel.incremental_update,
            sourceMode,
            frame.renderModel.changed_block_indices,
            layoutAffectedByCaret(previousActiveBlock),
            layoutAffectedByCaret(activeBlock));
        auto completionGeneration = frame.completion ? frame.completion->generation : 0;
        auto completionChanged = preparedDocument && preparedDocument->completionGeneration != completionGeneration;
        if (completionChanged && !invalidationPlan.rebuildAll) {
            for (auto index : {previousActiveBlock, activeBlock})
                if (index && std::ranges::find(invalidationPlan.invalidatedBlocks, *index) == invalidationPlan.invalidatedBlocks.end())
                    invalidationPlan.invalidatedBlocks.push_back(*index);
        }
        // An asynchronous embedded update may have replaced an intrinsic
        // context after the preceding frame's reconciliation. Refresh those
        // compact counts before subtracting a newly invalidated fragment.
        if (!invalidationPlan.rebuildAll && preparedDocument->flowContextsChanged)
            BuildFlowGroups();
        if (preparedDocument && (invalidationPlan.rebuildAll
            || invalidationPlan.modelChanged || invalidationPlan.activePositionChanged || completionChanged))
            preparedDocument->ResetTrailingPreview();

        if (invalidationPlan.rebuildAll)
        {
            auto previous = std::move(preparedDocument);
            preparedDocument = std::make_unique<EditorPreparedDocument>();
            preparedDocument->modelRevision = frame.renderModel.revision;
            preparedDocument->selection = selection;
            preparedDocument->documentWidth = documentWidth;
            preparedDocument->themeRevision = themeRevision;
            preparedDocument->blocks.resize(frame.renderModel.blocks.size());
            preparedDocument->UseNavigation(frame.renderModel);
            if (previous)
                preparedDocument->ownerIndexEntriesVisited = previous->ownerIndexEntriesVisited;
            if (previous
                && previous->documentWidth == documentWidth
                && previous->themeRevision == themeRevision)
            {
                preparedDocument->structuralOwnerIndex = std::move(previous->structuralOwnerIndex);
                std::vector<bool> retained(previous->blocks.size());
                std::vector<std::optional<std::size_t>> previousIndices(frame.renderModel.blocks.size());
                std::unordered_map<std::uint64_t, std::size_t> previousById;
                previousById.reserve(previous->blocks.size());
                for (std::size_t index = 0; index < previous->blocks.size(); ++index)
                {
                    auto const& block = previous->blocks[index];
                    if (block.sourceId.v != 0) previousById.emplace(block.sourceId.v, index);
                }
                for (std::size_t index = 0; index < frame.renderModel.blocks.size(); ++index)
                {
                    auto const& source = frame.renderModel.blocks[index];
                    auto found = previousById.find(source.id.v);
                    if (found == previousById.end()) continue;
                    auto& candidate = previous->blocks[found->second];
                    if (candidate.sourceMode != source.source_mode
                        || candidate.presentationKey != source.presentation_key) continue;
                    auto activeOwner = previousActiveBlock == found->second || activeBlock == found->second;
                    if (!source.source_mode && activeOwner && (completionChanged
                        || (invalidationPlan.activePositionChanged
                            && (!source.materialized || candidate.Visuals().caretPresentation != CaretPresentationState(
                                source, caret, frame.renderModel.editable_index))))) continue;
                    preparedDocument->blocks[index] = std::move(candidate);
                    retained[found->second] = true;
                    previousIndices[index] = found->second;
                    if (preparedDocument->blocks[index].valid
                        && source.kind != folia::RenderBlockKind::ThematicBreak)
                        preparedDocument->layoutBlocks.insert(index);
                }
                // Remove old associations before assigning changed indices:
                // a shifted survivor may reuse a removed fragment's slot.
                for (std::size_t index = 0; index < previous->blocks.size(); ++index)
                    if (!retained[index]) RemoveOwners(previous->blocks[index], index);
                for (std::size_t index = 0; index < previousIndices.size(); ++index)
                    if (previousIndices[index] && *previousIndices[index] != index)
                        preparedDocument->RegisterStructuralOwners(index);
            }
        }
        else
        {
            if (invalidationPlan.modelChanged) preparedDocument->UseNavigation(frame.renderModel);
            for (auto index : invalidationPlan.invalidatedBlocks)
            {
                auto const& source = frame.renderModel.blocks[index];
                auto& prepared = preparedDocument->blocks[index];
                auto imageSources = EditorDocumentBlockPreparer::ImageSources(prepared);
                auto group = FindFlowGroup(index);
                auto flowContext = group
                    ? std::optional{group->summary.Context()} : std::nullopt;
                if (group && prepared.hasFlowContext)
                    group->summary.Remove(prepared.intrinsicFlow);
                RemoveOwners(prepared, index);
                const bool residentTable = source.kind == folia::RenderBlockKind::Table && prepared.Visuals().table.has_value();
                if (residentTable)
                {
                    if (!source.materialized && frame.materializeBlocks)
                        frame.materializeBlocks(index, index + 1);
                    const auto requested = prepared.embeddedRequested;
                    auto& retained = *prepared.EnsureVisuals().table;
                    prepared = blockPreparer.Prepare(frame.renderModel.blocks[index], requested, true, flowContext, &retained);
                    preparedDocument->layoutBlocks.insert(index);
                    if (prepared.embeddedRequested && (prepared.containsMath || prepared.containsImage))
                        preparedDocument->embeddedBlocks.insert(index);
                    else preparedDocument->embeddedBlocks.erase(index);
                }
                else
                {
                    prepared = {};
                    MeasureGeometry(index, flowContext);
                }
                if (group)
                {
                    if (prepared.hasFlowContext) group->summary.Add(prepared.intrinsicFlow);
                    if (group->summary.Context() != *flowContext)
                        preparedDocument->pendingFlowGroups.insert(
                            frame.renderModel.fragments[index].source_root.v);
                }
                if (preparedDocument->geometry.Initialized())
                {
                    preparedDocument->geometry.Update(index, {
                        source.block_style.margin_top,
                        prepared.height,
                        source.block_style.margin_bottom
                            + (source.source_mode || source.fragment_continues ? 0.0f : styleSheet.blockGap),
                    });
                }
                preparedDocument->RegisterStructuralOwners(index);
                for (auto const& imageSource : imageSources)
                    inlineImages.ReleaseGif(imageSource);
            }
            if (invalidationPlan.modelChanged)
                preparedDocument->modelRevision = frame.renderModel.revision;
            if (preparedDocument->geometry.Initialized())
                preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
        }
        preparedDocument->selection = selection;
        preparedDocument->completionGeneration = completionGeneration;
    }

    void EditorPreparedDocumentCache::EnsureGeometry(
        std::stop_token stop,
        std::function<void(std::size_t, std::size_t)> progress)
    {
        if (!preparedDocument->geometry.Initialized()) InitializeGeometry(stop, progress);
        else if (preparedDocument->flowContextsChanged) BuildFlowGroups();
        ReconcileFlowContexts(stop);
        ReconcileTrailingPreview();
    }

    void EditorPreparedDocumentCache::ReconcileTrailingPreview()
    {
        auto owner = preparedDocument->selection.active.container_id;
        auto found = preparedDocument->FindOwnerBlock(owner);
        if (!found || *found >= frame.renderModel.fragments.size())
        {
            preparedDocument->ResetTrailingPreview();
            preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
            return;
        }
        auto index = *found;
        auto& source = preparedDocument->blocks[index];
        auto group = FindFlowGroup(index);
        if (!group || group->end == index + 1
            || (!source.containsMath && !source.containsMermaid && source.previewHeight == 0.0f))
        {
            preparedDocument->ResetTrailingPreview();
            preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
            return;
        }
        auto target = group->end - 1;
        auto& preview = preparedDocument->trailingPreview;
        if (!preview || preview->owner != owner || preview->sourceIndex != index
            || preview->targetIndex != target || preview->height != source.previewHeight)
        {
            std::optional<EditorPreparedDocument::TrailingPreview> resident;
            if (preview && preview->owner == owner && preview->sourceIndex == index
                && preview->targetIndex == target)
                resident = std::move(*preview);
            preparedDocument->ResetTrailingPreview();
            if (resident) preview = std::move(resident);
            else preview.emplace();
            preview->owner = owner;
            preview->sourceIndex = index;
            preview->targetIndex = target;
            preview->height = source.previewHeight;
        }
        if (source.valid && source.embeddedRequested)
        {
            preview->mathPreviews.clear();
            for (auto const& item : source.Visuals().mathPreviews)
                if (!item.floating) preview->mathPreviews.push_back(item);
            preview->mermaidPreviews = source.Visuals().mermaidPreviews;
            preview->resident = true;
            preview->pending = source.pendingMath || source.pendingMermaid;
            preview->embeddedGeneration = source.embeddedGeneration;
        }
        preparedDocument->geometry.UpdateHeight(index, preparedDocument->GeometryHeight(index));
        preparedDocument->geometry.UpdateHeight(target, preparedDocument->GeometryHeight(target));
        preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
    }

    EditorPreparedDocument::FlowGroup* EditorPreparedDocumentCache::FindFlowGroup(std::size_t index)
    {
        if (index >= frame.renderModel.fragments.size()) return nullptr;
        auto root = frame.renderModel.fragments[index].source_root.v;
        auto found = preparedDocument->flowGroups.find(root);
        return found == preparedDocument->flowGroups.end() ? nullptr : &found->second;
    }

    void EditorPreparedDocumentCache::BuildFlowGroups()
    {
        preparedDocument->flowGroups.clear();
        preparedDocument->pendingFlowGroups.clear();
        preparedDocument->flowContextsChanged = false;
        auto const& fragments = frame.renderModel.fragments;
        for (std::size_t begin = 0; begin < fragments.size();)
        {
            auto root = fragments[begin].source_root.v;
            auto end = begin + 1;
            while (end < fragments.size() && fragments[end].source_root.v == root) ++end;
            if (end > begin + 1)
            {
                auto& group = preparedDocument->flowGroups[root];
                group.begin = begin;
                group.end = end;
                for (auto index = begin; index < end; ++index)
                {
                    auto const& block = preparedDocument->blocks[index];
                    if (block.hasFlowContext) group.summary.Add(block.intrinsicFlow);
                }
                preparedDocument->pendingFlowGroups.insert(root);
            }
            begin = end;
        }
    }

    void EditorPreparedDocumentCache::ReconcileFlowContexts(std::stop_token stop)
    {
        // Only a changed shared formatting context invalidates other fragments.
        // Ordinary input updates its intrinsic summary in constant time above.
        for (auto root : preparedDocument->pendingFlowGroups)
        {
            auto const& group = preparedDocument->flowGroups.at(root);
            auto context = group.summary.Context();
            for (auto index = group.begin; index < group.end; ++index)
            {
                if (stop.stop_requested()) throw winrt::hresult_canceled();
                auto& block = preparedDocument->blocks[index];
                if (!block.hasFlowContext || block.appliedFlow == context) continue;
                auto imageSources = EditorDocumentBlockPreparer::ImageSources(block);
                MeasureGeometry(index, context);
                preparedDocument->RegisterStructuralOwners(index);
                preparedDocument->geometry.UpdateHeight(index, preparedDocument->GeometryHeight(index));
                for (auto const& imageSource : imageSources) inlineImages.ReleaseGif(imageSource);
            }
        }
        preparedDocument->pendingFlowGroups.clear();
        preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
    }

    void EditorPreparedDocumentCache::MeasureGeometry(std::size_t index,
        std::optional<EditorFlowLayoutContext> flowContext)
    {
        // Measurement and retention are different operations. Use the same
        // DirectWrite preparation path as painting, then immediately drop its
        // display map, glyph/line data and paint plan. Only one fragment needs
        // to be resident while the exact height index is populated.
        if (!frame.renderModel.blocks[index].materialized && frame.materializeBlocks)
            frame.materializeBlocks(index, index + 1);
        auto const& source = frame.renderModel.blocks[index];
        auto& prepared = preparedDocument->blocks[index];
        if (!flowContext && index < frame.renderModel.fragments.size())
        {
            auto const& fragment = frame.renderModel.fragments[index];
            if (!fragment.first || !fragment.last)
                flowContext = EditorFlowLayoutContext{.fragmented = true};
        }
        if (source.kind == folia::RenderBlockKind::ThematicBreak)
        {
            blockPreparer.InitializeMetadata(prepared, source);
            prepared.height = 40.0f;
            prepared.geometryMeasured = true;
            prepared.valid = true;
        }
        else
        {
            // Remote/embedded resources keep their existing asynchronous
            // policy. This measures their current presentation and never
            // starts screenfuls of offscreen network or render requests.
            prepared = blockPreparer.Prepare(source, false, false, flowContext);
            auto imageSources = EditorDocumentBlockPreparer::ImageSources(prepared);
            prepared.ReleaseVisualContent();
            for (auto const& imageSource : imageSources) inlineImages.ReleaseGif(imageSource);
        }
        preparedDocument->layoutBlocks.erase(index);
        preparedDocument->embeddedBlocks.erase(index);
        if (frame.releaseBlocksOutside) frame.releaseBlocksOutside(index + 1, index + 1);
    }

    void EditorPreparedDocumentCache::InitializeGeometry(
        std::stop_token stop,
        std::function<void(std::size_t, std::size_t)> const& progress)
    {
        preparedDocument->embeddedBlocks.clear();
        std::vector<BlockGeometryIndex::Entry> entries;
        entries.reserve(frame.renderModel.blocks.size());
        if (progress) progress(0, frame.renderModel.blocks.size());
        for (std::size_t index = 0; index < frame.renderModel.blocks.size(); ++index)
        {
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            auto const& block = frame.renderModel.blocks[index];
            auto& prepared = preparedDocument->blocks[index];
            if (!prepared.geometryMeasured)
            {
                MeasureGeometry(index);
                preparedDocument->RegisterStructuralOwners(index);
            }
            else if (prepared.sourceId.v == 0)
            {
                blockPreparer.InitializeMetadata(prepared, block);
                preparedDocument->RegisterStructuralOwners(index);
            }
            entries.push_back({
                block.block_style.margin_top,
                prepared.height,
                block.block_style.margin_bottom
                    + (block.source_mode || block.fragment_continues ? 0.0f : styleSheet.blockGap),
            });
            if (prepared.embeddedRequested
                && (prepared.containsMath
                    || prepared.containsImage
                    || prepared.containsMermaid))
                preparedDocument->embeddedBlocks.insert(index);
            if (progress) progress(index + 1, frame.renderModel.blocks.size());
        }
        preparedDocument->geometry.Reset(std::move(entries), styleSheet.verticalPadding);
        preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
        BuildFlowGroups();
    }
}
