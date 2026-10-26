#pragma once
namespace winrt::Folia {
    // Row presentation only; enumeration, actions and lifetime live in the controller.
    Microsoft::UI::Xaml::Controls::TreeViewNode ExplorerNodeFromElement(
        Microsoft::UI::Xaml::Controls::TreeView const& tree, Microsoft::UI::Xaml::DependencyObject element);
    Microsoft::UI::Xaml::Controls::TextBox ExplorerNameEditorFromElement(Microsoft::UI::Xaml::DependencyObject element);
    void UpdateFolderExplorerRow(Microsoft::UI::Xaml::Controls::TreeView const& tree,
        Microsoft::UI::Xaml::Controls::TreeViewNode const& node);
    void ConfigureFolderExplorerView(Microsoft::UI::Xaml::Controls::TreeView const& tree);
    void UpdateFolderExplorerGuides(Microsoft::UI::Xaml::Controls::TreeView const& tree,
        Microsoft::UI::Xaml::Controls::TreeViewNode const& activeBranch, bool visible);
}
