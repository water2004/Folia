#pragma once

#include "editor/session/EditorSession.h"
#include "editor/document/EditorDocumentController.h"
#include "windows/media/math/MathJaxRenderer.h"
#include "workspace/WorkspaceFileKind.h"
#include "editor/session/EditorSourceLocation.h"

namespace winrt::Folia {
    enum class WorkspaceOpenMode { Preview, Keep, NewTab };
    enum class WorkspaceTabKind { Editor, Image, Settings, Chat };

    std::uint64_t AllocateWorkspaceItemId();

    struct WorkspaceEditorDocument {
        EditorSession session;
        EditorDocumentController controller;
        std::optional<folia::platform::MathJaxRenderer> math;
        bool loaded = false;
        bool opening = false;
        bool controllerAttached = false;
        HWND presenter = nullptr;
        std::uint64_t openSerial = 0;
        std::optional<EditorSourceLocation> pendingLocation;
        std::uint64_t themeRevision = 0;
    };

    struct WorkspaceDocument {
        explicit WorkspaceDocument(std::uint64_t value, WorkspaceTabKind type = WorkspaceTabKind::Editor);

        bool IsModified() const { return editor && editor->controller.IsModified(); }
        const std::uint64_t id;
        const WorkspaceTabKind kind;
        const std::unique_ptr<WorkspaceEditorDocument> editor;
        std::filesystem::path path;
        const std::string referenceKey;
        std::string chatId, chatTitle;
        bool kept = false;
    };

    struct WorkspaceOpenResult {
        std::shared_ptr<WorkspaceDocument> document;
        std::optional<std::uint64_t> replaced;
        bool created = false;
    };

    // Own document sessions, not controls or a serialized copy of their source.
    // Activation is explicit so the shell can leave the old input context
    // before binding the shared editor surface to a different session.
    class WorkspaceDocuments {
    public:
        WorkspaceDocuments();
        std::span<std::shared_ptr<WorkspaceDocument> const> Items() const noexcept;
        std::shared_ptr<WorkspaceDocument> Active() const noexcept;
        std::shared_ptr<WorkspaceDocument> Find(std::uint64_t id) const noexcept;
        std::shared_ptr<WorkspaceDocument> Find(std::filesystem::path const& path) const;
        std::shared_ptr<WorkspaceDocument> New(WorkspaceOpenMode mode = WorkspaceOpenMode::Keep);
        std::shared_ptr<WorkspaceDocument> FindSettings() const noexcept;
        std::shared_ptr<WorkspaceDocument> OpenSettings();
        std::shared_ptr<WorkspaceDocument> FindChat(std::string_view id) const noexcept;
        using PreviewFilter = std::function<bool(std::uint64_t)>;
        WorkspaceOpenResult OpenChat(std::string id, std::string title, WorkspaceOpenMode, PreviewFilter const& = {});
        WorkspaceOpenResult Open(std::filesystem::path path, WorkspaceOpenMode mode,
            PreviewFilter const& canReplacePreview = {});
        void Activate(std::uint64_t id);
        bool Keep(std::uint64_t id);
        bool ObserveEdit(std::uint64_t id);
        void RefreshPath(std::uint64_t id);
        bool Remove(std::uint64_t id);
        // Keep a shared session when other views remain in the source window.
        void TransferTo(std::uint64_t id, WorkspaceDocuments& target, std::size_t index, bool keepSource = false);
        void Reorder(std::span<std::uint64_t const> ids);

    private:
        WorkspaceOpenResult Place(std::shared_ptr<WorkspaceDocument>, WorkspaceOpenMode, PreviewFilter const&);
        static bool SamePath(std::filesystem::path const& left, std::filesystem::path const& right);
        std::vector<std::shared_ptr<WorkspaceDocument>> documents_;
        std::shared_ptr<WorkspaceDocument> active_;
    };
}
