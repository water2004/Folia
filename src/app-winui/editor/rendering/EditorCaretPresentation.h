#pragma once

import folia.core.render_model;
import folia.markdown.text_edit;
import folia.markdown.owner_index;

namespace winrt::Folia {
    enum class EditorInlinePresentation { Hidden, Rendered, Source };

    struct EditorCaretPresentationToken {
        folia::TextSpan source;
        EditorInlinePresentation appearance;
        std::optional<std::size_t> mathCaret;
        friend bool operator==(EditorCaretPresentationToken const&, EditorCaretPresentationToken const&) = default;
    };
    using EditorCaretPresentationState = std::vector<EditorCaretPresentationToken>;

    EditorInlinePresentation ResolveInlinePresentation(folia::InlineRenderItem const&, folia::TextPosition);
    inline folia::TextPosition EditorPresentationCaret(folia::TextSelection selection)
    {
        return selection.selects_cells() ? folia::TextPosition{} : selection.active;
    }
    std::optional<std::size_t> EditorTableCellIndex(
        std::span<const folia::TextSpan> cells, folia::NodeId owner, const folia::OwnerIndex& index);
    std::optional<std::size_t> EditorTableCheckboxIndex(folia::RenderBlockSpecial const& table, std::size_t cell);
    EditorCaretPresentationState CaretPresentationState(folia::RenderBlock const&, folia::TextPosition, const folia::OwnerIndex&);
}
