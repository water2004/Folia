module folia.completion;
import std;

namespace folia::completion {
namespace {
std::size_t utf8_size(char32_t c) {
    return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
}
void append(std::string& out, char32_t c) {
    if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) c = 0xfffd;
    if (c < 0x80) out += static_cast<char>(c);
    else if (c < 0x800) {
        out += static_cast<char>(0xc0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 63));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xe0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    } else {
        out += static_cast<char>(0xf0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 63));
        out += static_cast<char>(0x80 | ((c >> 6) & 63)); out += static_cast<char>(0x80 | (c & 63));
    }
}
std::string encode(std::u32string_view value) {
    std::string result;
    for (auto c : value) append(result, c);
    return result;
}
std::u32string_view head(std::u32string_view value, std::size_t bytes) {
    std::size_t end = 0;
    while (end < value.size()) {
        auto size = utf8_size(value[end]);
        if (size > bytes) break;
        bytes -= size; ++end;
    }
    return value.substr(0, end);
}
std::u32string_view tail(std::u32string_view value, std::size_t bytes) {
    auto start = value.size();
    while (start) {
        auto size = utf8_size(value[start - 1]);
        if (size > bytes) break;
        bytes -= size; --start;
    }
    return value.substr(start);
}
std::size_t weight(Context const& c, Edit const& edit) {
    return c.preceding.size() + c.following.size() + c.prefix.size() + c.suffix.size()
        + (edit.before.size() + edit.after.size()) * sizeof(char32_t);
}
}
Budget make_budget(std::size_t context_bytes, std::uint32_t output_tokens) {
    if (context_bytes < 1000 || context_bytes > max_context_bytes || output_tokens < 16 || output_tokens > max_output_tokens)
        throw std::invalid_argument("Completion budget is out of range.");
    auto suffix = context_bytes / 7;
    auto surrounding = context_bytes / 7;
    return {context_bytes - suffix - surrounding * 2, suffix, surrounding, output_tokens, 512, surrounding};
}
Context capture(std::u32string_view source, std::size_t caret,
    std::span<std::u32string_view const> preceding, Budget budget,
    std::span<std::u32string_view const> following, std::string_view separator) {
    // Bound even custom callers; every traversal and allocation below is bounded.
    budget.prefix_bytes = std::min(budget.prefix_bytes, max_context_bytes);
    budget.suffix_bytes = std::min(budget.suffix_bytes, max_context_bytes - budget.prefix_bytes);
    auto remaining = std::min(budget.preceding_bytes, max_context_bytes - budget.prefix_bytes - budget.suffix_bytes);
    auto afterRemaining = std::min(budget.following_bytes,
        max_context_bytes - budget.prefix_bytes - budget.suffix_bytes - remaining);
    caret = std::min(caret, source.size());
    auto prefix = tail(source.substr(0, caret), budget.prefix_bytes);
    auto start = caret - prefix.size();
    if (start) {
        // Move forward to a stable block-local anchor, never expand the budget.
        // Nearby keystrokes then append to the same prompt prefix rather than
        // shifting its first character on every request in a long paragraph.
        auto advance = (256 - start % 256) % 256;
        start += std::min(advance, caret - start);
        prefix = source.substr(start, caret - start);
    }
    Context result{ {}, encode(prefix),
        encode(head(source.substr(caret), budget.suffix_bytes)) };
    // Even finding the editing line stays inside the captured window.
    auto lineBreak = prefix.find_last_of(U'\n');
    auto activeStart = lineBreak == prefix.npos ? start : start + lineBreak + 1;
    if (caret - activeStart > 512)
        activeStart = std::max(activeStart, caret / 256 * 256 - 256);
    result.active_prefix_byte = encode(source.substr(start, activeStart - start)).size();
    const auto nextLine = result.suffix.find('\n');
    result.active_suffix_bytes = nextLine == result.suffix.npos ? result.suffix.size() : nextLine;
    std::vector<std::string> fragments;
    for (auto it = preceding.rbegin(); it != preceding.rend() && remaining > separator.size(); ++it) {
        auto part = encode(tail(*it, remaining - separator.size()));
        remaining -= part.size() + separator.size();
        fragments.push_back(std::move(part));
    }
    for (auto it = fragments.rbegin(); it != fragments.rend(); ++it) result.preceding += *it + std::string(separator);
    for (auto text : following) {
        if (afterRemaining <= separator.size()) break;
        auto part = encode(head(text, afterRemaining - separator.size()));
        afterRemaining -= part.size() + separator.size();
        result.following += std::string(separator) + part;
    }
    return result;
}
std::string_view editable_suffix(Context const& c) {
    auto end = std::min(c.active_suffix_bytes, c.suffix.size());
    std::size_t at = 0, count = 0;
    while (at < end && count < editable_tail_characters && c.suffix[at] != '\n' && c.suffix[at] != '\r') {
        auto lead = static_cast<unsigned char>(c.suffix[at]);
        auto size = lead < 0x80 ? 1u : lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
        if (size > end - at) break;
        at += size; ++count;
    }
    return std::string_view(c.suffix).substr(0, at);
}
EditMarkers edit_markers(Context const& c, PromptSettings const& settings) {
    std::string start = "<" + settings.region_name + ">", end = "</" + settings.region_name + ">";
    auto contains = [&](std::string const& marker) {
        return c.preceding.contains(marker) || c.prefix.contains(marker) || c.suffix.contains(marker) || c.following.contains(marker);
    };
    while (contains(start) || contains(end)) { start.insert(start.size() - 1, "_"); end.insert(end.size() - 1, "_"); }
    return {std::move(start), std::move(end)};
}
namespace {
std::string expand_instructions(std::string_view text, std::string_view start, std::string_view end,
    std::string_view policy, std::string_view characters) {
    std::string out;
    while (!text.empty()) {
        auto begin = text.find("${");
        if (begin == text.npos) { out += text; break; }
        out += text.substr(0, begin);
        text.remove_prefix(begin + 2);
        auto close = text.find('}');
        if (close == text.npos) throw std::invalid_argument("Unclosed completion prompt parameter.");
        auto name = text.substr(0, close);
        if (name == "edit_start") out += start;
        else if (name == "edit_end") out += end;
        else if (name == "policy") out += policy;
        else if (name == "max_characters") out += characters;
        else throw std::invalid_argument("Unknown completion prompt parameter.");
        text.remove_prefix(close + 1);
    }
    return out;
}
}
std::optional<std::string> validate_prompt_settings(PromptSettings const& settings) {
    if (settings.instructions.empty() || settings.instructions.size() > 16384 || settings.document_prefix.size() > 1024
        || settings.before_edit.size() > 1024 || settings.after_edit.size() > 1024)
        return "Completion prompt settings are empty or exceed their size limit.";
    auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    if (settings.region_name.empty() || settings.region_name.size() > 24 || !letter(settings.region_name.front())
        || !std::ranges::all_of(settings.region_name, [&](char c) { return letter(c) || (c >= '0' && c <= '9') || c == '_'; }))
        return "Completion prompt regionName must be an ASCII tag name (1-24 characters).";
    for (auto const& policy : settings.policies)
        if (policy.empty() || policy.size() > 1024) return "Completion prompt policies are empty or exceed their size limit.";
    std::size_t exampleBytes = 0;
    if (settings.examples.size() > 4) return "Completion prompt has too many examples.";
    for (auto const& example : settings.examples)
        exampleBytes += example.before.size() + example.editable.size() + example.after.size() + example.response.size();
    if (exampleBytes > 4096) return "Completion prompt examples exceed their size limit.";
    if (!settings.instructions.contains("${edit_start}") || !settings.instructions.contains("${edit_end}"))
        return "Completion prompt must contain edit_start and edit_end parameters.";
    try { (void)expand_instructions(settings.instructions, "<edit>", "</edit>", "", "512"); }
    catch (std::invalid_argument const& error) { return error.what(); }
    return {};
}
Prompt make_prompt(Context const& c, PromptSettings const& settings, Budget budget, Eagerness eagerness) {
    if (auto error = validate_prompt_settings(settings)) throw std::invalid_argument(*error);
    const auto activePrefix = std::min(c.active_prefix_byte, c.prefix.size());
    const auto activeSuffix = editable_suffix(c).size();
    auto [start, end] = edit_markers(c, settings);
    auto level = static_cast<std::size_t>(eagerness);
    if (level >= settings.policies.size()) throw std::invalid_argument("Invalid completion eagerness.");
    Prompt prompt{
        expand_instructions(settings.instructions, start, end, settings.policies[level],
            std::to_string(std::min(budget.proposal_characters, std::size_t{512}))),
        settings.document_prefix + c.preceding + c.prefix.substr(0, activePrefix),
        c.prefix.substr(activePrefix) + settings.before_edit + start + c.suffix.substr(0, activeSuffix) + end
            + settings.after_edit + c.suffix.substr(activeSuffix) + c.following,
        std::clamp(budget.output_tokens, std::uint32_t{16}, max_output_tokens), start, end };
    for (auto const& example : settings.examples)
        prompt.examples.emplace_back(settings.document_prefix + example.before + settings.before_edit + start + example.editable + end
            + settings.after_edit + example.after,
            example.response);
    return prompt;
}
Cache::Cache(std::size_t max_bytes, std::size_t max_entries)
    : max_bytes_(max_bytes), max_entries_(max_entries) {}
std::optional<Edit> Cache::get(Context const& context) {
    auto it = std::ranges::find(entries_, context, &Entry::context);
    if (it == entries_.end()) return {};
    entries_.splice(entries_.begin(), entries_, it);
    return entries_.front().proposal;
}
void Cache::put(Context context, Edit text) {
    auto it = std::ranges::find(entries_, context, &Entry::context);
    if (it != entries_.end()) { bytes_ -= it->bytes; entries_.erase(it); }
    auto size = weight(context, text);
    if (size > max_bytes_ || max_entries_ == 0) return;
    while (!entries_.empty() && (bytes_ + size > max_bytes_ || entries_.size() >= max_entries_)) {
        bytes_ -= entries_.back().bytes; entries_.pop_back();
    }
    entries_.push_front({std::move(context), std::move(text), size}); bytes_ += size;
}
void Cache::clear() { entries_.clear(); bytes_ = 0; }
}
