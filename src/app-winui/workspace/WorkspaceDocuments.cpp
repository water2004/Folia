#include "pch.h"
import std;
#include "workspace/WorkspaceDocuments.h"

namespace winrt::Folia {
    namespace {
        std::uint64_t NextDocumentId() {
            static std::atomic_uint64_t next{1};
            return next.fetch_add(1, std::memory_order_relaxed);
        }
    }
    WorkspaceDocuments::WorkspaceDocuments() {
        active_ = New(WorkspaceOpenMode::Preview);
    }

    std::span<std::shared_ptr<WorkspaceDocument> const> WorkspaceDocuments::Items() const noexcept {
        return documents_;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::Active() const noexcept { return active_; }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::Find(std::uint64_t id) const noexcept {
        auto found = std::ranges::find(documents_, id, [](auto const& document) { return document->id; });
        return found == documents_.end() ? nullptr : *found;
    }

    bool WorkspaceDocuments::SamePath(std::filesystem::path const& left, std::filesystem::path const& right) {
        return !left.empty() && !right.empty()
            && ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::Find(std::filesystem::path const& path) const {
        auto normalized = path.lexically_normal();
        auto found = std::ranges::find_if(documents_, [&](auto const& document) { return SamePath(document->path, normalized); });
        return found == documents_.end() ? nullptr : *found;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::New(WorkspaceOpenMode mode) {
        auto document = std::make_shared<WorkspaceDocument>(NextDocumentId());
        document->kept = mode == WorkspaceOpenMode::Keep;
        document->loaded = true;
        documents_.push_back(document);
        return document;
    }

    WorkspaceOpenResult WorkspaceDocuments::Open(std::filesystem::path path, WorkspaceOpenMode mode) {
        if (path.empty() || !path.is_absolute()) throw std::invalid_argument("Workspace file paths must be absolute");
        path = path.lexically_normal();
        for (auto const& document : documents_)
            if (!document->kept && document->controller.IsModified()) document->kept = true;
        if (auto document = Find(path)) {
            if (mode == WorkspaceOpenMode::Keep) document->kept = true;
            return {std::move(document), {}, false};
        }
        std::optional<std::uint64_t> replaced;
        auto preview = std::ranges::find_if(documents_, [](auto const& document) {
            return !document->kept && !document->controller.IsModified();
        });
        auto document = std::make_shared<WorkspaceDocument>(NextDocumentId());
        document->path = std::move(path);
        document->kept = mode == WorkspaceOpenMode::Keep;
        if (preview != documents_.end()) {
            replaced = (*preview)->id;
            *preview = document;
        } else documents_.push_back(document);
        return {std::move(document), replaced, true};
    }

    void WorkspaceDocuments::Activate(std::uint64_t id) {
        auto document = Find(id);
        if (!document) throw std::out_of_range("Workspace document is not open");
        active_ = std::move(document);
    }

    bool WorkspaceDocuments::Keep(std::uint64_t id) {
        auto document = Find(id);
        if (!document || document->kept) return false;
        document->kept = true;
        return true;
    }

    bool WorkspaceDocuments::ObserveEdit(std::uint64_t id) {
        auto document = Find(id);
        return document && document->controller.IsModified() && Keep(id);
    }

    void WorkspaceDocuments::RefreshPath(std::uint64_t id) {
        if (auto document = Find(id); document && document->session.HasFile())
            document->path = std::filesystem::path(document->session.Path().c_str()).lexically_normal();
    }

    bool WorkspaceDocuments::Remove(std::uint64_t id) {
        auto found = std::ranges::find(documents_, id, [](auto const& document) { return document->id; });
        if (found == documents_.end()) return false;
        if (active_ && active_->id == id)
            throw std::logic_error("Activate the replacement document before closing the current document");
        (*found)->controller.Detach();
        documents_.erase(found);
        return true;
    }

    void WorkspaceDocuments::Reorder(std::span<std::uint64_t const> ids) {
        if (ids.size() != documents_.size()) throw std::invalid_argument("Workspace order must contain every document");
        std::vector<std::shared_ptr<WorkspaceDocument>> ordered;
        ordered.reserve(ids.size());
        for (auto id : ids) {
            auto document = Find(id);
            if (!document || std::ranges::find(ordered, document) != ordered.end())
                throw std::invalid_argument("Workspace order contains an unknown or repeated document");
            ordered.push_back(std::move(document));
        }
        documents_ = std::move(ordered);
    }

    void WorkspaceDocuments::TransferTo(std::uint64_t id, WorkspaceDocuments& target, std::size_t index) {
        auto document = Find(id);
        if (!document || this == &target || document == active_)
            throw std::logic_error("Transfer requires an inactive document and a different workspace");
        if (target.Find(id) || (!document->path.empty() && target.Find(document->path)))
            throw std::logic_error("The target workspace already owns this document");
        // Allocate before changing either owner. Shared-pointer moves cannot throw.
        target.documents_.reserve(target.documents_.size() + 1);
        target.documents_.insert(target.documents_.begin() + (std::min)(index, target.documents_.size()), document);
        std::erase(documents_, document);
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::Adjacent(std::uint64_t id, int direction) const noexcept {
        auto found = std::ranges::find(documents_, id, [](auto const& document) { return document->id; });
        if (found == documents_.end()) return nullptr;
        auto index = static_cast<std::size_t>(found - documents_.begin());
        index = direction < 0 ? (index + documents_.size() - 1) % documents_.size() : (index + 1) % documents_.size();
        return documents_[index];
    }
}
