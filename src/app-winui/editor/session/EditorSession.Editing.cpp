#include "pch.h"
#include "editor/session/EditorSession.h"

import folia.markdown.serializer;
import folia.markdown.block_source;
import folia.markdown.inline_cst;

namespace winrt::Folia
{
    namespace
    {
        // Reload restores the view through the serializer's source-boundary map.
        // This remains a boundary projection and never becomes editor state.
        struct SerializedSourceProjection
        {
            explicit SerializedSourceProjection(folia::SerializedMarkdownProjection value)
                : projection(std::move(value)) {}

            std::size_t SourceOffset(folia::TextPosition position) const
            {
                return folia::serialized_offset_for_source_position(projection, position)
                    .value_or((std::min)(position.source_offset, projection.text.size()));
            }

            folia::TextPosition Position(std::size_t sourceOffset, folia::TextAffinity affinity) const
            {
                return folia::source_position_for_serialized_offset(projection, sourceOffset, affinity)
                    .value_or(folia::TextPosition{});
            }

            folia::SerializedMarkdownProjection projection;
        };

    }

    bool EditorSession::IsSourceMode() const
    {
        return core_ && core_->editor.source_editor()!=nullptr;
    }

    bool EditorSession::EnterSourceMode()
    {
        if (!core_->editor.enter_source_mode()) return false;
        ClearSearch();
        core_->renderedSearchFragments.clear();
        ++revision_;
        core_->characterCount=core_->editor.source_editor()->source().size();
        RebuildRenderModel();
        return true;
    }

    bool EditorSession::ExitSourceMode()
    {
        if (!core_->editor.exit_source_mode()) return false;
        ClearSearch();
        core_->renderedSearchFragments.clear();
        ++revision_;
        RefreshCharacterCount();
        RebuildRenderModel();
        return true;
    }

    bool EditorSession::ToggleSourceMode()
    {
        return IsSourceMode() ? ExitSourceMode() : EnterSourceMode();
    }

    EditorReloadViewState EditorSession::CaptureReloadViewState() const
    {
        EditorReloadViewState state;
        state.sourceMode = IsSourceMode();
        if (core_->editor.source_editor())
        {
            auto selection = core_->editor.source_editor()->selection();
            state.anchor = selection.anchor;
            state.active = selection.active;
            state.anchorAffinity = selection.anchor_affinity;
            state.activeAffinity = selection.active_affinity;
            return state;
        }

        SerializedSourceProjection projection(
            folia::serialize_markdown_projection(core_->editor.document()));
        auto selection = core_->editor.selection();
        state.anchor = projection.SourceOffset(selection.anchor);
        state.active = projection.SourceOffset(selection.active);
        state.anchorAffinity = selection.anchor.affinity;
        state.activeAffinity = selection.active.affinity;
        return state;
    }

    void EditorSession::RestoreReloadViewState(EditorReloadViewState const& state)
    {
        if (state.sourceMode)
        {
            EnterSourceMode();
            auto length = core_->editor.source_editor()->source().size();
            core_->editor.set_source_selection({
                (std::min)(state.anchor, length),
                (std::min)(state.active, length),
                state.anchorAffinity,
                state.activeAffinity,
            });
            return;
        }

        SerializedSourceProjection projection(
            folia::serialize_markdown_projection(core_->editor.document()));
        auto anchor = projection.Position(state.anchor, state.anchorAffinity);
        auto active = projection.Position(state.active, state.activeAffinity);
        if (core_->editor.editable_source(anchor.container_id)
            && core_->editor.editable_source(active.container_id))
        {
            core_->editor.set_selection({anchor, active});
        }
    }

    bool EditorSession::ExecuteCommand(folia::Command const& command)
    {
        if (core_->editor.source_editor())
        {
            auto previousRevision = core_->editor.source_editor()->revision();
            if (!core_->editor.execute_command(command)) return false;
            if (core_->editor.source_editor()->revision() == previousRevision) return true;
            ++revision_;
            core_->characterCount = core_->editor.source_editor()->source().size();
            RebuildRenderModel();
            return true;
        }

        auto previousDocumentRevision = core_->editor.revision();
        if (command.kind == folia::CommandKind::Undo)
        {
            if (!core_->editor.undo()) return false;
        }
        else if (command.kind == folia::CommandKind::Redo)
        {
            if (!core_->editor.redo()) return false;
        }
        else if (command.kind == folia::CommandKind::SelectAll)
        {
            return core_->editor.execute_command(command);
        }
        else if (!TryMathBracketEdit(command) && !core_->editor.execute_command(command))
        {
            return false;
        }

        if (core_->editor.revision() != previousDocumentRevision)
        {
            ++revision_;
            auto change = core_->editor.take_last_document_change();
            if (!change || change->structural)
            {
                RefreshCharacterCount();
            }
            else
            {
                std::ptrdiff_t delta = 0;
                for (auto const& operation : change->text_operations)
                {
                    auto const& edit = change->forward ? operation.forward : operation.inverse;
                    delta += static_cast<std::ptrdiff_t>(edit.replacement.size());
                    delta -= static_cast<std::ptrdiff_t>(edit.range.length());
                }
                if (delta < 0 && core_->characterCount < static_cast<std::size_t>(-delta))
                    RefreshCharacterCount();
                else
                    core_->characterCount = static_cast<std::size_t>(
                        static_cast<std::ptrdiff_t>(core_->characterCount) + delta);
            }
            RebuildRenderModel(change ? &*change : nullptr);
        }
        return true;
    }

    bool EditorSession::TryMathBracketEdit(folia::Command const& command) {
        using namespace folia::math::editor;
        auto typing = command.kind == folia::CommandKind::InsertText && command.typed_input && command.text.size() == 1;
        if (typing && std::u32string_view{U"()[]{}"}.find(command.text.front()) == std::u32string_view::npos) return false;
        auto backward = command.kind == folia::CommandKind::DeleteBackward;
        if (!typing && !backward && command.kind != folia::CommandKind::DeleteForward) return false;
        auto selection = core_->editor.selection();
        if (selection.anchor.container_id != selection.active.container_id) return false;
        auto block = folia::find_document_block(core_->editor.document(), selection.active.container_id);
        if (!block) return false;
        std::u32string tex;
        std::vector<std::size_t> offsets;
        if (block->kind == folia::BlockKind::MathBlock) {
            tex = folia::block_source_content(block->block_source);
            offsets = block->block_source.tree().content_to_source;
        } else {
            auto find = [&](auto& self, folia::InlineCstNodes const& nodes) -> std::optional<folia::SourceRange> {
                for (auto const& node : nodes) {
                    if (auto nested = self(self, node.children)) return nested;
                    auto range = node.delimiter_ranges().content;
                    if (node.kind == folia::InlineCstKind::InlineMath
                        && selection.anchor.source_offset >= range.start && selection.anchor.source_offset <= range.end
                        && selection.active.source_offset >= range.start && selection.active.source_offset <= range.end) return range;
                }
                return std::nullopt;
            };
            auto range = find(find, block->inline_content.tree.nodes);
            if (!range) return false;
            tex = block->inline_content.source.substr(range->start, range->length());
            for (auto i = range->start; i <= range->end; ++i) offsets.push_back(i);
        }
        if (offsets.size() != tex.size() + 1) return false;
        auto local = [&](std::size_t value) -> std::optional<std::size_t> {
            auto it = std::lower_bound(offsets.begin(), offsets.end(), value);
            if (it == offsets.end() || *it != value) return std::nullopt;
            return static_cast<std::size_t>(it - offsets.begin());
        };
        auto anchor = local(selection.anchor.source_offset), active = local(selection.active.source_offset);
        if (!anchor || !active) return false;
        if (!bracketSyntax_) bracketSyntax_ = std::make_unique<SyntaxSession>();
        auto plan = bracketSyntax_->PlanBracketEdit(tex, *anchor, *active,
            typing ? BracketAction::Type : backward ? BracketAction::Backspace : BracketAction::Delete,
            typing ? command.text.front() : 0);
        if (!plan) return false;
        auto owner = selection.active.container_id;
        auto makePosition = [&](std::size_t offset) {
            return folia::TextPosition{owner, offset, folia::TextAffinity::Downstream};
        };
        if (plan->edits.empty()) {
            core_->editor.set_selection({makePosition(offsets.at(plan->anchor)), makePosition(offsets.at(plan->active))});
            return true;
        }
        auto source = core_->editor.editable_source(owner);
        if (!source) return false;
        auto begin = source->size(), end = std::size_t{};
        for (auto const& edit : plan->edits) {
            begin = (std::min)(begin, offsets.at(edit.range.start));
            end = (std::max)(end, offsets.at(edit.range.end));
        }
        auto replacement = source->substr(begin, end - begin);
        for (auto it = plan->edits.rbegin(); it != plan->edits.rend(); ++it) {
            auto first = offsets.at(it->range.start), last = offsets.at(it->range.end);
            replacement.replace(first - begin, last - first, it->text);
            auto delta = static_cast<std::ptrdiff_t>(it->text.size()) - static_cast<std::ptrdiff_t>(last - first);
            offsets.erase(offsets.begin() + it->range.start, offsets.begin() + it->range.end);
            for (auto i = it->range.start; i < offsets.size(); ++i)
                offsets[i] = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offsets[i]) + delta);
            std::vector<std::size_t> inserted;
            for (std::size_t i = 0; i < it->text.size(); ++i) inserted.push_back(first + i);
            offsets.insert(offsets.begin() + it->range.start, inserted.begin(), inserted.end());
        }
        return core_->editor.execute_document_replace_text({owner, {begin, end}}, replacement,
            {makePosition(offsets.at(plan->anchor)), makePosition(offsets.at(plan->active))});
    }
}
