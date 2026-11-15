#include "pch.h"
#include "editor/rendering/EditorPreparedDocumentCache.h"

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

    std::optional<std::size_t> EditorPreparedDocumentCache::FindOwnerBlock(
        folia::NodeId owner) const
    {
        if (!preparedDocument) return std::nullopt;
        auto found = preparedDocument->ownerBlockIndex.find(owner.v);
        if (found == preparedDocument->ownerBlockIndex.end()) return std::nullopt;
        return found->second;
    }

    void EditorPreparedDocumentCache::RemoveOwners(
        EditorPreparedDocument::Block const& block, std::size_t index)
    {
        for (auto owner : block.owners)
        {
            ++preparedDocument->ownerIndexEntriesVisited;
            auto found = preparedDocument->ownerBlockIndex.find(owner.v);
            if (found != preparedDocument->ownerBlockIndex.end() && found->second == index)
                preparedDocument->ownerBlockIndex.erase(found);
        }
    }

    void EditorPreparedDocumentCache::RegisterOwners(std::size_t index)
    {
        auto& prepared = preparedDocument->blocks[index];
        std::unordered_set<std::uint64_t> owners;
        owners.reserve(prepared.owners.size() + 1);
        for (auto owner : prepared.owners)
        {
            ++preparedDocument->ownerIndexEntriesVisited;
            owners.insert(owner.v);
        }
        auto add = [&](folia::NodeId owner)
        {
            ++preparedDocument->ownerIndexEntriesVisited;
            if (owner.v != 0 && owners.insert(owner.v).second) prepared.owners.push_back(owner);
        };
        // Thematic breaks have geometry without a text layout. Source-mode
        // rows have their own physical owner and no fragment descriptor.
        add(prepared.sourceId);
        if (index < frame.renderModel.fragments.size())
        {
            auto const& fragment = frame.renderModel.fragments[index];
            auto first = frame.renderModel.editable_index.find(fragment.first_owner.v);
            auto last = frame.renderModel.editable_index.find(fragment.last_owner.v);
            if (first != frame.renderModel.editable_index.end()
                && last != frame.renderModel.editable_index.end()
                && first->second <= last->second
                && last->second < frame.renderModel.editable_order.size())
            {
                // Include every editable cell in an atomic table and every
                // empty/hidden source owner, independently of visible glyphs.
                for (auto ordinal = first->second; ordinal <= last->second; ++ordinal)
                    add(frame.renderModel.editable_order[ordinal]);
            }
        }
        for (auto owner : prepared.owners)
        {
            ++preparedDocument->ownerIndexEntriesVisited;
            preparedDocument->ownerBlockIndex[owner.v] = index;
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
        if (preparedDocument)
        {
            cache = {
                .available = true,
                .blockCount = preparedDocument->blocks.size(),
                .modelRevision = preparedDocument->modelRevision,
                .documentWidth = preparedDocument->documentWidth,
                .themeRevision = preparedDocument->themeRevision,
                .active = preparedDocument->selection.active,
            };
            previousActiveBlock = FindOwnerBlock(
                preparedDocument->selection.active.container_id);
            activeBlock = FindOwnerBlock(selection.active.container_id);
        }
        auto sourceMode = !frame.renderModel.blocks.empty()
            && frame.renderModel.blocks.front().source_mode;
        auto layoutAffectedByCaret = [&](std::optional<std::size_t> index) -> std::optional<std::size_t> {
            if (!index || !preparedDocument || cache.active == selection.active) return {};
            if (*index >= frame.renderModel.blocks.size() || *index >= preparedDocument->blocks.size()) return index;
            auto const& prepared = preparedDocument->blocks[*index];
            if (!prepared.valid) return index;
            if (!frame.renderModel.blocks[*index].materialized && frame.materializeBlocks)
                frame.materializeBlocks(*index, *index + 1);
            auto const& source = frame.renderModel.blocks[*index];
            if (!source.materialized || prepared.Visuals().caretPresentation != CaretPresentationState(source, selection.active)) return index;
            return {};
        };
        auto invalidationPlan = BuildEditorPreparationInvalidationPlan(
            cache,
            frame.renderModel.blocks.size(),
            frame.renderModel.revision,
            documentWidth,
            themeRevision,
            selection.active,
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
            if (previous)
                preparedDocument->ownerIndexEntriesVisited = previous->ownerIndexEntriesVisited;
            if (previous
                && previous->documentWidth == documentWidth
                && previous->themeRevision == themeRevision)
            {
                preparedDocument->ownerBlockIndex = std::move(previous->ownerBlockIndex);
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
                            && (!source.materialized || candidate.Visuals().caretPresentation != CaretPresentationState(source, selection.active))))) continue;
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
                        RegisterOwners(index);
            }
        }
        else
        {
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
                prepared = {};
                MeasureGeometry(index, flowContext);
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
                RegisterOwners(index);
                preparedDocument->layoutBlocks.erase(index);
                preparedDocument->embeddedBlocks.erase(index);
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
        auto found = preparedDocument->ownerBlockIndex.find(owner.v);
        if (owner.v == 0 || found == preparedDocument->ownerBlockIndex.end()
            || found->second >= frame.renderModel.fragments.size())
        {
            preparedDocument->ResetTrailingPreview();
            preparedDocument->totalHeight = preparedDocument->geometry.TotalHeight();
            return;
        }
        auto index = found->second;
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
                RegisterOwners(index);
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
                RegisterOwners(index);
            }
            else if (prepared.sourceId.v == 0)
            {
                blockPreparer.InitializeMetadata(prepared, block);
                RegisterOwners(index);
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
