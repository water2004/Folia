#pragma once

namespace winrt::Folia {
    enum class WorkspaceFileActionKind { NewFile, NewFolder, Rename, Move, Copy };
    struct WorkspaceFileAction {
        WorkspaceFileActionKind kind;
        std::filesystem::path source;
        std::filesystem::path target;
        bool directory = false;
    };

    // Filename validation and path projection perform no filesystem IO.
    bool ValidWorkspaceEntryName(std::wstring_view name) noexcept;
    std::optional<std::filesystem::path> RelocatedWorkspacePath(
        WorkspaceFileAction const& action, std::filesystem::path const& path);
    // Exclusive creation/publication: an existing destination is never replaced.
    // Enumeration/copy/move work runs off the calling UI thread.
    Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyWorkspaceFileActionAsync(WorkspaceFileAction action);
}
