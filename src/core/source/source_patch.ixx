// Reversible text patches at the source-view boundary. These offsets never
// become positions inside the document's block-local editing model.
export module folia.core.source_patch;
import std;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.text_edit;

export namespace folia {

struct SourceSelection {
    std::size_t anchor=0;
    std::size_t active=0;
    TextAffinity anchor_affinity=TextAffinity::Downstream;
    TextAffinity active_affinity=TextAffinity::Downstream;
    static SourceSelection caret(std::size_t offset,TextAffinity affinity=TextAffinity::Downstream) {
        return {offset,offset,affinity,affinity};
    }
    bool is_caret() const { return anchor==active; }
    SourceRange ordered_range() const { return {(std::min)(anchor,active),(std::max)(anchor,active)}; }
    bool operator==(const SourceSelection&) const=default;
};

struct SourceTextPatch {
    SourceRange range_before;
    std::u32string inserted;
    std::u32string removed;
};

struct SourcePatch {
    // Disjoint edits in descending source order, all relative to the before
    // state. Reverse replay restores their offsets without a second mapping.
    std::vector<SourceTextPatch> edits;
    SourceSelection selection_before;
    SourceSelection selection_after;
};

// A block-tree replay can project a whole block although only a few characters
// changed. Keep untouched source lines (and their presentation ids) outside the
// replacement, rather than discarding and recreating them with the block region.
inline std::optional<SourceTextPatch> minimal_source_replacement(
    SourceRange range, std::u32string_view before, std::u32string_view after) {
    if (range.length() != before.size()) throw std::invalid_argument("Source replacement range does not match its text");
    std::size_t prefix = 0, old_end = before.size(), new_end = after.size();
    while (prefix < old_end && prefix < new_end && before[prefix] == after[prefix]) ++prefix;
    while (old_end > prefix && new_end > prefix && before[old_end - 1] == after[new_end - 1]) { --old_end; --new_end; }
    if (prefix == old_end && prefix == new_end) return std::nullopt;
    return SourceTextPatch{{range.start + prefix, range.start + old_end},
        std::u32string(after.substr(prefix, new_end - prefix)), std::u32string(before.substr(prefix, old_end - prefix))};
}

struct SourceReplacement {
    SourceRange range;
    std::u32string replacement;
};

inline std::ptrdiff_t source_patch_delta(const SourcePatch& patch) {
    std::ptrdiff_t result=0;
    for (const auto& edit:patch.edits)
        result+=static_cast<std::ptrdiff_t>(edit.inserted.size())-static_cast<std::ptrdiff_t>(edit.removed.size());
    return result;
}

inline bool validate_source_patch(std::u32string_view source,const SourcePatch& patch,bool forward) {
    const auto& current_selection=forward ? patch.selection_before : patch.selection_after;
    if (current_selection.anchor>source.size() || current_selection.active>source.size()) return false;
    std::ptrdiff_t lower_delta=0;
    for (std::size_t i=patch.edits.size();i>0;--i) {
        const auto& edit=patch.edits[i-1];
        if (edit.range_before.end<edit.range_before.start || edit.range_before.length()!=edit.removed.size()) return false;
        if (i<patch.edits.size()) {
            const auto& lower=patch.edits[i];
            if (lower.range_before.end>edit.range_before.start
                || lower.range_before.start==edit.range_before.start) return false;
        }
        const auto signed_start=static_cast<std::ptrdiff_t>(edit.range_before.start)+(forward ? 0 : lower_delta);
        const auto& expected=forward ? edit.removed : edit.inserted;
        if (signed_start<0) return false;
        const auto start=static_cast<std::size_t>(signed_start);
        if (start>source.size() || expected.size()>source.size()-start || source.substr(start,expected.size())!=expected)
            return false;
        lower_delta+=static_cast<std::ptrdiff_t>(edit.inserted.size())-static_cast<std::ptrdiff_t>(edit.removed.size());
    }
    const auto& selection=forward ? patch.selection_after : patch.selection_before;
    const auto new_size=static_cast<std::ptrdiff_t>(source.size())+(forward ? lower_delta : -lower_delta);
    return new_size>=0 && selection.anchor<=static_cast<std::size_t>(new_size)
        && selection.active<=static_cast<std::size_t>(new_size);
}

inline bool apply_source_patch(std::u32string& source,const SourcePatch& patch,bool forward=true) {
    if (!validate_source_patch(source,patch,forward)) return false;
    if (forward) {
        for (const auto& edit:patch.edits)
            source.replace(edit.range_before.start,edit.removed.size(),edit.inserted);
    } else {
        for (auto i=patch.edits.size();i>0;--i) {
            const auto& edit=patch.edits[i-1];
            source.replace(edit.range_before.start,edit.inserted.size(),edit.removed);
        }
    }
    return true;
}

// Saved source-view caret transport, relative to a direct block's emitted
// body. This can name syntax/separators; it is not an editable-owner position.
struct SourceViewAnchor {
    NodeId block{};
    std::ptrdiff_t offset=0;
    TextAffinity affinity=TextAffinity::Downstream;
};

struct SourceViewSelection {
    SourceViewAnchor anchor;
    SourceViewAnchor active;
};

} // namespace folia
