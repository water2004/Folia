// folia.core.document_history — reversible source and block-tree operation log.
//
// Normal undo/redo stores only the edits that changed the document. It never
// retains complete EditorDocument snapshots.
export module folia.core.document_history;
import std;
import folia.markdown.document;
import folia.core.document_operation_apply;
import folia.core.document_transaction;
import folia.markdown.text_edit;
import folia.core.source_patch;

export namespace folia {

struct RenderedHistorySelection {
    TextSelection before;
    TextSelection after;
};

struct SourceHistorySelection {
    SourceViewSelection before;
    SourceViewSelection after;
};

using DocumentHistorySelection=std::variant<RenderedHistorySelection,SourceHistorySelection>;

struct DocumentHistoryEntry {
    std::vector<DocumentOperation> operations;
    DocumentHistorySelection selection;
    std::uint64_t revision_before = 0;
    std::uint64_t revision_after = 0;
    std::uint64_t state_before = 0;
    std::uint64_t state_after = 0;
};

namespace document_history_detail {

inline std::uint64_t next_state() noexcept {
    static std::atomic_uint64_t sequence{1};
    return sequence.fetch_add(1, std::memory_order_relaxed);
}

inline DocumentHistoryEntry make_entry(const DocumentTransaction& transaction) {
    DocumentHistoryEntry entry;
    entry.operations = transaction.operations;
    entry.selection = RenderedHistorySelection{transaction.selection_before,transaction.selection_after};
    entry.revision_before = transaction.revision_before;
    entry.revision_after = transaction.revision_after;
    return entry;
}

inline bool apply_entry(EditorDocument& document, TextSelection& selection,
                        const DocumentHistoryEntry& entry, bool forward) {
    if (document.revision!=(forward ? entry.revision_before : entry.revision_after)) return false;
    if (!apply_document_operations(document, entry.operations, forward)) return false;
    if (forward) {
        document.revision = entry.revision_after;
    } else {
        document.revision = entry.revision_before;
    }
    if (const auto* rendered=std::get_if<RenderedHistorySelection>(&entry.selection))
        selection=forward ? rendered->after : rendered->before;
    return true;
}

} // namespace document_history_detail

class DocumentHistory {
public:
    explicit DocumentHistory(std::size_t capacity = 1000) : capacity_(capacity) {}

    void push(const DocumentTransaction& transaction) {
        if (transaction.operations.empty() || transaction.revision_before==transaction.revision_after) return;
        auto entry = document_history_detail::make_entry(transaction);
        entry.state_before = state_;
        entry.state_after = document_history_detail::next_state();
        undo_.push_back(std::move(entry));
        state_ = undo_.back().state_after;
        redo_.clear();
        if (undo_.size() > capacity_) undo_.erase(undo_.begin());
    }

    void push_source(const DocumentTransaction& transaction,
        SourceViewSelection before,SourceViewSelection after) {
        if (transaction.operations.empty() || transaction.revision_before==transaction.revision_after) return;
        push(transaction);
        if (!undo_.empty() && capacity_!=0)
            undo_.back().selection=SourceHistorySelection{before,after};
    }

    bool undo(EditorDocument& document, TextSelection& selection) {
        if (undo_.empty()) return false;
        auto entry = std::move(undo_.back());
        undo_.pop_back();
        if (!document_history_detail::apply_entry(document, selection, entry, false)) {
            undo_.push_back(std::move(entry));
            return false;
        }
        state_ = entry.state_before;
        redo_.push_back(std::move(entry));
        return true;
    }

    bool redo(EditorDocument& document, TextSelection& selection) {
        if (redo_.empty()) return false;
        auto entry = std::move(redo_.back());
        redo_.pop_back();
        if (!document_history_detail::apply_entry(document, selection, entry, true)) {
            redo_.push_back(std::move(entry));
            return false;
        }
        state_ = entry.state_after;
        undo_.push_back(std::move(entry));
        return true;
    }

    void clear() { undo_.clear(); redo_.clear(); state_ = document_history_detail::next_state(); }
    // Revisions can be reused after undo and a new edit. This identity follows
    // the actual history state, so save points do not mistake a new branch for
    // previously saved content. Selection and presentation changes keep it.
    std::uint64_t state() const noexcept { return state_; }
    bool has_undo() const { return !undo_.empty(); }
    bool has_redo() const { return !redo_.empty(); }
    const DocumentHistoryEntry* next_undo() const { return undo_.empty() ? nullptr : &undo_.back(); }
    const DocumentHistoryEntry* next_redo() const { return redo_.empty() ? nullptr : &redo_.back(); }

private:
    std::size_t capacity_;
    std::uint64_t state_ = document_history_detail::next_state();
    std::vector<DocumentHistoryEntry> undo_;
    std::vector<DocumentHistoryEntry> redo_;
};

} // namespace folia
