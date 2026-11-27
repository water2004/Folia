#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msiquery.h>
#include <wincred.h>
#include <cwchar>

namespace {
    DWORD DeleteKey(wchar_t const* target) noexcept {
        if (CredDeleteW(target, CRED_TYPE_GENERIC, 0)) return ERROR_SUCCESS;
        auto error = GetLastError();
        return error == ERROR_NOT_FOUND ? ERROR_SUCCESS : error;
    }

    DWORD DeleteKeys(wchar_t const* filter) noexcept {
        DWORD count = 0;
        PCREDENTIALW* entries = nullptr;
        if (!CredEnumerateW(filter, 0, &count, &entries)) {
            auto error = GetLastError();
            return error == ERROR_NOT_FOUND ? ERROR_SUCCESS : error;
        }
        DWORD error = ERROR_SUCCESS;
        for (DWORD i = 0; i < count; ++i) {
            if (entries[i]->Type != CRED_TYPE_GENERIC) continue;
            if (auto failure = DeleteKey(entries[i]->TargetName); failure && !error) error = failure;
        }
        CredFree(entries);
        return error;
    }
}

// The commit action runs in the installing user's context. CustomActionData
// supplies the inline-completion target and the agent profile credential filter.
extern "C" __declspec(dllexport) UINT __stdcall RemoveCompletionCredential(MSIHANDLE install) noexcept {
    wchar_t target[2 * CRED_MAX_GENERIC_TARGET_NAME_LENGTH + 2]{};
    DWORD length = ARRAYSIZE(target);
    const auto status = MsiGetPropertyW(install, L"CustomActionData", target, &length);
    if (status != ERROR_SUCCESS || length == 0) return ERROR_INSTALL_FAILURE;
    auto filter = std::wcschr(target, L'|');
    if (filter) *filter++ = L'\0';
    auto error = DeleteKey(target);
    if (filter && *filter) {
        auto failure = DeleteKeys(filter);
        if (!error) error = failure;
    }
    if (!error) return ERROR_SUCCESS;
    if (const auto record = MsiCreateRecord(1)) {
        MsiRecordSetStringW(record, 0, L"Unable to delete the saved Folia API key. Windows error [1].");
        MsiRecordSetInteger(record, 1, error);
        MsiProcessMessage(install, INSTALLMESSAGE_ERROR, record);
        MsiCloseHandle(record);
    }
    return ERROR_INSTALL_FAILURE;
}
