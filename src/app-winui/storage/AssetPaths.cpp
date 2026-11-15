#include "pch.h"
#include "storage/AssetPaths.h"

#ifndef FOLIA_ASSETS_DIRECTORY
#define FOLIA_ASSETS_DIRECTORY L""
#endif

namespace
{
    std::filesystem::path ExecutableDirectory()
    {
        std::wstring executable(32768, L'\0');
        auto const length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size()) winrt::throw_last_error();
        executable.resize(length);
        return std::filesystem::path(executable).parent_path();
    }

    std::wstring ExpandKnownFolderTokens(std::wstring configured)
    {
        constexpr std::wstring_view token = L"{LocalAppData}";
        constexpr std::wstring_view environment_reference = L"%LOCALAPPDATA%";
        for (auto position = configured.find(token);
             position != std::wstring::npos;
             position = configured.find(token, position + environment_reference.size()))
        {
            configured.replace(position, token.size(), environment_reference);
        }
        return configured;
    }

    std::filesystem::path ExpandEnvironment(std::wstring const& configured)
    {
        auto const token_expanded = ExpandKnownFolderTokens(configured);
        auto required = ExpandEnvironmentStringsW(token_expanded.c_str(), nullptr, 0);
        if (required == 0) winrt::throw_last_error();
        std::wstring expanded(required, L'\0');
        auto written = ExpandEnvironmentStringsW(token_expanded.c_str(), expanded.data(), required);
        if (written == 0 || written > required) winrt::throw_last_error();
        expanded.resize(written - 1);
        return expanded;
    }
}

namespace winrt::Folia
{
    std::filesystem::path AssetsDirectory()
    {
        static auto const directory = []
        {
            std::wstring configured = FOLIA_ASSETS_DIRECTORY;
            constexpr std::wstring_view executablePrefix = L"{ExecutableDirectory}/";
            if (configured.starts_with(executablePrefix))
                return ExecutableDirectory() / configured.substr(executablePrefix.size());
            if (!configured.empty()) return ExpandEnvironment(configured);
            return std::filesystem::current_path() / L"Assets";
        }();
        return directory;
    }

    std::filesystem::path AssetPath(std::filesystem::path const& relative)
    {
        if (relative.is_absolute())
            throw std::invalid_argument("asset paths must be relative to the Assets directory");
        return AssetsDirectory() / relative;
    }
}
