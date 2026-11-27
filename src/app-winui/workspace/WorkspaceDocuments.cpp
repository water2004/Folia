#include "pch.h"
import std;
#include "workspace/WorkspaceDocuments.h"
#include "storage/RecordId.h"

namespace winrt::Folia {
    namespace {
        std::filesystem::path WorkspacePath(std::filesystem::path path) {
            path = path.lexically_normal();
            if (path.native().find(L'~') == std::wstring::npos) return path;
            // Shell activation can supply a DOS short name. StorageFile returns the long name on load.
            auto size = ::GetLongPathNameW(path.c_str(), nullptr, 0);
            if (!size) return path;
            std::wstring name(size, L'\0');
            auto written = ::GetLongPathNameW(path.c_str(), name.data(), size);
            if (!written || written >= size) return path;
            name.resize(written);
            return name;
        }
    }

    WorkspaceDocument::WorkspaceDocument(std::uint64_t value, WorkspaceTabKind type)
        : id(value), kind(type), editor(type == WorkspaceTabKind::Editor ? std::make_unique<WorkspaceEditorDocument>() : nullptr),
          referenceKey(type == WorkspaceTabKind::Editor ? records::NewId() : std::string{}) {}

    std::uint64_t AllocateWorkspaceItemId() {
        static std::atomic_uint64_t next{1};
        return next.fetch_add(1, std::memory_order_relaxed);
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
        auto normalized = WorkspacePath(path);
        auto found = std::ranges::find_if(documents_, [&](auto const& document) { return SamePath(document->path, normalized); });
        return found == documents_.end() ? nullptr : *found;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::New(WorkspaceOpenMode mode) {
        auto document = std::make_shared<WorkspaceDocument>(AllocateWorkspaceItemId());
        document->kept = mode != WorkspaceOpenMode::Preview;
        document->editor->loaded = true;
        documents_.push_back(document);
        return document;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::FindSettings() const noexcept {
        auto found = std::ranges::find(documents_, WorkspaceTabKind::Settings, [](auto const& doc) { return doc->kind; });
        return found == documents_.end() ? nullptr : *found;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::OpenSettings() {
        if (auto existing = FindSettings()) return existing;
        auto settings = std::make_shared<WorkspaceDocument>(AllocateWorkspaceItemId(), WorkspaceTabKind::Settings);
        settings->kept = true;
        documents_.push_back(settings);
        return settings;
    }

    std::shared_ptr<WorkspaceDocument> WorkspaceDocuments::FindChat(std::string_view id) const noexcept {
        auto found = std::ranges::find_if(documents_, [&](auto const& doc) {
            return doc->kind == WorkspaceTabKind::Chat && doc->chatId == id;
        });
        return found == documents_.end() ? nullptr : *found;
    }

    WorkspaceOpenResult WorkspaceDocuments::OpenChat(std::string id, std::string title, WorkspaceOpenMode mode, PreviewFilter const& filter) {
        if (auto existing = FindChat(id)) {
            if (mode != WorkspaceOpenMode::Preview) existing->kept = true;
            return {std::move(existing), {}, false};
        }
        auto chat = std::make_shared<WorkspaceDocument>(AllocateWorkspaceItemId(), WorkspaceTabKind::Chat);
        chat->chatId = std::move(id);
        chat->chatTitle = std::move(title);
        return Place(std::move(chat), mode, filter);
    }

    WorkspaceOpenResult WorkspaceDocuments::Open(std::filesystem::path path, WorkspaceOpenMode mode, PreviewFilter const& filter) {
        if (path.empty() || !path.is_absolute()) throw std::invalid_argument("Workspace file paths must be absolute");
        path = WorkspacePath(std::move(path));
        if (auto existing = Find(path)) {
            if (mode != WorkspaceOpenMode::Preview || existing->IsModified()) existing->kept = true;
            return {std::move(existing), {}, false};
        }
        auto kind = IsWorkspaceImage(ClassifyWorkspaceFile(path)) ? WorkspaceTabKind::Image : WorkspaceTabKind::Editor;
        auto document = std::make_shared<WorkspaceDocument>(AllocateWorkspaceItemId(), kind);
        document->path = std::move(path);
        return Place(std::move(document), mode, filter);
    }

    WorkspaceOpenResult WorkspaceDocuments::Place(std::shared_ptr<WorkspaceDocument> document, WorkspaceOpenMode mode,
        PreviewFilter const& canReplacePreview) {
        document->kept = mode != WorkspaceOpenMode::Preview;
        for (auto& candidate : documents_) {
            if (candidate->IsModified()) candidate->kept = true;
            if (candidate->kept || (canReplacePreview && !canReplacePreview(candidate->id))) continue;
            if (mode == WorkspaceOpenMode::NewTab && (candidate->kind != WorkspaceTabKind::Editor || !candidate->path.empty())) continue;
            auto replaced = candidate->id;
            candidate = document;
            return {std::move(document), replaced, true};
        }
        documents_.push_back(document);
        return {std::move(document), {}, true};
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
        return document && document->IsModified() && Keep(id);
    }

    void WorkspaceDocuments::RefreshPath(std::uint64_t id) {
        if (auto document = Find(id); document && document->editor && document->editor->session.HasFile())
            document->path = std::filesystem::path(document->editor->session.Path().c_str()).lexically_normal();
    }

    bool WorkspaceDocuments::Remove(std::uint64_t id) {
        auto found = std::ranges::find(documents_, id, [](auto const& document) { return document->id; });
        if (found == documents_.end()) return false;
        if (active_ && active_->id == id)
            throw std::logic_error("Activate the replacement document before closing the current document");
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

    void WorkspaceDocuments::TransferTo(std::uint64_t id, WorkspaceDocuments& target, std::size_t index, bool keepSource) {
        auto document = Find(id);
        if (!document || this == &target || (!keepSource && document == active_))
            throw std::logic_error("Transfer requires an inactive document and a different workspace");
        if (target.Find(id) == document) {
            if (!keepSource) std::erase(documents_, document);
            return;
        }
        if (target.Find(id) || (!document->path.empty() && target.Find(document->path))
            || (document->kind == WorkspaceTabKind::Settings && target.FindSettings()))
            throw std::logic_error("The target workspace already owns this document");
        // Allocate before changing either owner. Shared-pointer moves cannot throw.
        target.documents_.reserve(target.documents_.size() + 1);
        target.documents_.insert(target.documents_.begin() + (std::min)(index, target.documents_.size()), document);
        if (!keepSource) std::erase(documents_, document);
    }
}
