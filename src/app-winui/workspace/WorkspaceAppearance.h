#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace winrt::Folia {
    enum class WindowMaterial { MicaAlt, Mica, Acrylic, Solid };

    struct WindowMaterialChoice {
        WindowMaterial value;
        std::wstring_view id;
        const wchar_t* label;
    };
    inline constexpr std::array windowMaterials{
        WindowMaterialChoice{WindowMaterial::MicaAlt, L"mica-alt", L"MaterialMicaAlt"},
        WindowMaterialChoice{WindowMaterial::Mica, L"mica", L"MaterialMica"},
        WindowMaterialChoice{WindowMaterial::Acrylic, L"acrylic", L"MaterialAcrylic"},
        WindowMaterialChoice{WindowMaterial::Solid, L"solid", L"MaterialSolid"},
    };

    struct WorkspaceAppearance {
        bool autoHideTools = false;
        WindowMaterial material = WindowMaterial::MicaAlt;
        std::optional<WindowMaterial> readingMaterial;
        bool operator==(WorkspaceAppearance const&) const = default;
    };
}
