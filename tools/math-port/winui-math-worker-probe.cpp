#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <objbase.h>
#include <winrt/base.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <exception>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>


#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/math/MathSvgStore.h"

using folia::platform::MathJaxErrorKind;
using folia::platform::MathRenderPurpose;
using folia::platform::MathJaxRenderer;
using folia::platform::MathJaxSvg;
using folia::platform::MathSvgStore;

namespace {
void require(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string(message));
}

void checkGeometry(folia::platform::MathJaxSvg const& value, std::string_view name) {
    require(static_cast<bool>(value), std::string(name) + ": parsed result is usable");
    require(value.width > 0 && value.height > 0, std::string(name) + ": aggregate dimensions");
    require(!value.fragments.empty(), std::string(name) + ": at least one SVG fragment");
    for (auto const &fragment : value.fragments) {
        require(fragment.renderId != 0, std::string(name) + ": fragment render id");
        require(fragment.width > 0 && fragment.height > 0,
                std::string(name) + ": fragment dimensions");
    }
}

void checkFragments(folia::platform::MathJaxSvg const &value, std::string_view name) {
    checkGeometry(value, name);
    for (auto const &fragment : value.fragments)
        require(fragment.svg && !fragment.svg->empty(), std::string(name) + ": fragment SVG bytes");
}

std::shared_ptr<folia::platform::MathJaxSvg const> Ready(folia::platform::MathJaxRenderer& renderer,
    std::string_view tex, bool display, float em, float containerWidth,
    std::string_view color, std::stop_token stop = {},
    folia::platform::MathRenderPurpose purpose = folia::platform::MathRenderPurpose::Document) {
    auto prepared = renderer.Prepare(tex, display, em, containerWidth, color, stop, purpose);
    auto rendered = renderer.Render(tex, display, em, containerWidth, color, stop, purpose);
    require(static_cast<bool>(rendered), "prepared math geometry is immediately available");
    require(prepared && rendered->fragments.size() == prepared->fragments.size(),
            "prepared math SVGs correspond to the resident geometry fragments");
    for (std::size_t index = 0; index < prepared->fragments.size(); ++index)
        require(prepared->fragments[index].renderId == rendered->fragments[index].renderId,
                "prepared SVG retains its geometry render id");
    return prepared;
}

void WaitForIdleBounded(folia::platform::MathJaxRenderer& renderer,
    std::chrono::seconds timeout = std::chrono::seconds(50))
{
    std::stop_source waitStop;
    std::mutex mutex;
    std::condition_variable_any changed;
    std::jthread watchdog([&](std::stop_token stop) {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, stop, timeout, [] { return false; }))
            waitStop.request_stop();
    });
    try
    {
        renderer.WaitForIdle(waitStop.get_token());
    }
    catch (winrt::hresult_canceled const&)
    {
        watchdog.request_stop();
        throw std::runtime_error("timed out waiting for native math worker to become idle");
    }
    watchdog.request_stop();
}

bool sameMetrics(folia::platform::MathJaxSvg const& left, folia::platform::MathJaxSvg const& right) {
    if (left.width != right.width || left.height != right.height
        || left.verticalAlign != right.verticalAlign || left.fragments.size() != right.fragments.size())
        return false;
    for (std::size_t i = 0; i < left.fragments.size(); ++i) {
        auto const& a = left.fragments[i];
        auto const& b = right.fragments[i];
        if (a.renderId != b.renderId || a.width != b.width || a.height != b.height
            || a.verticalAlign != b.verticalAlign || a.breakSpace != b.breakSpace
            || a.breakBefore != b.breakBefore)
            return false;
    }
    return true;
}

bool sameRenderIds(folia::platform::MathJaxSvg const& left, folia::platform::MathJaxSvg const& right) {
    if (left.fragments.size() != right.fragments.size())
        return false;
    for (std::size_t i = 0; i < left.fragments.size(); ++i) {
        if (left.fragments[i].renderId != right.fragments[i].renderId)
            return false;
    }
    return true;
}

bool FileExists(std::filesystem::path const& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::filesystem::path MakeGuidPath(std::filesystem::path const& directory,
                                   std::wstring_view extension = L".svgstore") {
    GUID id{};
    if (FAILED(CoCreateGuid(&id)))
        throw std::runtime_error("CoCreateGuid failed");
    wchar_t idText[40]{};
    if (!StringFromGUID2(id, idText, static_cast<int>(std::size(idText))))
        throw std::runtime_error("StringFromGUID2 failed");
    return directory / (std::wstring(idText) + std::wstring(extension));
}

void testChildStore(std::filesystem::path const& receiptPath) {
    folia::platform::MathSvgStore store;
    const auto range = store.Append("<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
    require(range.size != 0 && !store.Path().empty(), "child store created backing file");

    const auto receipt = CreateFileW(receiptPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (receipt == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateFileW(child receipt)");
    const auto pathText = store.Path().wstring();
    const auto bytes = static_cast<DWORD>(pathText.size() * sizeof(wchar_t));
    DWORD written = 0;
    const auto success = WriteFile(receipt, pathText.data(), bytes, &written, nullptr);
    const auto error = success ? ERROR_WRITE_FAULT : GetLastError();
    CloseHandle(receipt);
    if (!success || written != bytes)
        throw std::system_error(static_cast<int>(error), std::system_category(),
                                "WriteFile(child receipt)");

    for (;;) Sleep(1000);
}

class ChildStoreProcess {
public:
    ChildStoreProcess() = default;
    ChildStoreProcess(ChildStoreProcess const&) = delete;
    ChildStoreProcess& operator=(ChildStoreProcess const&) = delete;
    ChildStoreProcess(ChildStoreProcess&& other) noexcept
        : process(std::exchange(other.process, nullptr)), receipt(std::move(other.receipt)),
          storePath(std::move(other.storePath)) {}
    ChildStoreProcess& operator=(ChildStoreProcess&& other) noexcept {
        if (this != &other) {
            Close();
            process = std::exchange(other.process, nullptr);
            receipt = std::move(other.receipt);
            storePath = std::move(other.storePath);
        }
        return *this;
    }
    ~ChildStoreProcess() { Close(); }

    HANDLE process = nullptr;
    std::filesystem::path receipt;
    std::filesystem::path storePath;

    void Terminate() {
        if (!process)
            return;
        if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT && !TerminateProcess(process, 0))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "TerminateProcess(store child)");
        WaitForSingleObject(process, INFINITE);
        CloseHandle(process);
        process = nullptr;
    }

private:
    void Close() noexcept {
        if (process) {
            if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
                TerminateProcess(process, 1);
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
            process = nullptr;
        }
        if (!receipt.empty())
            DeleteFileW(receipt.c_str());
    }
};

ChildStoreProcess StartChildStore();

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : handle(value) {}
    ~ScopedHandle() { reset(); }
    ScopedHandle(ScopedHandle const&) = delete;
    ScopedHandle& operator=(ScopedHandle const&) = delete;
    HANDLE get() const noexcept { return handle; }
    void reset(HANDLE value = INVALID_HANDLE_VALUE) noexcept {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
            CloseHandle(handle);
        handle = value;
    }
private:
    HANDLE handle;
};

class TemporaryPathOverride {
public:
    explicit TemporaryPathOverride(std::filesystem::path const& root) {
        std::wstring buffer(32768, L'\0');
        SetLastError(ERROR_SUCCESS);
        const auto length = GetEnvironmentVariableW(L"TMP", buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length >= buffer.size())
            throw std::system_error(ERROR_INSUFFICIENT_BUFFER, std::system_category(),
                                    "Read TMP environment variable");
        if (length != 0)
            previous = std::wstring(buffer.data(), length);
        else if (GetLastError() != ERROR_ENVVAR_NOT_FOUND)
            previous = std::wstring{};

        if (!SetEnvironmentVariableW(L"TMP", root.c_str()))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "Set isolated TMP environment variable");
        active = true;
    }
    ~TemporaryPathOverride() {
        if (active)
            SetEnvironmentVariableW(L"TMP", previous ? previous->c_str() : nullptr);
    }
    TemporaryPathOverride(TemporaryPathOverride const&) = delete;
    TemporaryPathOverride& operator=(TemporaryPathOverride const&) = delete;
private:
    std::optional<std::wstring> previous;
    bool active = false;
};

void CreateSeedFile(std::filesystem::path const& path, std::string_view contents = {}) {
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateFileW(cache fixture)");
    DWORD written = 0;
    const auto succeeded = contents.empty()
        || (WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr)
            && written == contents.size());
    const auto error = succeeded ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!succeeded)
        throw std::system_error(static_cast<int>(error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : error),
                                std::system_category(), "WriteFile(cache fixture)");
}

void RequireNoReclaimFailures(folia::platform::MathSvgStore::CleanupResult const& result,
                              std::string_view phase) {
    require(result.failed == 0, std::string(phase) + ": orphan cleanup reported failures");
}

void runCacheProbe(std::filesystem::path const& isolatedRoot) {
    require(isolatedRoot.is_absolute(), "cache fixture root must be absolute");
    require(FileExists(isolatedRoot) &&
                (GetFileAttributesW(isolatedRoot.c_str()) & FILE_ATTRIBUTE_DIRECTORY),
            "cache fixture root directory exists");
    TemporaryPathOverride tempOverride(isolatedRoot);
    const auto cacheDirectory = isolatedRoot / L"FoliaMath";
    if (!CreateDirectoryW(cacheDirectory.c_str(), nullptr)) {
        const auto error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
            throw std::system_error(static_cast<int>(error), std::system_category(),
                                    "CreateDirectoryW(cache fixture)");
    }

    const auto missingRoot = isolatedRoot / L"missing-cache-root";
    if (!CreateDirectoryW(missingRoot.c_str(), nullptr))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateDirectoryW(missing cache root)");
    {
        TemporaryPathOverride missingTemp(missingRoot);
        const auto missingResult = folia::platform::MathSvgStore::ReclaimOrphans();
        require(missingResult.removed == 0 && missingResult.inUse == 0
                    && missingResult.skipped == 0 && missingResult.failed == 0,
                "missing cache directory is a quiet cleanup no-op");
    }

    const auto emptyOrphan = MakeGuidPath(cacheDirectory);
    const auto partialOrphan = MakeGuidPath(cacheDirectory);
    const auto busyOrphan = MakeGuidPath(cacheDirectory);
    const auto readOnlyOrphan = MakeGuidPath(cacheDirectory);
    CreateSeedFile(emptyOrphan);
    CreateSeedFile(partialOrphan, "partial SVG write");
    CreateSeedFile(readOnlyOrphan, "read-only orphan");
    if (!SetFileAttributesW(readOnlyOrphan.c_str(), FILE_ATTRIBUTE_READONLY))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SetFileAttributesW(read-only cache fixture)");
    const auto busyHandleValue = CreateFileW(busyOrphan.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (busyHandleValue == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateFileW(busy cache fixture)");
    ScopedHandle busyHandle(busyHandleValue);

    const auto unrelated = cacheDirectory / L"unrelated.svgstore";
    const auto malformed = cacheDirectory / L"not-a-guid.svgstore";
    CreateSeedFile(unrelated, "unrelated");
    CreateSeedFile(malformed, "malformed name");
    const auto directoryEntry = MakeGuidPath(cacheDirectory);
    if (!CreateDirectoryW(directoryEntry.c_str(), nullptr))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateDirectoryW(GUID cache fixture)");
    const auto sentinel = directoryEntry / L"sentinel.txt";
    CreateSeedFile(sentinel, "directory sentinel");
    const auto reparseEntry = MakeGuidPath(cacheDirectory);
    const auto reparseCreated = CreateSymbolicLinkW(reparseEntry.c_str(), unrelated.c_str(),
        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != FALSE;
    if (!reparseCreated)
        std::cout << "cache reparse fixture=skipped win32=" << GetLastError() << '\n';

    const auto junctionMarker = isolatedRoot / L"junctions.ready";
    const auto junctionTarget = isolatedRoot / L"junction-target";
    const auto directJunction = cacheDirectory / L"{74a13d90-4e9d-4f16-8207-97e949a6f001}.svgstore";
    const auto reparseRoot = isolatedRoot / L"reparse-root";
    const auto rootJunction = reparseRoot / L"FoliaMath";
    const auto junctionTargetStore = junctionTarget / L"{74a13d90-4e9d-4f16-8207-97e949a6f002}.svgstore";
    const auto junctionTargetSentinel = junctionTarget / L"owned-target.txt";
    const auto junctionsPrepared = FileExists(junctionMarker);
    if (junctionsPrepared) {
        require(FileExists(directJunction) && FileExists(rootJunction),
                "PowerShell created both isolated cache junctions");
        const auto directAttributes = GetFileAttributesW(directJunction.c_str());
        const auto rootAttributes = GetFileAttributesW(rootJunction.c_str());
        require(directAttributes != INVALID_FILE_ATTRIBUTES
                    && (directAttributes & FILE_ATTRIBUTE_REPARSE_POINT),
                "matching cache entry fixture is a reparse point");
        require(rootAttributes != INVALID_FILE_ATTRIBUTES
                    && (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT),
                "isolated FoliaMath root fixture is a reparse point");
        require(FileExists(junctionTargetStore) && FileExists(junctionTargetSentinel),
                "junction target contains valid cache data and a sentinel");
    } else {
        std::cout << "cache junction fixtures=skipped powershell-creation-unavailable\n";
    }

    auto first = folia::platform::MathSvgStore::ReclaimOrphans();
    require(first.failed == 1, "read-only orphan is reported as a cleanup failure");
    require(first.removed == 2, "first cleanup removes the empty and partial orphan files");
    require(first.inUse == 1, "first cleanup preserves the open cache handle");
    require(!FileExists(emptyOrphan) && !FileExists(partialOrphan),
            "closed orphan files are deleted");
    require(FileExists(busyOrphan) && FileExists(readOnlyOrphan),
            "open and read-only cache files remain present");
    require(FileExists(unrelated) && FileExists(malformed),
            "unrelated and malformed names remain untouched");
    require(FileExists(directoryEntry) && FileExists(sentinel),
            "GUID-named directory and its contents remain untouched");
    if (junctionsPrepared)
        require(FileExists(junctionTargetStore) && FileExists(junctionTargetSentinel),
                "cache-entry junction target is not traversed or modified");
    if (reparseCreated)
        require(FileExists(reparseEntry), "GUID-named reparse point remains untouched");
    std::cout << "cache cleanup first removed=" << first.removed
              << " inUse=" << first.inUse << " skipped=" << first.skipped
              << " failed=" << first.failed << '\n';

    if (!SetFileAttributesW(readOnlyOrphan.c_str(), FILE_ATTRIBUTE_NORMAL))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "Restore read-only cache fixture attributes");
    busyHandle.reset();
    auto released = folia::platform::MathSvgStore::ReclaimOrphans();
    RequireNoReclaimFailures(released, "released cache cleanup");
    require(released.removed == 2 && released.inUse == 0,
            "released and restored cache files become reclaimable");
    require(!FileExists(busyOrphan) && !FileExists(readOnlyOrphan),
            "released cache files are deleted");
    std::cout << "cache readonly=reported-restored-reclaimed\n";

    auto child = StartChildStore();
    require(FileExists(child.storePath), "independent child holds a live SVG store file");
    auto whileChildActive = folia::platform::MathSvgStore::ReclaimOrphans();
    RequireNoReclaimFailures(whileChildActive, "cleanup with active child");
    require(whileChildActive.inUse >= 1, "active child cache is reported in use");
    require(FileExists(child.storePath), "cleanup preserves cache owned by another process");
    require(WaitForSingleObject(child.process, 0) == WAIT_TIMEOUT,
            "cleanup leaves the independent child process running");
    std::cout << "cache cleanup active-child removed=" << whileChildActive.removed
              << " inUse=" << whileChildActive.inUse << " skipped=" << whileChildActive.skipped
              << " failed=" << whileChildActive.failed << '\n';

    std::vector<std::filesystem::path> concurrentOrphans;
    concurrentOrphans.reserve(16);
    for (std::size_t i = 0; i < 16; ++i) {
        auto orphan = MakeGuidPath(cacheDirectory);
        CreateSeedFile(orphan, "concurrent startup orphan");
        concurrentOrphans.push_back(std::move(orphan));
    }
    std::array<folia::platform::MathSvgStore::CleanupResult, 2> concurrentResults{};
    std::array<std::thread, 2> reclaimers{
        std::thread([&] { concurrentResults[0] = folia::platform::MathSvgStore::ReclaimOrphans(); }),
        std::thread([&] { concurrentResults[1] = folia::platform::MathSvgStore::ReclaimOrphans(); }),
    };
    for (auto& reclaimer : reclaimers)
        reclaimer.join();
    for (auto const& orphan : concurrentOrphans)
        require(!FileExists(orphan), "concurrent cleanup removes every closed orphan file");
    require(FileExists(child.storePath)
                && WaitForSingleObject(child.process, 0) == WAIT_TIMEOUT,
            "concurrent cleanup preserves the independently owned child cache");
    std::cout << "cache concurrent scans=2 orphans=16 removed="
              << concurrentResults[0].removed + concurrentResults[1].removed
              << " inUse=" << concurrentResults[0].inUse + concurrentResults[1].inUse
              << " result=all-orphans-gone-live-cache-kept\n";

    child.Terminate();
    require(!FileExists(child.storePath), "child termination deletes its owned store");
    auto afterChildExit = folia::platform::MathSvgStore::ReclaimOrphans();
    RequireNoReclaimFailures(afterChildExit, "cleanup after child exit");
    require(afterChildExit.inUse == 0, "no active cache remains after child exit");
    require(FileExists(unrelated) && FileExists(malformed)
                && FileExists(directoryEntry) && FileExists(sentinel),
            "non-cache fixtures survive all cleanup passes");
    if (junctionsPrepared) {
        require(FileExists(junctionTargetStore) && FileExists(junctionTargetSentinel),
                "target data survives all cleanup passes through the cache-entry junction");
        require(FileExists(directJunction) && FileExists(rootJunction),
                "both cache reparse points remain after orphan scans");
        {
            TemporaryPathOverride reparseTemp(reparseRoot);
            const auto reparseRootResult = folia::platform::MathSvgStore::ReclaimOrphans();
            require(reparseRootResult.removed == 0 && reparseRootResult.inUse == 0
                        && reparseRootResult.failed == 0 && reparseRootResult.skipped >= 1,
                    "reparse-point FoliaMath root is skipped without traversing target");
        }
        require(FileExists(junctionTargetStore) && FileExists(junctionTargetSentinel),
                "reparse cache-root cleanup leaves target cache data intact");
        std::cout << "cache reparse-root=skipped target=untouched\n";
    }
    if (reparseCreated) {
        require(FileExists(reparseEntry), "GUID-named reparse point remains after all cleanup passes");
        DeleteFileW(reparseEntry.c_str());
    }
    std::cout << "cache cleanup after-child removed=" << afterChildExit.removed
              << " inUse=" << afterChildExit.inUse << " skipped=" << afterChildExit.skipped
              << " failed=" << afterChildExit.failed << '\n';
    std::cout << "cache-only fixture result=PASS\n";
}

ChildStoreProcess StartChildStore() {
    ChildStoreProcess child;
    child.receipt = MakeGuidPath(std::filesystem::temp_directory_path(), L".receipt");
    wchar_t executable[32768]{};
    const auto executableLength = GetModuleFileNameW(nullptr, executable,
        static_cast<DWORD>(std::size(executable)));
    if (executableLength == 0 || executableLength >= std::size(executable))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "GetModuleFileNameW");
    std::wstring command = L"\"" + std::wstring(executable, executableLength)
        + L"\" --store-child \"" + child.receipt.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "CreateProcessW(store child)");
    child.process = process.hProcess;
    CloseHandle(process.hThread);

    const auto readyDeadline = GetTickCount64() + 15000;
    while (GetTickCount64() < readyDeadline) {
        if (FileExists(child.receipt)) {
            const auto receipt = CreateFileW(child.receipt.c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (receipt != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER length{};
                if (GetFileSizeEx(receipt, &length) && length.QuadPart > 0
                    && length.QuadPart <= 32768 && length.QuadPart % sizeof(wchar_t) == 0) {
                    std::wstring pathText(static_cast<std::size_t>(length.QuadPart) / sizeof(wchar_t), L'\0');
                    DWORD read = 0;
                    if (ReadFile(receipt, pathText.data(), static_cast<DWORD>(length.QuadPart), &read, nullptr)
                        && read == length.QuadPart)
                        child.storePath = std::filesystem::path(pathText);
                }
                CloseHandle(receipt);
            }
            if (!child.storePath.empty())
                return child;
        }
        if (WaitForSingleObject(child.process, 0) == WAIT_OBJECT_0)
            break;
        Sleep(10);
    }
    throw std::runtime_error("child store did not publish its path receipt");
}

void testStore() {
    constexpr std::size_t recordCount = 4097;
    constexpr std::size_t recordSize = 4096;
    const std::string record(recordSize, 'S');
    std::vector<folia::platform::MathSvgStore::Range> ranges;
    ranges.reserve(recordCount);
    std::filesystem::path storePath;
    {
        folia::platform::MathSvgStore store;
        require(store.Path().empty(), "empty store does not create a file");
        for (std::size_t i = 0; i < recordCount; ++i)
            ranges.push_back(store.Append(record));
        storePath = store.Path();
        require(!storePath.empty() && FileExists(storePath),
                "store file exists while store owns it");
        require(static_cast<std::uint64_t>(recordCount) * recordSize > 16ull * 1024 * 1024,
                "store coverage exceeds 16 MiB");
        require(ranges.size() > 4096, "store coverage exceeds 4096 entries");
        for (auto index : std::array<std::size_t, 5>{0, 1, 2048, 4095, 4096}) {
            auto read = store.Read(ranges[index]);
            require(read && *read == record, "store range round-trips exact bytes");
        }
        bool rejected = false;
        try {
            (void)store.Read({ static_cast<std::uint64_t>(recordCount) * recordSize, 1 });
        } catch (std::out_of_range const&) {
            rejected = true;
        }
        require(rejected, "store rejects ranges beyond committed bytes");
    }
    require(!FileExists(storePath), "store file is deleted after destruction");
    std::cout << "store records=" << recordCount << " bytes="
              << recordCount * recordSize << " roundtrip=exact destructor-delete=success\n";

    auto child = StartChildStore();
    require(FileExists(child.storePath), "child store file exists before termination");
    child.Terminate();
    require(!FileExists(child.storePath),
            "child process termination closes and deletes its store file");
    std::cout << "store child-process-delete=success\n";
}

void testOwnership() {
    folia::platform::MathJaxRenderer renderer;
    constexpr std::string_view sharedTex = R"(\frac{7}{9})";
    constexpr std::string_view replacementTex = R"(\sqrt{13})";
    auto sharedId = std::uint64_t{};
    auto replacementId = std::uint64_t{};

    renderer.BeginBlock(101);
    auto first = Ready(renderer, sharedTex, false, 16.0f, 900.0f, "#000000");
    renderer.EndBlock();
    checkFragments(*first, "owner 101 shared formula");
    sharedId = first->fragments.front().renderId;

    renderer.BeginBlock(202);
    auto secondOwner = Ready(renderer, sharedTex, false, 16.0f, 900.0f, "#000000");
    renderer.EndBlock();
    require(sameRenderIds(*first, *secondOwner), "same cache key is shared by both block owners");

    renderer.BeginBlock(101);
    auto replacement = Ready(renderer, replacementTex, false, 16.0f, 900.0f, "#000000");
    renderer.EndBlock();
    checkFragments(*replacement, "owner replacement formula");
    replacementId = replacement->fragments.front().renderId;
    std::string currentTex(replacementTex);
    auto currentId = replacementId;

    const std::array bothOwners{std::uint64_t{101}, std::uint64_t{202}};
    renderer.RetainBlocks(bothOwners);
    auto stillShared = Ready(renderer, sharedTex, false, 16.0f, 900.0f, "#000000");
    require(stillShared->fragments.front().renderId == sharedId,
            "changing block 101's key retains the formula still owned by block 202");
    auto stillReplacement = Ready(renderer, replacementTex, false, 16.0f, 900.0f,
                                             "#000000");
    require(stillReplacement->fragments.front().renderId == replacementId,
            "changed block 101 retains its replacement result");

    for (std::uint64_t i = 0; i < 12; ++i) {
        const auto tex = i % 2 == 0 ? R"(\sqrt{17})" : R"(\sqrt{19})";
        renderer.BeginBlock(101);
        auto edited = Ready(renderer, tex, false, 16.0f, 900.0f, "#000000");
        checkFragments(*edited, "repeated owner edit");
        renderer.EndBlock();
        currentTex = tex;
        currentId = edited->fragments.front().renderId;
        renderer.RetainBlocks(bothOwners);
        require(Ready(renderer, sharedTex, false, 16.0f, 900.0f, "#000000")
                    ->fragments.front().renderId == sharedId,
                "repeated edits to block 101 keep block 202's shared result");
    }

    const std::array onlyFirstOwner{std::uint64_t{101}};
    renderer.RetainBlocks(onlyFirstOwner);
    auto releasedShared = Ready(renderer, sharedTex, false, 16.0f, 900.0f, "#000000");
    require(releasedShared->fragments.front().renderId != sharedId,
            "removing the last owner releases the old key");
    auto releasedReplacement = Ready(renderer, replacementTex, false, 16.0f, 900.0f,
                                                "#000000");
    require(releasedReplacement->fragments.front().renderId != replacementId,
            "repeated edits release block 101's superseded key");
    auto current = Ready(renderer, currentTex, false, 16.0f, 900.0f, "#000000");
    require(current->fragments.front().renderId == currentId,
            "removing block 202 preserves block 101's current live result");
    std::cout << "block ownership=shared replaced retained repeat-edits released\n";
}

void testAsyncEditsAndClear() {
    folia::platform::MathJaxRenderer renderer;
    auto completions = std::make_shared<std::atomic_size_t>(0);
    renderer.SetCompletionCallback([completions] { completions->fetch_add(1, std::memory_order_relaxed); });

    constexpr std::string_view initialTex = R"(\frac{1}{x+1})";
    renderer.BeginBlock(403);
    auto initialPending = renderer.Render(initialTex, false, 16.0f, 640.0f, "#000000", {},
                                          folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    require(!initialPending, "cold edit preview returns immediately while native work is pending");
    auto initial = renderer.Prepare(initialTex, false, 16.0f, 640.0f, "#000000", {},
                                    folia::platform::MathRenderPurpose::Preview);
    checkFragments(*initial, "memory-only initial preview");
    WaitForIdleBounded(renderer);
    auto initialStats = renderer.Snapshot();
    require(initialStats.fileReads == 0 && initialStats.fileWrites == 0
                && initialStats.backingPath.empty(),
            "preview-only rendering performs no file I/O and creates no SVG store");

    const std::string oldPreview = "\\int_0^{1000} \\frac{x^2+1}{x+1} dx";
    constexpr std::string_view latestPreview = R"(\sqrt{101})";
    renderer.BeginBlock(404);
    auto oldPending = renderer.Render(oldPreview, false, 16.0f, 640.0f, "#000000", {},
                                     folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    require(!oldPending, "cold live-edit preview returns immediately while native work is pending");

    renderer.BeginBlock(404);
    auto latestPending = renderer.Render(latestPreview, false, 16.0f, 640.0f, "#000000", {},
                                        folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    auto latest = Ready(renderer, latestPreview, false, 16.0f, 640.0f, "#000000", {},
                        folia::platform::MathRenderPurpose::Preview);
    checkFragments(*latest, "latest edited preview");
    WaitForIdleBounded(renderer);
    auto afterEdit = renderer.Snapshot();
    require(afterEdit.pending == 0 && afterEdit.entries == 2,
            "replacing a block preview releases its stale key and keeps the newest result");
    require(sameRenderIds(*latest,
                *renderer.Render(latestPreview, false, 16.0f, 640.0f, "#000000", {},
                                 folia::platform::MathRenderPurpose::Preview)),
            "latest edit preview remains cached with stable SVG ids");
    std::cout << "async-edit old-request=pending latest-request="
              << (latestPending ? "already-ready" : "pending")
              << " entries-after-replacement=" << afterEdit.entries
              << " file-reads=" << afterEdit.fileReads << " file-writes=" << afterEdit.fileWrites
              << " backing-file=" << (afterEdit.backingPath.empty() ? "none" : "unexpected") << '\n';

    constexpr std::string_view malformed = R"(\frac{x}{)";
    renderer.BeginBlock(404);
    (void)renderer.Render(malformed, false, 16.0f, 640.0f, "#000000", {},
                          folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    auto malformedResult = Ready(renderer, malformed, false, 16.0f, 640.0f, "#000000", {},
                                 folia::platform::MathRenderPurpose::Preview);
    require(malformedResult->errorKind == folia::platform::MathJaxErrorKind::Formula && !malformedResult->fragments.empty(),
            "malformed edit preview returns a formula diagnostic during editing");
    constexpr std::string_view repaired = R"(\frac{x}{2})";
    renderer.BeginBlock(404);
    (void)renderer.Render(repaired, false, 16.0f, 640.0f, "#000000", {},
                          folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    auto repairedResult = Ready(renderer, repaired, false, 16.0f, 640.0f, "#000000", {},
                                folia::platform::MathRenderPurpose::Preview);
    checkFragments(*repairedResult, "repaired edit preview");
    require(repairedResult->errorKind == folia::platform::MathJaxErrorKind::None,
            "valid source immediately replaces the malformed preview");

    renderer.BeginBlock(405);
    (void)renderer.Render(R"(\sum_{i=1}^{500} i^2)", false, 16.0f, 640.0f,
                          "#000000", {}, folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    renderer.Clear();
    constexpr std::string_view afterClearTex = R"(\mathbb{N})";
    renderer.BeginBlock(406);
    (void)renderer.Render(afterClearTex, false, 16.0f, 640.0f, "#000000", {},
                          folia::platform::MathRenderPurpose::Preview);
    renderer.EndBlock();
    auto afterClear = Ready(renderer, afterClearTex, false, 16.0f, 640.0f, "#000000", {},
                            folia::platform::MathRenderPurpose::Preview);
    WaitForIdleBounded(renderer);
    auto afterClearStats = renderer.Snapshot();
    require(afterClear && afterClearStats.pending == 0 && afterClearStats.entries == 1
                && afterClearStats.previewSvgBytes > 0
                && afterClearStats.fileReads == 0 && afterClearStats.fileWrites == 0
                && afterClearStats.backingPath.empty(),
            "completion of pre-Clear work cannot mutate the new cache state");
    require(completions->load(std::memory_order_relaxed) != 0,
            "asynchronous preview completion callbacks are delivered");
    std::cout << "async-clear stale-completion=isolated callbacks="
              << completions->load(std::memory_order_relaxed)
              << " final-entries=" << afterClearStats.entries
              << " preview-bytes=" << afterClearStats.previewSvgBytes
              << " file-reads=" << afterClearStats.fileReads
              << " file-writes=" << afterClearStats.fileWrites
              << " backing-file=none\n";
}

void testDocumentSvgEviction() {
    using namespace std::chrono;
    constexpr std::size_t targetBytes = 8u * 1024u * 1024u;
    constexpr std::size_t formulaLimit = 256;
    std::string body = "x";
    for (std::size_t index = 0; index < 767; ++index) body += "+x";

    folia::platform::MathJaxRenderer renderer;
    std::vector<std::uint64_t> owners;
    owners.reserve(formulaLimit);
    std::string firstFormula;
    std::vector<std::string> firstSvgs;
    std::vector<std::uint64_t> firstRenderIds;
    std::vector<std::uint64_t> allRenderIds;
    std::size_t cumulativeSvgBytes = 0;
    auto stageStart = steady_clock::now();
    for (std::size_t index = 1; index <= formulaLimit && cumulativeSvgBytes <= targetBytes; ++index) {
        auto formula = std::string(R"(\underbrace{)") + body + "}_{" + std::to_string(index) + "}";
        const auto owner = static_cast<std::uint64_t>(index);
        owners.push_back(owner);
        renderer.BeginBlock(owner);
        auto prepared = renderer.Prepare(formula, false, 16.0f, 640.0f, "#000000", {},
                                         folia::platform::MathRenderPurpose::Document);
        renderer.EndBlock();
        checkFragments(*prepared, "document formula before SVG eviction");
        auto geometry = renderer.Render(formula, false, 16.0f, 640.0f, "#000000", {},
                                        folia::platform::MathRenderPurpose::Document);
        checkGeometry(*geometry, "resident document geometry");
        require(geometry->fragments.size() == prepared->fragments.size(),
                "document geometry retains all prepared SVG fragments");
        for (std::size_t fragment = 0; fragment < prepared->fragments.size(); ++fragment) {
            require(prepared->fragments[fragment].renderId == geometry->fragments[fragment].renderId,
                    "document geometry IDs match prepared SVG data");
            cumulativeSvgBytes += prepared->fragments[fragment].svg->size();
            allRenderIds.push_back(prepared->fragments[fragment].renderId);
        }
        if (index == 1) {
            firstFormula = std::move(formula);
            for (auto const& fragment : prepared->fragments) {
                firstSvgs.push_back(*fragment.svg);
                firstRenderIds.push_back(fragment.renderId);
            }
        }
    }
    WaitForIdleBounded(renderer);
    renderer.RetainBlocks(owners);
    auto pressureStats = renderer.Snapshot();
    require(cumulativeSvgBytes > targetBytes,
            "document fixture produced more than eight MiB of actual prepared SVG bytes");
    require(pressureStats.fileWrites > 0 && !pressureStats.backingPath.empty()
                && FileExists(pressureStats.backingPath),
            "document SVG pressure is persisted in the owned temporary SVG store");
    require(pressureStats.residentSvgBytes == 0,
            "offscreen document SVG bytes are released after the explicit consumer returns");
    auto privateBytes = [] {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        require(GetProcessMemoryInfo(GetCurrentProcess(),
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) != FALSE,
                "read process private memory");
        return counters.PrivateUsage;
    };
    require(renderer.Snapshot().nativeSessionActive,
            "the lightweight engine remains on its worker after the request releases font bodies");
    std::cout << "opening-engine-private=" << privateBytes() << " native-session=retained\n";

    auto geometry = renderer.Render(firstFormula, false, 16.0f, 640.0f, "#000000", {},
                                    folia::platform::MathRenderPurpose::Document);
    checkGeometry(*geometry, "evicted formula geometry");
    require(geometry->fragments.size() == firstRenderIds.size()
                && geometry->fragments.front().renderId == firstRenderIds.front()
                && !geometry->fragments.front().svg,
            "SVG eviction retains the formula geometry and stable render id");
    require(renderer.Snapshot().pending == 0 && renderer.Snapshot().fileReads == pressureStats.fileReads,
            "layout cache hits do not queue offscreen SVG reads");
    renderer.SetVisibleSvgIds(firstRenderIds);
    const auto readStart = steady_clock::now();
    std::vector<std::shared_ptr<std::string const>> svgs(geometry->fragments.size());
    bool queuedRead = false;
    for (std::size_t index = 0; index < geometry->fragments.size(); ++index) {
        require(geometry->fragments[index].renderId == firstRenderIds[index],
                "SVG eviction preserves every fragment render id");
        svgs[index] = renderer.GetSvg(firstRenderIds[index]);
        queuedRead = queuedRead || !svgs[index];
    }
    const auto readCall = duration_cast<microseconds>(steady_clock::now() - readStart).count();
    WaitForIdleBounded(renderer);
    for (std::size_t index = 0; index < firstRenderIds.size(); ++index) {
        if (!svgs[index]) svgs[index] = renderer.GetSvg(firstRenderIds[index]);
        require(svgs[index] && *svgs[index] == firstSvgs[index],
                "worker SVG reload preserves exact complete formula bytes");
    }
    auto afterReadStats = renderer.Snapshot();
    require(afterReadStats.nativeBatches == pressureStats.nativeBatches,
            "reading cached SVG performs no native rendering batch");
    require(afterReadStats.lastIoThread != std::this_thread::get_id()
                && afterReadStats.lastIoThread != std::thread::id{},
            "SVG file operations run on the cache worker, not the calling scroll thread");
    require(afterReadStats.fileReads > pressureStats.fileReads,
            "asynchronous SVG reload is recorded as worker file I/O");
    std::cout << "document-svg-pressure formulas=" << owners.size()
              << " prepared-bytes=" << cumulativeSvgBytes
              << " resident-bytes=" << pressureStats.residentSvgBytes
              << " file-writes=" << pressureStats.fileWrites
              << " reload-reads=" << afterReadStats.fileReads - pressureStats.fileReads
              << " GetSvg-initial=" << (queuedRead ? "queued" : "prefetched")
              << " GetSvg-call-us=" << readCall
              << " prep-elapsed-ms=" << duration_cast<milliseconds>(steady_clock::now() - stageStart).count()
              << " exact-reload=success\n";
    svgs.clear();
    renderer.SetVisibleSvgIds({});
    require(renderer.Snapshot().residentSvgBytes == 0,
            "leaving the viewport releases the last document SVG bytes");

    // Hold the real worker at its normal completion callback so viewport
    // changes deterministically happen before it can consume queued reads.
    struct Gate {
        std::mutex mutex;
        std::condition_variable changed;
        bool entered = false;
        bool released = false;
        void Release() {
            std::scoped_lock lock(mutex);
            released = true;
            changed.notify_all();
        }
    };
    auto gate = std::make_shared<Gate>();
    auto releaseGate = [gate](void*) { gate->Release(); };
    std::unique_ptr<void, decltype(releaseGate)> gateGuard(gate.get(), releaseGate);
    renderer.SetCompletionCallback([gate] {
        std::unique_lock lock(gate->mutex);
        gate->entered = true;
        gate->changed.notify_all();
        gate->changed.wait_for(lock, seconds(10), [&] { return gate->released; });
    });
    (void)renderer.Render("x+917", false, 16.0f, 640.0f, "#000000", {}, folia::platform::MathRenderPurpose::Preview);
    {
        std::unique_lock lock(gate->mutex);
        require(gate->changed.wait_for(lock, seconds(10), [&] { return gate->entered; }),
                "worker reaches the bounded viewport test gate");
    }
    const auto beforeScroll = renderer.Snapshot();
    // A new width/edit can also produce native jobs while scrolling. Cancel
    // obsolete owned jobs without discarding completed document geometry.
    renderer.BeginBlock(11001);
    (void)renderer.Render("x+11001", false, 16.0f, 640.0f, "#000000");
    renderer.EndBlock();
    renderer.BeginBlock(11002);
    (void)renderer.Render("x+11002", false, 16.0f, 640.0f, "#000000");
    renderer.EndBlock();
    const std::array<std::uint64_t, 1> visibleOwners{11002};
    renderer.CancelOffscreenRenders(visibleOwners);
    renderer.SetVisibleSvgIds(allRenderIds);
    require(renderer.Snapshot().queuedReads == allRenderIds.size(),
            "only current viewport requests are queued");
    const auto newestId = allRenderIds.back();
    for (int pass = 0; pass < 100; ++pass) {
        const auto id = allRenderIds[static_cast<std::size_t>(pass) % allRenderIds.size()];
        renderer.SetVisibleSvgIds(std::span{&id, std::size_t{1}});
        require(renderer.Snapshot().queuedReads == 1,
                "rapid viewport replacement never accumulates obsolete reads");
    }
    renderer.SetVisibleSvgIds(std::span{&newestId, std::size_t{1}});
    const auto queuedScroll = renderer.Snapshot();
    require(queuedScroll.canceledReads > beforeScroll.canceledReads,
            "old viewport reads are removed before worker I/O");
    renderer.SetCompletionCallback({});
    gate->Release();
    WaitForIdleBounded(renderer);
    const auto afterScroll = renderer.Snapshot();
    require(afterScroll.nativeRenders == beforeScroll.nativeRenders + 1,
            "obsolete viewport native jobs are canceled before rendering");
    require(afterScroll.fileReads == beforeScroll.fileReads + 1
                && renderer.GetSvg(newestId) && !renderer.GetSvg(firstRenderIds.front()),
            "only the latest viewport is read and retained after rapid scrolling");
    renderer.SetVisibleSvgIds({});
    require(renderer.Snapshot().residentSvgBytes == 0 && renderer.Snapshot().queuedReads == 0,
            "empty viewport has no resident document SVG or queued reads");
    std::cout << "viewport-replacements=100 queued-latest=1 canceled-reads="
              << queuedScroll.canceledReads - beforeScroll.canceledReads
              << " actual-reads=" << afterScroll.fileReads - beforeScroll.fileReads
              << " actual-native-renders=" << afterScroll.nativeRenders - beforeScroll.nativeRenders
              << " offscreen-resident-bytes=0\n";
    auto previousPath = afterReadStats.backingPath;
    renderer.Clear();
    auto closeDeadline = GetTickCount64() + 5000;
    while (FileExists(previousPath) && GetTickCount64() < closeDeadline) Sleep(5);
    require(!FileExists(previousPath), "Clear closes the previous document cache on its worker");
    require(renderer.Snapshot().backingPath.empty(), "fresh state creates no file before its first formula");
    std::cout << "document-cache-clear worker-close=success fresh-state=no-file\n";
}

void testAbandonedBatch() {
    folia::platform::MathJaxRenderer renderer;
    {
        auto batch = renderer.BeginBatch();
        renderer.BeginBlock(12001);
        batch.Render(R"(\mathbb{R})", false, 16.0f, 640.0f, "#000000");
        renderer.EndBlock();
        auto staged = renderer.Snapshot();
        require(staged.pending == 1 && staged.nativeRenders == 0 && staged.fileWrites == 0,
                "staged math has no native work or document file before submission");
    }
    WaitForIdleBounded(renderer);
    auto abandoned = renderer.Snapshot();
    require(abandoned.pending == 0 && abandoned.entries == 0 && abandoned.fileWrites == 0
                && abandoned.backingPath.empty(),
            "abandoning a batch removes its requests without leaving pending work or files");
    auto preview = Ready(renderer, R"(\mathbb{R})", false, 16.0f, 640.0f, "#000000", {},
                         folia::platform::MathRenderPurpose::Preview);
    checkFragments(*preview, "preview after abandoned opening batch");
    require(renderer.Snapshot().nativeRenders == 1 && renderer.Snapshot().fileWrites == 0,
            "the same cache remains usable for an asynchronous memory-only preview");
    std::cout << "abandoned-batch requests=released pending=0 file-writes=0 preview=recovered\n";
}

void runCancellationProbe() {
    folia::platform::MathJaxRenderer renderer;
    std::stop_source cancelled;
    cancelled.request_stop();
    bool caughtCancellation = false;
    try {
        (void)renderer.Render(R"(\sqrt{2})", false, 16.0f, 1280.0f,
                              "#000000", cancelled.get_token());
    } catch (winrt::hresult_canceled const&) {
        caughtCancellation = true;
    }
    require(caughtCancellation, "pre-requested cancellation throws winrt::hresult_canceled");
    std::cout << "pre-requested cancellation=hresult_canceled\n";
}

void testCrossThreadRendererMove() {
    constexpr std::string_view cachedTex = R"(\frac{23}{29})";
    std::optional<folia::platform::MathJaxRenderer> transferred;
    std::exception_ptr workerFailure;
    std::uint64_t expectedRenderId = 0;
    std::string expectedSvg;

    std::thread worker([&] {
        try {
            folia::platform::MathJaxRenderer producer;
            producer.BeginBlock(303);
            auto first = Ready(producer, cachedTex, false, 16.0f, 640.0f,
                                         "#000000");
            producer.EndBlock();
            checkFragments(*first, "worker-owned cached formula");
            expectedRenderId = first->fragments.front().renderId;
            expectedSvg = *first->fragments.front().svg;
            transferred.emplace(std::move(producer));
        } catch (...) {
            workerFailure = std::current_exception();
        }
    });
    worker.join();
    if (workerFailure)
        std::rethrow_exception(workerFailure);
    require(transferred.has_value(), "worker transfers the renderer cache");

    auto cached = transferred->Render(cachedTex, false, 16.0f, 640.0f,
                                      "#000000");
    checkGeometry(*cached, "transferred cache hit on main thread");
    require(cached->fragments.front().renderId == expectedRenderId,
            "cross-thread cache hit preserves the worker render id");
    auto transferStats = transferred->Snapshot();
    require(transferStats.nativeRenders == 1 && transferStats.pending == 0,
            "UI-side cache lookup reuses the transferred prepared SVG without native work");
    auto cachedSvg = transferred->GetSvg(expectedRenderId);
    if (!cachedSvg) {
        transferred->SetVisibleSvgIds(std::span{&expectedRenderId, std::size_t{1}});
        WaitForIdleBounded(*transferred);
        cachedSvg = transferred->GetSvg(expectedRenderId);
    }
    require(cachedSvg && *cachedSvg == expectedSvg,
            "cross-thread cache move preserves the resident SVG bytes");

    auto mainThreadRender = Ready(*transferred, R"(\sqrt{31})", false, 16.0f, 640.0f,
                                  "#000000");
    checkFragments(*mainThreadRender, "new render after cross-thread cache transfer");
    std::cout << "cross-thread move=cache-preserved new-request=worker-computed\n";
}

void runIndexProbe() {
    constexpr std::size_t formulaCount = 4100;
    folia::platform::MathJaxRenderer renderer;
    std::vector<std::uint64_t> owners;
    owners.reserve(formulaCount);
    std::uint64_t firstRenderId = 0;
    std::uint64_t lastRenderId = 0;
    float firstWidth = 0.0f;
    float firstHeight = 0.0f;
    float firstVerticalAlign = 0.0f;
    float lastWidth = 0.0f;
    float lastHeight = 0.0f;
    float lastVerticalAlign = 0.0f;

    for (std::size_t i = 1; i <= formulaCount; ++i) {
        const auto tex = std::to_string(i);
        const auto owner = static_cast<std::uint64_t>(i);
        owners.push_back(owner);
        renderer.BeginBlock(owner);
        auto rendered = Ready(renderer, tex, false, 16.0f, 640.0f, "#000000");
        renderer.EndBlock();
        checkFragments(*rendered, "numeric index formula");
        if (i == 1) {
            firstRenderId = rendered->fragments.front().renderId;
            firstWidth = rendered->width;
            firstHeight = rendered->height;
            firstVerticalAlign = rendered->verticalAlign;
        }
        if (i == formulaCount) {
            lastRenderId = rendered->fragments.front().renderId;
            lastWidth = rendered->width;
            lastHeight = rendered->height;
            lastVerticalAlign = rendered->verticalAlign;
        }
    }

    renderer.RetainBlocks(owners);
    auto firstAgain = Ready(renderer, "1", false, 16.0f, 640.0f, "#000000");
    checkFragments(*firstAgain, "first formula after large index");
    require(firstAgain->fragments.front().renderId == firstRenderId
                && firstAgain->width == firstWidth && firstAgain->height == firstHeight
                && firstAgain->verticalAlign == firstVerticalAlign,
            "first formula and metrics remain indexed after 4100 renders");
    auto lastAgain = Ready(renderer, "4100", false, 16.0f, 640.0f, "#000000");
    checkFragments(*lastAgain, "last formula after large index");
    require(lastAgain->fragments.front().renderId == lastRenderId
                && lastAgain->width == lastWidth && lastAgain->height == lastHeight
                && lastAgain->verticalAlign == lastVerticalAlign,
            "last formula and metrics remain indexed after 4100 renders");
    std::cout << "math-index formulas=" << formulaCount
              << " first-last=retained render-ids-and-metrics=stable\n";
}

void runProbe() {
    folia::platform::MathJaxRenderer renderer;

    auto mathbb = Ready(renderer, R"(\mathbb{R})", false, 16.0f, 1280.0f,
                                  "#000000");
    checkFragments(*mathbb, "mathbb");
    std::cout << "mathbb fragments=" << mathbb->fragments.size() << '\n';

    constexpr std::string_view fraction = R"(\frac{1}{2})";
    auto first = Ready(renderer, fraction, true, 16.0f, 1280.0f, "#000000");
    checkFragments(*first, "fraction");
    auto same = Ready(renderer, fraction, true, 16.0f, 1280.0f, "#000000");
    require(sameMetrics(*same, *first),
            "identical cache key preserves render ids and metrics");

    auto recolored = Ready(renderer, fraction, true, 16.0f, 1280.0f, "#c02020");
    checkFragments(*recolored, "recolored fraction");
    require(!sameRenderIds(*recolored, *first), "color participates in cache key");
    auto resized = Ready(renderer, fraction, true, 16.0f, 720.0f, "#000000");
    checkFragments(*resized, "resized fraction");
    require(!sameRenderIds(*resized, *first), "container width participates in cache key");
    std::cout << "fraction cache=stable color-key=distinct width-key=distinct\n";

    constexpr std::string_view unicode = "\xCE\xB1+\xCE\xB2";
    auto unicodeResult = Ready(renderer, unicode, false, 16.0f, 1280.0f, "#000000");
    checkFragments(*unicodeResult, "raw Unicode");
    std::cout << "raw-unicode fragments=" << unicodeResult->fragments.size() << '\n';

    constexpr std::string_view malformed = R"(\frac{a}{)";
    auto formulaError = Ready(renderer, malformed, false, 16.0f, 1280.0f,
                                        "#000000");
    require(formulaError->errorKind == folia::platform::MathJaxErrorKind::Formula,
            "malformed TeX is classified as a formula diagnostic");
    require(!formulaError->error.empty(), "formula diagnostic retains its message");
    checkFragments(*formulaError, "malformed formula output");
    std::cout << "malformed kind=Formula fragments=" << formulaError->fragments.size() << '\n';

    renderer.Clear();
    auto afterClear = Ready(renderer, fraction, true, 16.0f, 1280.0f, "#000000");
    checkFragments(*afterClear, "fraction after Clear");
    require(!sameRenderIds(*afterClear, *first), "Clear removes the prior cached result");
    std::cout << "clear=rerendered\n";

    testOwnership();
    testAsyncEditsAndClear();
    testCrossThreadRendererMove();
    testAbandonedBatch();
    testDocumentSvgEviction();

}
} // namespace

int wmain(int argc, wchar_t** argv) {
    std::cout << std::unitbuf;
    try {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--cancel-only") {
            runCancellationProbe();
            return 0;
        }
        if (argc == 2 && std::wstring_view(argv[1]) == L"--index-only") {
            runIndexProbe();
            return 0;
        }
        if (argc == 3 && std::wstring_view(argv[1]) == L"--cache-only") {
            runCacheProbe(std::filesystem::path(argv[2]));
            return 0;
        }
        if (argc == 3 && std::wstring_view(argv[1]) == L"--store-child") {
            testChildStore(std::filesystem::path(argv[2]));
            return 0;
        }
        runProbe();
        testStore();
        std::cout << "math-index >4096 formulas: not exercised; store records test only\n";
        std::cout << "asynchronous math and file-backed SVG probe: PASS\n";
        return 0;
    } catch (winrt::hresult_error const& error) {
        std::cerr << "asynchronous math and file-backed SVG probe: FAIL: HRESULT=0x"
                  << std::hex << static_cast<std::uint32_t>(error.code())
                  << " message=" << winrt::to_string(error.message()) << '\n';
        return 1;
    } catch (std::exception const &error) {
        std::cerr << "asynchronous math and file-backed SVG probe: FAIL: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "asynchronous math and file-backed SVG probe: FAIL: unknown non-standard exception\n";
        return 1;
    }
}
