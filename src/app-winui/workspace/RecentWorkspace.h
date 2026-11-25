#pragma once

#include <filesystem>
#include <vector>

namespace winrt::Folia {
    enum class RecentWorkspaceKind { File, Folder };

    struct RecentWorkspaceEntries {
        std::vector<std::filesystem::path> files;
        std::vector<std::filesystem::path> folders;
    };

    class RecentWorkspace {
    public:
        static constexpr std::size_t MaximumEntries = 12;

        RecentWorkspace();
        explicit RecentWorkspace(std::filesystem::path file);

        RecentWorkspaceEntries Read() const;
        void Remember(RecentWorkspaceKind kind, std::filesystem::path const& path) const;
        bool ForgetMissing(RecentWorkspaceKind kind, std::filesystem::path const& path, winrt::hresult error) const;

    private:
        void Update(RecentWorkspaceKind kind, std::filesystem::path path, bool remember) const;
        std::filesystem::path file_;
    };
}
