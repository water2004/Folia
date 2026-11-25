#pragma once
#include "workspace/WorkspaceFileKind.h"

namespace winrt::Folia {
    enum class WorkspaceIcon { PanelLeft, Source, Folder, Document, Markdown, Image, Refresh, Collapse, NewFile, NewFolder };
    WorkspaceIcon WorkspaceFileIcon(WorkspaceFileKind kind) noexcept;
    std::wstring WorkspaceIconXaml(WorkspaceIcon icon);
    winrt::Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon);
}
