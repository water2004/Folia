// Console tests and probes must report failures without opening desktop UI.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dbghelp.h>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <typeinfo>
#include <crtdbg.h>

// Configure the CRT before test suites and probe globals are initialized.
#pragma init_seg(lib)
#pragma comment(lib, "Dbghelp.lib")
namespace {
constexpr wchar_t crashDumpEnvironment[] = L"FOLIA_TEST_CRASH_DUMP";
wchar_t crashDumpPath[32768]{};

bool isAbsoluteWindowsPath(const wchar_t* path) noexcept {
    if (!path) return false;
    if (path[0] == L'\\' && path[1] == L'\\') return true;
    return ((path[0] >= L'A' && path[0] <= L'Z') ||
            (path[0] >= L'a' && path[0] <= L'z')) &&
           path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}

void printExceptionAddress(void const* address) noexcept {
    HMODULE module = nullptr;
    if (!address || !GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module)) {
        std::fprintf(stderr, "native exception address=%p module=<unresolved>\n", address);
        return;
    }
    wchar_t name[MAX_PATH]{};
    (void)GetModuleFileNameW(module, name, MAX_PATH);
    const auto rva = reinterpret_cast<std::uintptr_t>(address) -
                     reinterpret_cast<std::uintptr_t>(module);
    std::fprintf(stderr, "native exception address=%p module=%ls rva=0x%zx\n",
                 address, name, rva);
}

LONG WINAPI reportUnhandledNativeException(EXCEPTION_POINTERS* exception) noexcept {
    const auto* record = exception ? exception->ExceptionRecord : nullptr;
    const DWORD code = record ? record->ExceptionCode : 0;
    const void* address = record ? record->ExceptionAddress : nullptr;
    std::fprintf(stderr, "Unhandled native exception: code=0x%08lx\n",
                 static_cast<unsigned long>(code));
    printExceptionAddress(address);
    std::fflush(stderr);

    HANDLE file = CreateFileW(crashDumpPath, GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "MiniDumpWriteDump: CreateFileW failed, error=%lu\n",
                     static_cast<unsigned long>(GetLastError()));
        std::fflush(stderr);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    MINIDUMP_EXCEPTION_INFORMATION details{};
    details.ThreadId = GetCurrentThreadId();
    details.ExceptionPointers = exception;
    details.ClientPointers = FALSE;
    const BOOL written = MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
        exception ? &details : nullptr, nullptr, nullptr);
    const DWORD dumpError = written ? ERROR_SUCCESS : GetLastError();
    (void)CloseHandle(file);
    if (written) {
        std::fprintf(stderr, "MiniDumpWriteDump: wrote %ls\n", crashDumpPath);
    } else {
        std::fprintf(stderr, "MiniDumpWriteDump failed, error=%lu\n",
                     static_cast<unsigned long>(dumpError));
    }
    std::fflush(stderr);

    // Returning EXCEPTION_EXECUTE_HANDLER preserves process termination even
    // if opening or writing the optional dump failed.
    return EXCEPTION_EXECUTE_HANDLER;
}

void installOptionalCrashDump() noexcept {
    const DWORD length = GetEnvironmentVariableW(
        crashDumpEnvironment, crashDumpPath,
        static_cast<DWORD>(sizeof(crashDumpPath) / sizeof(crashDumpPath[0])));
    if (length == 0) return; // Opt-in only.
    if (length >= sizeof(crashDumpPath) / sizeof(crashDumpPath[0]) ||
        !isAbsoluteWindowsPath(crashDumpPath)) {
        crashDumpPath[0] = L'\0';
        std::fputs("FOLIA_TEST_CRASH_DUMP ignored: expected an absolute Windows path\n",
                   stderr);
        std::fflush(stderr);
        return;
    }
    (void)SetUnhandledExceptionFilter(reportUnhandledNativeException);
}

[[noreturn]] void reportTermination() noexcept {
    if (const auto error = std::current_exception()) {
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& caught) {
            std::fprintf(stderr, "Unhandled exception: %s: %s\n",
                         typeid(caught).name(), caught.what());
        } catch (...) {
            std::fputs("Unhandled non-standard exception\n", stderr);
        }
    } else {
        std::fputs("terminate() without an active exception\n", stderr);
    }
    void* frames[48]{};
    const auto count = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (unsigned short index = 0; index < count; ++index) {
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(frames[index]), &module)) {
            wchar_t name[MAX_PATH]{};
            (void)GetModuleFileNameW(module, name, MAX_PATH);
            const auto offset = reinterpret_cast<std::uintptr_t>(frames[index]) -
                                reinterpret_cast<std::uintptr_t>(module);
            std::fprintf(stderr, "frame[%hu] %p module=%ls rva=0x%zx\n",
                         index, frames[index], name, offset);
        }
    }
    std::fflush(stderr);
    std::abort();
}
struct ConsoleTestRuntime {
    ConsoleTestRuntime() noexcept {
        installOptionalCrashDump();
        SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS |
                     SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        (void)_set_error_mode(_OUT_TO_STDERR);
        (void)_set_abort_behavior(_WRITE_ABORT_MSG,
                                 _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        (void)std::set_terminate(reportTermination);
#ifdef _DEBUG
        constexpr int kinds[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
        for (const int kind : kinds) {
            (void)_CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
            (void)_CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
        }
#endif
    }
};
ConsoleTestRuntime consoleTestRuntime;
}
