#pragma once
#include "workspace/WorkspaceFileKind.h"

namespace winrt::Folia {
    enum class WorkspaceIcon { PanelLeft, Source, Folder, Document, Markdown, Image, Refresh, Collapse, NewFile, NewFolder, Outline, Settings, Add, More, SplitEditor, Chat, Send, Stop, Shield, Chevron, TestConnection, ProviderService, Model, Network, Reasoning, Prediction, Cache, ArrowDown, ChevronLeft, ChevronRight, TextLayout };
    WorkspaceIcon WorkspaceFileIcon(WorkspaceFileKind kind) noexcept;
    winrt::Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon, winrt::Microsoft::UI::Xaml::Media::Brush const& stroke);
    std::wstring WorkspaceIconXaml(WorkspaceIcon icon, std::wstring_view stroke = L"TextFillColorPrimaryBrush");
    winrt::Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon, std::wstring_view stroke = L"TextFillColorPrimaryBrush");
}
