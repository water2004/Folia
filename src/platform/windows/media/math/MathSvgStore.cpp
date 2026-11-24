#include <windows.h>
#include "windows/media/math/MathSvgStore.h"

#include <objbase.h>

import std;

namespace folia::platform
{
    namespace
    {
        class UniqueHandle
        {
        public:
            explicit UniqueHandle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : handle(value) {}
            ~UniqueHandle()
            {
                if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
                    CloseHandle(handle);
            }

            UniqueHandle(UniqueHandle const&) = delete;
            UniqueHandle& operator=(UniqueHandle const&) = delete;
            UniqueHandle(UniqueHandle&& other) noexcept : handle(other.handle)
            {
                other.handle = INVALID_HANDLE_VALUE;
            }
            UniqueHandle& operator=(UniqueHandle&& other) noexcept
            {
                if (this != &other)
                {
                    reset(other.handle);
                    other.handle = INVALID_HANDLE_VALUE;
                }
                return *this;
            }

            HANDLE get() const noexcept { return handle; }
            void reset(HANDLE value = INVALID_HANDLE_VALUE) noexcept
            {
                if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
                    CloseHandle(handle);
                handle = value;
            }

        private:
            HANDLE handle;
        };

        [[noreturn]] void ThrowWindowsError(DWORD error, char const* operation)
        {
            throw std::system_error(static_cast<int>(error), std::system_category(), operation);
        }

        void Seek(HANDLE handle, std::uint64_t offset, char const* operation)
        {
            LARGE_INTEGER position{};
            position.QuadPart = static_cast<LONGLONG>(offset);
            if (!SetFilePointerEx(handle, position, nullptr, FILE_BEGIN))
                ThrowWindowsError(GetLastError(), operation);
        }

        std::filesystem::path TemporaryDirectory()
        {
            std::wstring buffer(32768, L'\0');
            const auto length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
            if (length == 0)
                ThrowWindowsError(GetLastError(), "GetTempPathW");
            if (length >= buffer.size())
                ThrowWindowsError(ERROR_INSUFFICIENT_BUFFER, "GetTempPathW");
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }

        std::filesystem::path MakeStorePath()
        {
            auto directory = TemporaryDirectory() / L"FoliaMath";
            if (!CreateDirectoryW(directory.c_str(), nullptr))
            {
                const auto error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS)
                    ThrowWindowsError(error, "CreateDirectoryW(FoliaMath)");
                const auto attributes = GetFileAttributesW(directory.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                    ThrowWindowsError(attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY,
                        "Validate FoliaMath temporary directory");
            }

            GUID id{};
            const auto guidStatus = CoCreateGuid(&id);
            if (FAILED(guidStatus))
                ThrowWindowsError(static_cast<DWORD>(HRESULT_CODE(guidStatus)), "CoCreateGuid");
            wchar_t guidText[40]{};
            if (StringFromGUID2(id, guidText, static_cast<int>(std::size(guidText))) == 0)
                throw std::runtime_error("StringFromGUID2 failed");
            return directory / (std::wstring(guidText) + L".svgstore");
        }

        UniqueHandle CreateStoreFile(std::filesystem::path const& path)
        {
            const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
                ThrowWindowsError(GetLastError(), "CreateFileW(MathSvgStore)");
            return UniqueHandle(handle);
        }

        bool IsStoreFileName(std::wstring_view name) noexcept
        {
            // StringFromGUID2's exact filename form, followed by our suffix.
            if (name.size() != 47 || name.front() != L'{' || name[37] != L'}'
                || name.substr(38) != L".svgstore") return false;
            for (std::size_t index = 1; index < 37; ++index)
            {
                auto ch = name[index];
                if (index == 9 || index == 14 || index == 19 || index == 24)
                {
                    if (ch != L'-') return false;
                }
                else if (!(ch >= L'0' && ch <= L'9')
                    && !(ch >= L'a' && ch <= L'f') && !(ch >= L'A' && ch <= L'F'))
                    return false;
            }
            return true;
        }

        void RecordOpenFailure(MathSvgStore::CleanupResult& result, DWORD error) noexcept
        {
            if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION)
                ++result.inUse;
            else if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
                || error == ERROR_DELETE_PENDING)
                ++result.skipped;
            else
                ++result.failed;
        }

        void ReclaimStoreFile(std::filesystem::path const& path,
            MathSvgStore::CleanupResult& result)
        {
            // The existing renderer holds a non-shared read/write handle.
            // A live instance therefore prevents this exclusive open. Inspect
            // and delete through the same handle, avoiding a second path lookup.
            UniqueHandle file(CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES, 0, nullptr,
                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (file.get() == INVALID_HANDLE_VALUE)
            {
                RecordOpenFailure(result, GetLastError());
                return;
            }
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file.get(), &info))
            {
                ++result.failed;
                return;
            }
            if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            {
                ++result.skipped;
                return;
            }
            FILE_DISPOSITION_INFO disposition{TRUE};
            if (SetFileInformationByHandle(file.get(), FileDispositionInfo, &disposition, sizeof(disposition)))
                ++result.removed;
            else
                ++result.failed;
        }
    }

    MathSvgStore::CleanupResult MathSvgStore::ReclaimOrphans() noexcept
    {
        CleanupResult result;
        try
        {
            auto directory = TemporaryDirectory() / L"FoliaMath";
            // Keep the directory itself open without sharing delete access,
            // so it cannot be renamed/replaced while paths are enumerated.
            UniqueHandle folder(CreateFileW(directory.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (folder.get() == INVALID_HANDLE_VALUE)
            {
                auto error = GetLastError();
                if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
                    RecordOpenFailure(result, error);
                return result;
            }
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(folder.get(), &info))
            {
                ++result.failed;
                return result;
            }
            if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            {
                ++result.skipped;
                return result;
            }
            WIN32_FIND_DATAW entry{};
            auto found = FindFirstFileExW((directory / L"*.svgstore").c_str(), FindExInfoBasic,
                &entry, FindExSearchNameMatch, nullptr, 0);
            if (found == INVALID_HANDLE_VALUE)
            {
                if (GetLastError() != ERROR_FILE_NOT_FOUND) ++result.failed;
                return result;
            }
            auto closeSearch = [](void* handle) { FindClose(handle); };
            std::unique_ptr<void, decltype(closeSearch)> search(found, closeSearch);
            for (;;)
            {
                if (!IsStoreFileName(entry.cFileName)
                    || (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                    ++result.skipped;
                else
                    ReclaimStoreFile(directory / entry.cFileName, result);
                if (!FindNextFileW(search.get(), &entry))
                {
                    if (GetLastError() != ERROR_NO_MORE_FILES) ++result.failed;
                    break;
                }
            }
        }
        catch (...)
        {
            ++result.failed;
        }
        return result;
    }

    struct MathSvgStore::State
    {
        mutable std::mutex mutex;
        UniqueHandle file;
        std::filesystem::path path;
        std::uint64_t committedSize = 0;
        bool appendFailed = false;

        void EnsureFile()
        {
            if (file.get() != INVALID_HANDLE_VALUE)
                return;
            auto createdPath = MakeStorePath();
            auto createdFile = CreateStoreFile(createdPath);
            path = std::move(createdPath);
            file = std::move(createdFile);
        }

        bool RollbackAppend() noexcept
        {
            LARGE_INTEGER position{};
            position.QuadPart = static_cast<LONGLONG>(committedSize);
            return SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN)
                && SetEndOfFile(file.get());
        }
    };

    MathSvgStore::MathSvgStore() : state(std::make_unique<State>()) {}
    MathSvgStore::~MathSvgStore() = default;
    MathSvgStore::MathSvgStore(MathSvgStore&&) noexcept = default;
    MathSvgStore& MathSvgStore::operator=(MathSvgStore&&) noexcept = default;

    MathSvgStore::Range MathSvgStore::Append(std::string_view svg)
    {
        if (!state)
            throw std::logic_error("Append called on moved-from MathSvgStore");

        std::lock_guard lock(state->mutex);
        if (svg.empty())
            return { state->committedSize, 0 };
        if (state->appendFailed)
            throw std::runtime_error("MathSvgStore append is unavailable after a failed rollback");

        const auto maxOffset = static_cast<std::uint64_t>((std::numeric_limits<LONGLONG>::max)());
        if (svg.size() > maxOffset || state->committedSize > maxOffset - static_cast<std::uint64_t>(svg.size()))
            throw std::length_error("MathSvgStore exceeds the supported file offset range");

        state->EnsureFile();
        const auto start = state->committedSize;
        Seek(state->file.get(), start, "Seek(MathSvgStore append)");

        std::size_t writtenTotal = 0;
        while (writtenTotal < svg.size())
        {
            const auto remaining = svg.size() - writtenTotal;
            const auto chunk = static_cast<DWORD>((std::min)(remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD written = 0;
            const auto writeSucceeded = WriteFile(
                state->file.get(), svg.data() + writtenTotal, chunk, &written, nullptr);
            if (!writeSucceeded || written != chunk)
            {
                const auto error = writeSucceeded ? ERROR_WRITE_FAULT : GetLastError();
                if (!state->RollbackAppend())
                    state->appendFailed = true;
                ThrowWindowsError(error, "WriteFile(MathSvgStore)");
            }
            writtenTotal += written;
        }

        state->committedSize += static_cast<std::uint64_t>(writtenTotal);
        return { start, static_cast<std::uint64_t>(writtenTotal) };
    }

    std::shared_ptr<std::string const> MathSvgStore::Read(Range range) const
    {
        if (!state)
            throw std::logic_error("Read called on moved-from MathSvgStore");

        std::lock_guard lock(state->mutex);
        if (range.offset > state->committedSize
            || range.size > state->committedSize - range.offset)
            throw std::out_of_range("MathSvgStore range exceeds committed data");
        if (range.size > std::string{}.max_size()
            || range.size > (std::numeric_limits<std::size_t>::max)())
            throw std::length_error("MathSvgStore range is too large to read");

        auto result = std::make_shared<std::string>(static_cast<std::size_t>(range.size), '\0');
        if (range.size == 0)
            return result;
        if (state->file.get() == INVALID_HANDLE_VALUE)
            throw std::runtime_error("MathSvgStore committed data has no backing file");

        Seek(state->file.get(), range.offset, "Seek(MathSvgStore read)");
        std::size_t readTotal = 0;
        while (readTotal < result->size())
        {
            const auto remaining = result->size() - readTotal;
            const auto chunk = static_cast<DWORD>((std::min)(remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD read = 0;
            if (!ReadFile(state->file.get(), result->data() + readTotal, chunk, &read, nullptr))
                ThrowWindowsError(GetLastError(), "ReadFile(MathSvgStore)");
            if (read != chunk)
                ThrowWindowsError(ERROR_HANDLE_EOF, "ReadFile(MathSvgStore) short read");
            readTotal += read;
        }
        return result;
    }

    std::filesystem::path const& MathSvgStore::Path() const noexcept
    {
        static const std::filesystem::path emptyPath;
        return state ? state->path : emptyPath;
    }
}
