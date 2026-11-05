#include "pch.h"
#include "editor/session/EditorSession.h"
import folia.markdown.document_text;

namespace winrt::Folia {
std::optional<folia::completion::Context> EditorSession::CaptureCompletionContext(folia::completion::Budget budget) const {
    auto selection = Selection();
    if (!selection.is_caret()) return {};
    auto capture = [&](std::u32string_view source, std::size_t index, std::size_t count, auto sourceAt, std::string_view separator) {
        std::vector<std::u32string_view> preceding, following;
        auto remaining = (std::min)(budget.preceding_bytes, folia::completion::max_context_bytes);
        for (auto i = index; i > 0 && remaining > separator.size();) {
            if (auto text = sourceAt(--i)) {
                preceding.push_back(*text);
                remaining -= (std::min)(text->size(), remaining - separator.size()) + separator.size();
            } else remaining -= separator.size();
        }
        std::ranges::reverse(preceding);
        remaining = (std::min)(budget.following_bytes, folia::completion::max_context_bytes);
        for (auto i = index + 1; i < count && remaining > separator.size(); ++i) {
            if (auto text = sourceAt(i)) {
                following.push_back(*text);
                remaining -= (std::min)(text->size(), remaining - separator.size()) + separator.size();
            } else remaining -= separator.size();
        }
        return folia::completion::capture(source, selection.active.source_offset, preceding, budget, following, separator);
    };
    if (auto editor = core_->editor.source_editor()) {
        auto const& lines = editor->lines();
        auto found = std::ranges::find(lines, selection.active.container_id, &folia::SourceLine::id);
        if (found == lines.end()) return {};
        return capture(found->text, static_cast<std::size_t>(found - lines.begin()), lines.size(),
            [&](std::size_t i) -> std::optional<std::u32string_view> { return lines[i].text; }, "\n");
    }
    auto source = TextInputSourceView(selection.active.container_id);
    if (!source) return {};
    auto const& document = core_->editor.document();
    // Reuse the maintained editable index, without a full document walk or serialization.
    auto found = document.cached_editable_index.find(selection.active.container_id.v);
    if (found == document.cached_editable_index.end())
        return folia::completion::capture(*source, selection.active.source_offset, {}, budget);
    return capture(*source, found->second, document.cached_editable_order.size(),
        [&](std::size_t i) { return TextInputSourceView(document.cached_editable_order[i]); }, "\n\n");
}
}
