#include "pch.h"
import std;
#include "workspace/WorkspaceIcons.h"
#include "theme/ThemeConfig.h"

namespace winrt::Folia {
    WorkspaceIcon WorkspaceFileIcon(WorkspaceFileKind kind) noexcept {
        if (kind == WorkspaceFileKind::Markdown) return WorkspaceIcon::Markdown;
        return IsWorkspaceImage(kind) ? WorkspaceIcon::Image : WorkspaceIcon::Document;
    }

    std::wstring WorkspaceIconXaml(WorkspaceIcon icon, std::wstring_view stroke) {
        constexpr std::wstring_view keys[]{L"sidebar", L"source", L"folder", L"document", L"markdown", L"image",
            L"refresh", L"collapse", L"new-file", L"new-folder", L"outline", L"settings", L"add", L"more", L"split-editor", L"agent-chat",
            L"send-message", L"stop-message", L"read-access", L"expand-choice", L"test-connection", L"provider-service",
            L"settings-model", L"settings-network", L"settings-reasoning",
            L"settings-prediction", L"settings-cache", L"arrow-down", L"chevron-left", L"chevron-right"};
        const auto* glyph = FindThemeGlyph(keys[static_cast<std::size_t>(icon)]);
        if (!glyph) throw std::runtime_error("missing workspace glyph in theme assets");
        return LR"(<Viewbox xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Width="16" Height="16" Stretch="Uniform">
            <Canvas Width="24" Height="24"><Path Stroke="{ThemeResource )" + std::wstring(stroke) + LR"(}"
                StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round" StrokeThickness=")"
            + glyph->strokeWidth + LR"(" Data=")" + glyph->path + LR"("/></Canvas></Viewbox>)";
    }

    Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon, Microsoft::UI::Xaml::Media::Brush const& stroke) {
        auto view = MakeWorkspaceIcon(icon);
        view.Child().as<Microsoft::UI::Xaml::Controls::Canvas>().Children().GetAt(0).as<Microsoft::UI::Xaml::Shapes::Path>().Stroke(stroke);
        return view;
    }

    Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon, std::wstring_view stroke) {
        return Microsoft::UI::Xaml::Markup::XamlReader::Load(WorkspaceIconXaml(icon, stroke)).as<Microsoft::UI::Xaml::Controls::Viewbox>();
    }
}
