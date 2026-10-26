#pragma once

namespace winrt::Folia {
    // One directory per request. The tree owns only the entries the user has
    // expanded; enumeration and sorting never run on the XAML thread.
    class FolderExplorerController {
    public:
        using OpenFile = std::function<void(std::filesystem::path, bool keep)>;
        using ReportError = std::function<void(winrt::hstring)>;
        FolderExplorerController();
        ~FolderExplorerController();
        void Attach(winrt::Microsoft::UI::Xaml::Controls::TreeView const& tree,
            OpenFile open, ReportError report);
        void Detach();
        void Open(std::filesystem::path path);
        void Expand(winrt::Microsoft::UI::Xaml::Controls::TreeViewNode const& node);
        void Refresh();
        void Collapse();
        std::filesystem::path RootPath() const;

    private:
        struct State;
        static void OpenRoot(std::shared_ptr<State> const& state, std::filesystem::path path);
        static void RefreshLoadedNodes(std::shared_ptr<State> const& state);
        static void CollapseNodes(std::shared_ptr<State> const& state);
        static winrt::fire_and_forget Populate(std::shared_ptr<State> state,
            winrt::Microsoft::UI::Xaml::Controls::TreeViewNode node);
        std::shared_ptr<State> state_;
    };
}
