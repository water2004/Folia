#pragma once
namespace winrt::Folia {
    // Row presentation only; enumeration, actions and lifetime live in the controller.
    void ConfigureFolderExplorerView(Microsoft::UI::Xaml::Controls::TreeView const& tree);
}
