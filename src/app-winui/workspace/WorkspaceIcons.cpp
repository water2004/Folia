#include "pch.h"
import std;
#include "workspace/WorkspaceIcons.h"
#include "theme/ThemeConfig.h"

namespace winrt::Folia {
    WorkspaceIcon WorkspaceFileIcon(WorkspaceFileKind kind) noexcept {
        if (kind == WorkspaceFileKind::Markdown) return WorkspaceIcon::Markdown;
        return IsWorkspaceImage(kind) ? WorkspaceIcon::Image : WorkspaceIcon::Document;
    }

    std::wstring WorkspaceIconXaml(WorkspaceIcon icon) {
        constexpr std::wstring_view keys[]{L"sidebar", L"source", L"folder", L"document", L"markdown", L"image",
            L"refresh", L"collapse", L"new-file", L"new-folder", L"outline", L"settings", L"add", L"more", L"split-editor"};
        const auto* glyph = FindThemeGlyph(keys[static_cast<std::size_t>(icon)]);
        if (!glyph) throw std::runtime_error("missing workspace glyph in theme assets");
        return LR"(<Viewbox xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Width="16" Height="16" Stretch="Uniform">
            <Canvas Width="24" Height="24"><Path Stroke="{ThemeResource TextFillColorPrimaryBrush}"
                StrokeStartLineCap="Round" StrokeEndLineCap="Round" StrokeLineJoin="Round" StrokeThickness=")"
            + glyph->strokeWidth + LR"(" Data=")" + glyph->path + LR"("/></Canvas></Viewbox>)";
    }

    Microsoft::UI::Xaml::Controls::Viewbox MakeWorkspaceIcon(WorkspaceIcon icon) {
        return Microsoft::UI::Xaml::Markup::XamlReader::Load(WorkspaceIconXaml(icon)).as<Microsoft::UI::Xaml::Controls::Viewbox>();
    }
}
