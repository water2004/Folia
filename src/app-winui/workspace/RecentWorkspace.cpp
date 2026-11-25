#include "pch.h"
import std;
#include "workspace/RecentWorkspace.h"
#include "storage/AssetPaths.h"
#include <wil/resource.h>

namespace winrt::Folia {
    namespace {
        using namespace Windows::Data::Json;
        using Path = std::filesystem::path;

        bool SamePath(Path const& left, Path const& right) {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        bool LockHistory(wil::unique_handle& lock) {
            lock.reset(::CreateMutexW(nullptr, FALSE, L"Local\\Folia.RecentWorkspaceHistory"));
            if (!lock) return false;
            auto wait = ::WaitForSingleObject(lock.get(), 250);
            return wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }

        Path Normalize(Path path) {
            path = path.lexically_normal().make_preferred();
            if (!path.has_filename() && path.has_relative_path()) path = path.parent_path();
            return path;
        }

        std::vector<Path> ReadPaths(JsonObject const& root, wchar_t const* key) {
            std::vector<Path> paths;
            for (auto const& entry : root.GetNamedArray(key, JsonArray{})) {
                if (entry.ValueType() != JsonValueType::String) continue;
                auto path = Normalize(Path(entry.GetString().c_str()));
                if (!path.is_absolute() || std::ranges::any_of(paths, [&](auto const& seen) { return SamePath(seen, path); }))
                    continue;
                paths.push_back(std::move(path));
                if (paths.size() == RecentWorkspace::MaximumEntries) break;
            }
            return paths;
        }

        RecentWorkspaceEntries ReadHistory(Path const& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return {};
            std::string bytes{std::istreambuf_iterator<char>(file), {}};
            JsonObject root{nullptr};
            if (!JsonObject::TryParse(winrt::to_hstring(bytes), root)) return {};
            return {ReadPaths(root, L"files"), ReadPaths(root, L"folders")};
        }

        JsonArray WritePaths(std::vector<Path> const& paths) {
            JsonArray values;
            for (auto const& path : paths)
                values.Append(JsonValue::CreateStringValue(winrt::hstring(path.c_str())));
            return values;
        }

        void WriteHistory(Path const& path, RecentWorkspaceEntries const& entries) {
            std::filesystem::create_directories(path.parent_path());
            JsonObject root;
            root.Insert(L"files", WritePaths(entries.files));
            root.Insert(L"folders", WritePaths(entries.folders));
            auto temporary = path;
            temporary += L".tmp";
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                file << winrt::to_string(root.Stringify());
                file.close();
                if (!file) return;
            }
            if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
        }
    }

    RecentWorkspace::RecentWorkspace() : RecentWorkspace(AssetPath(L"recent-workspace.json")) {}
    RecentWorkspace::RecentWorkspace(Path file) : file_(std::move(file)) {}

    RecentWorkspaceEntries RecentWorkspace::Read() const {
        try {
            wil::unique_handle lock;
            if (!LockHistory(lock)) return {};
            auto release = wil::scope_exit([&] { ::ReleaseMutex(lock.get()); });
            return ReadHistory(file_);
        } catch (...) {
            return {};
        }
    }

    void RecentWorkspace::Remember(RecentWorkspaceKind kind, Path const& path) const {
        Update(kind, path, true);
    }

    bool RecentWorkspace::ForgetMissing(RecentWorkspaceKind kind, Path const& path, winrt::hresult error) const {
        bool missing = error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || error == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)
            || (kind == RecentWorkspaceKind::Folder && error == HRESULT_FROM_WIN32(ERROR_DIRECTORY));
        if (missing) Update(kind, path, false);
        return missing;
    }

    void RecentWorkspace::Update(RecentWorkspaceKind kind, Path path, bool remember) const {
        try {
            path = Normalize(std::move(path));
            if (!path.is_absolute()) return;
            wil::unique_handle lock;
            if (!LockHistory(lock)) return;
            auto release = wil::scope_exit([&] { ::ReleaseMutex(lock.get()); });
            auto history = ReadHistory(file_);
            auto& items = kind == RecentWorkspaceKind::File ? history.files : history.folders;
            if (remember && !items.empty() && SamePath(items.front(), path)) return;
            auto removed = std::erase_if(items, [&](auto const& item) { return SamePath(item, path); });
            if (!remember && removed == 0) return;
            if (remember) items.insert(items.begin(), std::move(path));
            if (items.size() > MaximumEntries) items.resize(MaximumEntries);
            WriteHistory(file_, history);
        } catch (...) {
            // Opening the item succeeds independently of history persistence.
        }
    }
}
