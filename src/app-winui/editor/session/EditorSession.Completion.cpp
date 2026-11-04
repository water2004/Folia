#include "pch.h"
#include "editor/session/EditorSession.h"
import folia.markdown.document_text;

namespace winrt::Folia {
std::optional<folia::completion::Context> EditorSession::CaptureCompletionContext(folia::completion::Budget budget) const {
    auto selection = Selection();
    if (selection.anchor != selection.active) return {};
    if (auto editor = core_->editor.source_editor()) {
        auto const& lines = editor->lines();
        auto found = std::ranges::find(lines, selection.active.container_id, &folia::SourceLine::id);
        if (found == lines.end()) return {};
        std::vector<std::u32string_view> preceding, following;
        auto index = static_cast<std::size_t>(found - lines.begin());
        for (auto i = index > 3 ? index - 3 : 0; i < index; ++i) preceding.push_back(lines[i].text);
        for (auto i = index + 1; i < lines.size() && i <= index + 3; ++i) following.push_back(lines[i].text);
        return folia::completion::capture(found->text, selection.active.source_offset, preceding, budget, following);
    }
    auto source = TextInputSourceView(selection.active.container_id);
    if (!source) return {};
    std::vector<std::u32string_view> preceding, following;
    auto const& document = core_->editor.document();
    // This index is already maintained by source/tree edits; do not walk the tree.
    auto found = document.cached_editable_index.find(selection.active.container_id.v);
    if (found != document.cached_editable_index.end()) {
        auto end = found->second;
        for (auto index = end > 3 ? end - 3 : 0; index < end; ++index)
            if (auto text = TextInputSourceView(document.cached_editable_order[index])) preceding.push_back(*text);
        for (auto index = end + 1; index < document.cached_editable_order.size() && index <= end + 3; ++index)
            if (auto text = TextInputSourceView(document.cached_editable_order[index])) following.push_back(*text);
    }
    return folia::completion::capture(*source, selection.active.source_offset, preceding, budget, following);
}
}
