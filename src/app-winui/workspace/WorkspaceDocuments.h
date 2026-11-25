#pragma once

#include "editor/session/EditorSession.h"
#include "editor/document/EditorDocumentController.h"
#include "windows/media/math/MathJaxRenderer.h"
#include "workspace/WorkspaceFileKind.h"

namespace winrt::Folia {
    enum class WorkspaceOpenMode { Preview, Keep };

    struct WorkspaceEditorDocument {
        EditorSession session;
        EditorDocumentController controller;
        std::optional<folia::platform::MathJaxRenderer> math;
        std::optional<folia::TextPosition> viewportPosition;
        float scrollOffset = 0;
        float viewportInset = 0;
        bool loaded = false;
        bool opening = false;
        bool controllerAttached = false;
        std::uint64_t openSerial = 0;
        std::uint64_t themeRevision = 0;
    };

    struct WorkspaceDocument {
        explicit WorkspaceDocument(std::uint64_t value, bool image = false)
            : id(value), editor(image ? nullptr : std::make_unique<WorkspaceEditorDocument>()) {}

        bool IsModified() const { return editor && editor->controller.IsModified(); }
        const std::uint64_t id;
        const std::unique_ptr<WorkspaceEditorDocument> editor;
        std::filesystem::path path;
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
        WorkspaceOpenResult Open(std::filesystem::path path, WorkspaceOpenMode mode);
        void Activate(std::uint64_t id);
        bool Keep(std::uint64_t id);
        bool ObserveEdit(std::uint64_t id);
        void RefreshPath(std::uint64_t id);
        bool Remove(std::uint64_t id);
        // Move the same session/controller; the source must activate its successor first.
        void TransferTo(std::uint64_t id, WorkspaceDocuments& target, std::size_t index);
        void Reorder(std::span<std::uint64_t const> ids);
        std::shared_ptr<WorkspaceDocument> Adjacent(std::uint64_t id, int direction) const noexcept;

    private:
        static bool SamePath(std::filesystem::path const& left, std::filesystem::path const& right);
        std::vector<std::shared_ptr<WorkspaceDocument>> documents_;
        std::shared_ptr<WorkspaceDocument> active_;
    };
}
