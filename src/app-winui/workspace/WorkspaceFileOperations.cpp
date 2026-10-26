#include "pch.h"
import std;
#include "workspace/WorkspaceFileOperations.h"
#include <wil/resource.h>

namespace winrt::Folia {
    namespace {
        std::wstring PathText(std::filesystem::path path) {
            auto result = path.lexically_normal().make_preferred().native();
            while (result.size() > path.root_path().native().size() && result.back() == L'\\') result.pop_back();
            return result;
        }
        bool Same(std::wstring_view a, std::wstring_view b) {
            return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
        }
        bool Contains(std::filesystem::path const& parent, std::filesystem::path const& child) {
            auto a = PathText(parent), b = PathText(child);
            if (Same(a, b)) return true;
            if (a.empty() || b.size() <= a.size() || !Same(a, std::wstring_view(b).substr(0, a.size()))) return false;
            return a.back() == L'\\' || b[a.size()] == L'\\';
        }
        struct StagedCopy {
            std::filesystem::path path;
            bool owned = false;
            ~StagedCopy() {
                if (!owned) return;
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        };
        std::filesystem::path StageName(std::filesystem::path const& parent) {
            GUID id{};
            winrt::check_hresult(::CoCreateGuid(&id));
            wchar_t text[40]{};
            winrt::check_bool(::StringFromGUID2(id, text, 40) != 0);
            return parent / (std::wstring(L".folia-copy-") + text);
        }
        struct CopyProgress {
            WorkspaceFileProgressHandler handler;
            std::filesystem::path path;
            ULONGLONG lastUpdate = 0;
            std::exception_ptr failure;
            static DWORD CALLBACK Report(LARGE_INTEGER total, LARGE_INTEGER completed, LARGE_INTEGER, LARGE_INTEGER,
                DWORD, DWORD, HANDLE, HANDLE, LPVOID context) noexcept {
                auto& self = *static_cast<CopyProgress*>(context);
                const auto now = ::GetTickCount64();
                if (self.handler && (self.lastUpdate == 0 || now - self.lastUpdate >= 100)) {
                    self.lastUpdate = now;
                    try { self.handler({self.path, static_cast<std::uint64_t>(completed.QuadPart), static_cast<std::uint64_t>(total.QuadPart)}); }
                    catch (...) { self.failure = std::current_exception(); return PROGRESS_CANCEL; }
                }
                return PROGRESS_CONTINUE;
            }
            void Copy(std::filesystem::path const& source, std::filesystem::path const& target, bool exclusive) {
                path = source;
                auto success = ::CopyFileExW(source.c_str(), target.c_str(), Report, this, nullptr, exclusive ? COPY_FILE_FAIL_IF_EXISTS : 0);
                if (failure) std::rethrow_exception(failure);
                winrt::check_bool(success);
            }
        };
        std::filesystem::path CopyExclusive(WorkspaceFileAction action, WorkspaceFileProgressHandler progress) {
            CopyProgress copy{std::move(progress)};
            const bool sameFolderCopy = Same(PathText(action.source), PathText(action.target));
            auto parent = action.target.parent_path();
            auto base = action.directory ? action.target.filename() : action.target.stem();
            auto extension = action.directory ? std::filesystem::path{} : action.target.extension();
            unsigned copyIndex = 0;
            auto nextCopyName = [&] {
                do {
                    ++copyIndex;
                    auto suffix = std::wstring(L" copy") + (copyIndex > 1 ? L" " + std::to_wstring(copyIndex) : L"");
                    action.target = parent / (base.native() + suffix + extension.native());
                } while (std::filesystem::exists(action.target));
            };
            if (sameFolderCopy) nextCopyName();
            else if (std::filesystem::exists(action.target)) throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
            StagedCopy stage{StageName(action.target.parent_path())};
            if (action.directory) {
                winrt::check_bool(::CreateDirectoryW(stage.path.c_str(), nullptr));
                stage.owned = true;
                for (auto const& item : std::filesystem::recursive_directory_iterator(action.source)) {
                    auto target = stage.path / item.path().lexically_relative(action.source);
                    auto status = item.symlink_status();
                    if (std::filesystem::is_symlink(status)) std::filesystem::copy_symlink(item.path(), target);
                    else if (std::filesystem::is_directory(status)) std::filesystem::create_directory(target);
                    else copy.Copy(item.path(), target, true);
                }
            } else {
                // Claim the unique staging name before copying so cleanup can
                // remove only a file created by this request, including failure.
                wil::unique_hfile file(::CreateFileW(stage.path.c_str(), GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
                winrt::check_bool(file.is_valid());
                stage.owned = true;
                file.reset();
                copy.Copy(action.source, stage.path, false);
            }
            while (!::MoveFileW(stage.path.c_str(), action.target.c_str())) {
                auto error = ::GetLastError();
                if (sameFolderCopy && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)) nextCopyName();
                else throw winrt::hresult_error(HRESULT_FROM_WIN32(error));
            }
            stage.owned = false;
            return action.target;
        }
    }

    bool ValidWorkspaceEntryName(std::wstring_view name) noexcept {
        if (name.empty() || name.size() > 255 || name == L"." || name == L".." || name.back() == L'.' || name.back() == L' ') return false;
        for (auto c : name) if (c < 32 || std::wstring_view(LR"(<>:"/\|?*)").find(c) != std::wstring_view::npos) return false;
        auto base = name.substr(0, name.find(L'.'));
        while (!base.empty() && base.back() == L' ') base.remove_suffix(1);
        for (auto reserved : {L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$"}) if (Same(base, reserved)) return false;
        if (base.size() == 4 && (Same(base.substr(0, 3), L"COM") || Same(base.substr(0, 3), L"LPT"))
            && std::wstring_view(L"123456789¹²³").find(base[3]) != std::wstring_view::npos) return false;
        return true;
    }

    std::optional<std::filesystem::path> RelocatedWorkspacePath(
        WorkspaceFileAction const& action, std::filesystem::path const& path) {
        if (action.kind != WorkspaceFileActionKind::Move && action.kind != WorkspaceFileActionKind::Rename) return std::nullopt;
        auto source = PathText(action.source), candidate = PathText(path);
        if (Same(source, candidate)) return action.target;
        if (!action.directory || !Contains(action.source, path)) return std::nullopt;
        auto offset = source.size();
        if (offset < candidate.size() && candidate[offset] == L'\\') ++offset;
        return action.target / candidate.substr(offset);
    }

    Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyWorkspaceFileActionAsync(WorkspaceFileAction action, WorkspaceFileProgressHandler progress) {
        co_await winrt::resume_background();
        if (!action.target.is_absolute() || !ValidWorkspaceEntryName(action.target.filename().native()))
            throw winrt::hresult_invalid_argument(L"Invalid destination name");
        switch (action.kind) {
        case WorkspaceFileActionKind::NewFile: {
            wil::unique_hfile file(::CreateFileW(action.target.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            winrt::check_bool(file.is_valid());
            co_return winrt::hstring(action.target.c_str());
        }
        case WorkspaceFileActionKind::NewFolder:
            winrt::check_bool(::CreateDirectoryW(action.target.c_str(), nullptr));
            co_return winrt::hstring(action.target.c_str());
        default: break;
        }
        if (!action.source.is_absolute()) throw winrt::hresult_invalid_argument(L"Invalid source path");
        if (action.kind != WorkspaceFileActionKind::Copy && PathText(action.source) == PathText(action.target)) co_return winrt::hstring(action.target.c_str());
        if (std::filesystem::is_directory(action.source) != action.directory)
            throw winrt::hresult_invalid_argument(L"The source item changed; refresh the folder first");
        const bool copyBesideSource = action.kind == WorkspaceFileActionKind::Copy && Same(PathText(action.source), PathText(action.target));
        if (action.directory && !copyBesideSource && !Same(PathText(action.source), PathText(action.target)) && Contains(std::filesystem::weakly_canonical(action.source),
                std::filesystem::weakly_canonical(action.target.parent_path()) / action.target.filename()))
            throw winrt::hresult_invalid_argument(L"A folder cannot be placed inside itself");
        if (action.kind == WorkspaceFileActionKind::Copy) action.target = CopyExclusive(std::move(action), std::move(progress));
        else winrt::check_bool(::MoveFileW(action.source.c_str(), action.target.c_str()));
        co_return winrt::hstring(action.target.c_str());
    }
}
