#pragma once

#include <filesystem>
#include <wil/win32_helpers.h>

namespace app_tests {
    inline std::filesystem::path RepositoryRoot() {
        return wil::GetEnvironmentVariableW(L"FOLIA_TEST_ROOT").get();
    }
}
