#include "pch.h"
#include "editor/session/EditorSession.h"

import folia.markdown.serializer;

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
        else if (!core_->editor.execute_command(command))
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
}
