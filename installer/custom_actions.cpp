#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msiquery.h>
#include <wincred.h>

// A commit action runs only after removal succeeds, in the installing user's
// context. CustomActionData supplies the credential target from the package.
extern "C" __declspec(dllexport) UINT __stdcall RemoveCompletionCredential(MSIHANDLE install) noexcept {
    wchar_t target[CRED_MAX_GENERIC_TARGET_NAME_LENGTH + 1]{};
    DWORD length = ARRAYSIZE(target);
    const auto status = MsiGetPropertyW(install, L"CustomActionData", target, &length);
    if (status != ERROR_SUCCESS || length == 0) return ERROR_INSTALL_FAILURE;
    if (CredDeleteW(target, CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
        return ERROR_SUCCESS;
    const auto error = GetLastError();
    if (const auto record = MsiCreateRecord(1)) {
        MsiRecordSetStringW(record, 0, L"Unable to delete the saved Folia API key. Windows error [1].");
        MsiRecordSetInteger(record, 1, error);
        MsiProcessMessage(install, INSTALLMESSAGE_ERROR, record);
        MsiCloseHandle(record);
    }
    return ERROR_INSTALL_FAILURE;
}
