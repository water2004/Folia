#pragma once

import folia.core.render_model;
import folia.markdown.text_edit;

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
    EditorCaretPresentationState CaretPresentationState(folia::RenderBlock const&, folia::TextPosition);
}
