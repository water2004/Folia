#pragma once

namespace winrt::Folia {
    enum class WorkspaceIcon { PanelLeft, PanelRight, Folder, Document, Refresh, Collapse };
    std::wstring_view WorkspaceIconPath(WorkspaceIcon icon) noexcept;
    winrt::Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon);
}
