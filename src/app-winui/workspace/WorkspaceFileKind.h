#pragma once

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <string_view>

namespace winrt::Folia {
    enum class WorkspaceFileKind { Markdown, Text, RasterImage, VectorImage, Other };

    inline constexpr const wchar_t* WorkspaceTextExtensions[]{L".md", L".markdown", L".txt"};
    inline constexpr const wchar_t* WorkspaceRasterExtensions[]{
        L".png", L".jpg", L".jpeg", L".gif", L".bmp", L".tif", L".tiff", L".ico", L".webp"};
    inline constexpr auto WorkspaceVectorExtension = L".svg";

    inline bool IsWorkspaceImage(WorkspaceFileKind kind) noexcept {
        return kind == WorkspaceFileKind::RasterImage || kind == WorkspaceFileKind::VectorImage;
    }

    inline WorkspaceFileKind ClassifyWorkspaceFile(std::filesystem::path const& path) {
        auto extension = path.extension();
        auto matches = [&](const wchar_t* value) { return _wcsicmp(extension.c_str(), value) == 0; };
        if (std::ranges::any_of(WorkspaceRasterExtensions, matches)) return WorkspaceFileKind::RasterImage;
        if (matches(WorkspaceVectorExtension)) return WorkspaceFileKind::VectorImage;
        if (matches(WorkspaceTextExtensions[2])) return WorkspaceFileKind::Text;
        if (matches(WorkspaceTextExtensions[0]) || matches(WorkspaceTextExtensions[1])) return WorkspaceFileKind::Markdown;
        return WorkspaceFileKind::Other;
    }
}
