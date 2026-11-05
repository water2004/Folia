#pragma once
import folia.completion;
import folia.markdown.utf;

namespace completion_tests {
// Fixture intent becomes an explicit insert or replacement operation.
inline std::u32string Reply(folia::completion::Prompt const& prompt,
    std::u32string_view text, std::u32string_view replaced = {}) {
    if (text.empty() && replaced.empty()) return {};
    auto begin = prompt.cursor.find(prompt.edit_start);
    auto end = prompt.cursor.find(prompt.edit_end, begin + prompt.edit_start.size());
    if (begin == prompt.cursor.npos || end == prompt.cursor.npos) throw std::runtime_error("Missing fixture edit markers.");
    auto tail = folia::utf8_to_cps(std::string_view(prompt.cursor).substr(begin + prompt.edit_start.size(), end - begin - prompt.edit_start.size()));
    if (!tail.starts_with(replaced)) throw std::runtime_error("Fixture replacement does not match the local tail.");
    if (replaced.empty()) return U"<insert>" + std::u32string(text) + U"</insert>";
    return U"<replace>" + std::u32string(text) + tail.substr(replaced.size()) + U"</replace>";
}
}
