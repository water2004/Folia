#pragma once

#include "providers/ModelTypes.h"
#include <wil/resource.h>
import std;

namespace winrt::Folia::records {
    using folia::ai::Json;

    inline std::string NewId() {
        GUID id{};
        check_hresult(CoCreateGuid(&id));
        wchar_t text[39]{};
        StringFromGUID2(id, text, 39);
        std::string result;
        for (auto c : std::wstring_view(text))
            if (c != L'{' && c != L'}' && c != L'-') result += static_cast<char>(std::towlower(c));
        return result;
    }

    inline std::filesystem::path Path(std::filesystem::path const& folder, std::string const& id, wchar_t const* extension) {
        if (id.size() != 32 || !std::ranges::all_of(id, [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) throw std::invalid_argument("Invalid record identifier");
        return folder / (std::wstring(to_hstring(id)) + extension);
    }

    inline Json Read(std::filesystem::path const& path) {
        if (std::filesystem::file_size(path) > 512 * 1024) throw std::runtime_error("Invalid record metadata");
        std::ifstream stream(path, std::ios::binary);
        return Json::parse(stream);
    }

    inline void Write(std::filesystem::path const& path, Json const& value) {
        std::filesystem::create_directories(path.parent_path());
        auto temp = path.parent_path() / (std::wstring(to_hstring(NewId())) + L".tmp");
        auto cleanup = wil::scope_exit([&] { DeleteFileW(temp.c_str()); });
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        stream << value.dump();
        stream.close();
        if (!stream || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not save record metadata");
    }

    template<class T, class Decode>
    std::vector<T> List(std::filesystem::path const& dir, Decode decode) {
        std::vector<std::pair<std::filesystem::file_time_type, T>> found;
        if (!std::filesystem::exists(dir)) return {};
        for (auto const& file : std::filesystem::directory_iterator(dir)) {
            if (file.path().extension() != L".json" || !file.is_regular_file()) continue;
            try { found.emplace_back(file.last_write_time(), decode(Read(file.path()), file.path().stem().string())); }
            catch (std::exception const&) { continue; }
        }
        std::ranges::sort(found, std::greater{}, &decltype(found)::value_type::first);
        std::vector<T> result;
        for (auto& [_, value] : found) result.push_back(std::move(value));
        return result;
    }
}
