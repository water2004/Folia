#pragma once

namespace winrt::Folia {
    enum class WorkspaceIcon { PanelLeft, PanelRight, Folder, Document, Refresh, Collapse, NewFile, NewFolder };
    std::wstring WorkspaceIconXaml(WorkspaceIcon icon);
    winrt::Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon);
}
