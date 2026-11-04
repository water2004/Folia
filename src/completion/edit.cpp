module;
#include <nlohmann/json.hpp>
module folia.completion;
import std;

namespace folia::completion {
namespace {
// JSON has already validated Unicode. Keep conversion local to the protocol
// boundary; proposal ranges and matching consistently count Unicode scalars.
std::u32string scalars(std::string_view text) {
    std::u32string out;
    for (std::size_t at = 0; at < text.size();) {
        auto lead = static_cast<unsigned char>(text[at++]);
        if (lead < 0x80) { out.push_back(lead); continue; }
        auto extra = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
        char32_t value = lead & (extra == 1 ? 0x1f : extra == 2 ? 0x0f : 0x07);
        while (extra--) value = (value << 6) | (static_cast<unsigned char>(text[at++]) & 0x3f);
        out.push_back(value);
    }
    return out;
}
struct Alignment {
    Edit const& edit;
    std::size_t width;
    std::vector<std::uint16_t> lengths;
    explicit Alignment(Edit const& value) : edit(value), width(value.after.size() + 1) {
        if (edit.before.size() > 128 || edit.after.size() > 512)
            throw std::invalid_argument("Completion edit exceeds its bounded window.");
        lengths.resize((edit.before.size() + 1) * width);
        for (auto i = edit.before.size(); i-- > 0;)
            for (auto j = edit.after.size(); j-- > 0;)
                lengths[i * width + j] = edit.before[i] == edit.after[j]
                    ? 1 + get(i + 1, j + 1) : std::max(get(i + 1, j), get(i, j + 1));
    }
    std::uint16_t get(std::size_t i, std::size_t j) const { return lengths[i * width + j]; }
    bool equal(std::size_t i, std::size_t j) const {
        return i < edit.before.size() && j < edit.after.size() && edit.before[i] == edit.after[j];
    }
    void step(std::size_t& i, std::size_t& j) const {
        if (equal(i, j)) { ++i; ++j; }
        else if (i < edit.before.size() && (j == edit.after.size() || get(i + 1, j) >= get(i, j + 1))) ++i;
        else ++j;
    }
};
}
std::optional<Edit> decode_edit_response(std::u32string_view response, Context const& context, Budget budget) {
    if (response.empty()) return Edit{};
    if (response.size() > 8192) return {};
    auto json = nlohmann::json::parse(response.begin(), response.end(), nullptr, false);
    if (json.is_discarded() || !json.is_object() || json.size() != 2
        || !json.contains("oldText") || !json["oldText"].is_string()
        || !json.contains("newText") || !json["newText"].is_string()) return {};
    const auto old = json["oldText"].get<std::string>();
    if (!context.suffix.starts_with(old)) return {};
    Edit result{scalars(old), {}};
    const auto generated = scalars(json["newText"].get<std::string>());
    for (std::size_t i = 0; i < generated.size(); ++i) {
        if (generated[i] != U'\r') result.after += generated[i];
        else {
            result.after += U'\n';
            if (i + 1 < generated.size() && generated[i + 1] == U'\n') ++i;
        }
    }
    if (result.before.size() > 128 || result.after.size() > std::min(budget.proposal_characters, std::size_t{512})) return {};
    unsigned lines = 1;
    for (auto c : result.after) {
        if ((c < 32 && c != U'\n' && c != U'\t') || (c == U'\n' && ++lines > 4)) return {};
    }
    return result;
}
std::vector<Change> diff(Edit const& edit) {
    Alignment alignment(edit);
    std::vector<Change> result;
    std::size_t i = 0, j = 0;
    while (i < edit.before.size() || j < edit.after.size()) {
        if (alignment.equal(i, j)) { ++i; ++j; continue; }
        Change change{i, i, j, j};
        do { alignment.step(i, j); }
        while ((i < edit.before.size() || j < edit.after.size()) && !alignment.equal(i, j));
        change.before_end = i; change.after_end = j;
        result.push_back(change);
    }
    return result;
}
Edit prefix_edit(Edit const& edit, std::size_t count) {
    count = std::min(count, edit.after.size());
    if (count == edit.after.size()) return edit;
    Alignment alignment(edit);
    std::size_t i = 0, j = 0;
    while (j < count) alignment.step(i, j);
    return {edit.before.substr(0, i), edit.after.substr(0, count)};
}
void Proposal::set(std::uint64_t owner, std::size_t offset, Edit edit) {
    if (edit.before.size() > 128 || edit.after.size() > 512)
        throw std::invalid_argument("Completion edit exceeds its bounded window.");
    owner_ = owner; offset_ = offset; edit_ = std::move(edit); ++generation_;
}
void Proposal::clear() {
    if (active()) { edit_ = {}; ++generation_; }
}
bool Proposal::at(std::uint64_t owner, std::size_t offset) const {
    return active() && owner_ == owner && offset_ == offset;
}
bool Proposal::consume(std::uint64_t owner, std::size_t offset, std::u32string_view inserted) {
    if (!at(owner, offset) || inserted.empty() || !edit_.after.starts_with(inserted)) { clear(); return false; }
    auto accepted = prefix_edit(edit_, inserted.size());
    edit_.before.erase(0, accepted.before.size()); edit_.after.erase(0, inserted.size());
    offset_ += inserted.size(); ++generation_; return true;
}
}
