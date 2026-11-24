#pragma once

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

namespace folia::platform
{
    struct ExecutableLibraryDeleter
    {
        void operator()(HMODULE module) const noexcept { FreeLibrary(module); }
    };

    using ExecutableLibrary =
        std::unique_ptr<std::remove_pointer_t<HMODULE>, ExecutableLibraryDeleter>;

    inline ExecutableLibrary LoadExecutableLibrary(wchar_t const* filename)
    {
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size()) return {};
        executable.resize(length);
        const auto path = std::filesystem::path(executable).parent_path() / filename;
        return ExecutableLibrary(LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    }
}
