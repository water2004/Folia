#include "pch.h"
#include "agent/ChatTools.h"
#include <wil/resource.h>

namespace winrt::Folia::chat {
    using folia::ai::Json;
    using folia::ai::agent::Tool;

    namespace {
        constexpr std::size_t pageBytes = 3'000;

        Json Error(char const* message) { return {{"error", message}}; }

        std::size_t Offset(Json const& args) {
            auto offset = args.value("offset", std::int64_t{});
            if (offset < 0 || std::cmp_greater(offset, (std::numeric_limits<std::size_t>::max)())) throw std::out_of_range("offset");
            return static_cast<std::size_t>(offset);
        }

        Json Page(std::string_view text, std::size_t offset, bool more = false) {
            if (offset > text.size()) return Error("Offset outside source");
            auto end = (std::min)(text.size(), offset + pageBytes);
            while (end < text.size() && end > offset && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
            auto chunk = text.substr(offset, end - offset);
            if (chunk.find('\0') != chunk.npos) return Error("Only UTF-8 text is supported");
            std::string utf8(chunk);
            if (!utf8.empty() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                static_cast<int>(utf8.size()), nullptr, 0) == 0) return Error("Only UTF-8 text is supported");
            Json result{{"text", std::move(utf8)}};
            result["next_offset"] = end < text.size() || more ? Json(end) : Json(nullptr);
            return result;
        }

        // Search byte offsets are directly usable by both read tools. Keep enough overlap
        // for matches and snippets spanning I/O chunks, including multi-byte characters.
        template<class Read>
        Json Search(Json const& args, std::size_t size, Read read, std::stop_token stop) {
            auto query = args.at("query").get<std::string>();
            if (query.empty() || query.size() > 512) return Error("Use a literal query of 1 to 512 UTF-8 bytes");
            auto pos = Offset(args);
            if (pos > size) return Error("Offset outside source");
            Json hits = Json::array();
            auto end = pos + (std::min)(size - pos, std::size_t{8 * 1024 * 1024});
            while (pos < end && !stop.stop_requested()) {
                auto length = (std::min)(end - pos, std::size_t{64 * 1024});
                auto text = read(pos, (std::min)(size - pos, length + query.size() + 96));
                if (text.size() < length) return Error("Document changed during search");
                if (text.find('\0') != text.npos) return Error("Only UTF-8 text is supported");
                for (auto at = text.find(query); at < length; at = text.find(query, at + query.size())) {
                    auto first = at > 72 ? at - 72 : 0;
                    while (first < at && (static_cast<unsigned char>(text[first]) & 0xc0) == 0x80) ++first;
                    auto last = (std::min)(text.size(), at + query.size() + 88);
                    while (last > at && last < text.size() && (static_cast<unsigned char>(text[last]) & 0xc0) == 0x80) --last;
                    hits.push_back({{"offset", pos + at}, {"snippet", std::string(text.substr(first, last - first))}});
                    if (hits.size() == 12) {
                        auto next = pos + at + query.size();
                        return {{"matches", hits}, {"next_offset", next < size ? Json(next) : Json(nullptr)}};
                    }
                }
                pos += length;
            }
            return {{"matches", hits}, {"next_offset", pos < size ? Json(pos) : Json(nullptr)}};
        }

        std::wstring FinalPath(HANDLE handle) {
            constexpr DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
            auto size = GetFinalPathNameByHandleW(handle, nullptr, 0, flags);
            if (!size) throw std::runtime_error("Unresolvable file path");
            std::wstring result(size, L'\0');
            auto length = GetFinalPathNameByHandleW(handle, result.data(), size, flags);
            if (!length || length >= size) throw std::runtime_error("Unresolvable file path");
            result.resize(length);
            return result;
        }

        class Folder {
        public:
            explicit Folder(std::filesystem::path path)
                : root_(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr)),
                  path_(root_ ? FinalPath(root_.get()) : throw std::runtime_error("Could not open workspace")) {}

            Json Read(Json const& args) const {
                auto file = Open(args.at("path").get<std::string>(), GENERIC_READ);
                FILE_STANDARD_INFO info{};
                if (GetFileType(file.get()) != FILE_TYPE_DISK ||
                    !GetFileInformationByHandleEx(file.get(), FileStandardInfo, &info, sizeof(info)) || info.Directory)
                    return Error("Use a workspace text file");
                auto offset = Offset(args);
                if (std::cmp_greater(offset, info.EndOfFile.QuadPart)) return Error("Offset outside file");
                LARGE_INTEGER start{};
                start.QuadPart = offset;
                std::array<char, pageBytes + 4> data{};
                DWORD count = 0;
                if (!SetFilePointerEx(file.get(), start, nullptr, FILE_BEGIN) ||
                    !ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &count, nullptr))
                    return Error("Could not read workspace file");
                auto page = Page(std::string_view(data.data(), count), 0, std::cmp_less(offset + count, info.EndOfFile.QuadPart));
                if (auto next = page.find("next_offset"); next != page.end() && !next->is_null())
                    *next = offset + next->get<std::size_t>();
                return page;
            }

            Json Find(Json const& args, std::stop_token stop) const {
                auto file = Open(args.at("path").get<std::string>(), GENERIC_READ);
                LARGE_INTEGER size{};
                if (GetFileType(file.get()) != FILE_TYPE_DISK || !GetFileSizeEx(file.get(), &size))
                    return Error("Use a workspace text file");
                return Search(args, static_cast<std::size_t>(size.QuadPart), [&](std::size_t pos, std::size_t length) {
                    LARGE_INTEGER start{};
                    start.QuadPart = pos;
                    std::string text(length, '\0');
                    DWORD count{};
                    if (!SetFilePointerEx(file.get(), start, nullptr, FILE_BEGIN) ||
                        !ReadFile(file.get(), text.data(), static_cast<DWORD>(length), &count, nullptr))
                        throw std::runtime_error("Could not read workspace file");
                    text.resize(count);
                    return text;
                }, stop);
            }

            Json List(Json const& args, std::stop_token stop, std::wstring_view prefix = {}) const {
                auto file = Open(args.value("path", "."), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES);
                alignas(FILE_ID_BOTH_DIR_INFO) std::array<std::byte, 16384> data;
                Json content = Json::array();
                auto start = Offset(args);
                std::size_t index = 0;
                while (!stop.stop_requested()) {
                    if (!GetFileInformationByHandleEx(file.get(), FileIdBothDirectoryInfo, data.data(), static_cast<DWORD>(data.size()))) {
                        if (GetLastError() == ERROR_NO_MORE_FILES) break;
                        return Error("Could not list workspace directory");
                    }
                    std::size_t position = 0;
                    for (;;) {
                        auto const& entry = *reinterpret_cast<FILE_ID_BOTH_DIR_INFO const*>(data.data() + position);
                        auto name = std::wstring_view(entry.FileName, entry.FileNameLength / sizeof(wchar_t));
                        bool matches = prefix.empty() || (name.size() >= prefix.size() &&
                            CompareStringOrdinal(name.data(), static_cast<int>(prefix.size()), prefix.data(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL);
                        if (matches && name != L"." && name != L".." && index++ >= start) {
                            if (content.size() == 40) return {{"entries", std::move(content)}, {"next_offset", start + 40}};
                            content.push_back({{"name", to_string(hstring(name))},
                                {"directory", (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0}});
                        }
                        if (!entry.NextEntryOffset) break;
                        position += entry.NextEntryOffset;
                    }
                }
                return {{"entries", std::move(content)}, {"next_offset", nullptr}};
            }

        private:
            wil::unique_hfile Open(std::string const& relative, DWORD access) const {
                if (relative.empty() || relative.size() > 2'000 || relative.find_first_of(":\0", 0, 2) != relative.npos)
                    throw std::invalid_argument("Invalid workspace path");
                std::filesystem::path tail(std::wstring(to_hstring(relative)));
                if (tail.has_root_path() || std::ranges::any_of(tail, [](auto const& part) { return part == L".."; }))
                    throw std::invalid_argument("Use a relative path inside the workspace");
                tail = tail.lexically_normal().make_preferred();
                auto path = std::filesystem::path(path_);
                if (tail != L".") path /= tail;
                wil::unique_hfile file(CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
                if (!file) throw std::runtime_error("Could not open workspace path");
                auto actual = FinalPath(file.get());
                // Use the resolved path of the same handle subsequently read. Case-sensitive
                // comparison also prevents aliasing siblings in a case-sensitive NTFS directory.
                if (actual != path_ && !(actual.starts_with(path_) &&
                    (path_.ends_with(L'\\') || (actual.size() > path_.size() && actual[path_.size()] == L'\\'))))
                    throw std::runtime_error("Path outside workspace");
                return file;
            }

            wil::unique_hfile root_;
            std::wstring path_;
        };

        Tool Define(char const* name, char const* description, Json properties, Json required, decltype(Tool::invoke) invoke) {
            return {{name, description, {{"type", "object"}, {"properties", std::move(properties)},
                       {"required", std::move(required)}, {"additionalProperties", false}}}, std::move(invoke)};
        }
    }

    std::vector<std::string> FindWorkspaceMentions(std::filesystem::path const& root, std::string const& prefix, std::stop_token stop) {
        auto slash = prefix.find_last_of('/');
        auto parent = slash == prefix.npos ? std::string{} : prefix.substr(0, slash + 1);
        auto leaf = to_hstring(prefix.substr(parent.size()));
        auto entries = Folder(root).List({{"path", parent.empty() ? "." : parent}}, stop, leaf);
        std::vector<std::string> matches;
        for (auto const& item : entries.value("entries", Json::array()))
            matches.push_back(parent + item.at("name").get<std::string>() + (item.at("directory").get<bool>() ? "/" : ""));
        return matches;
    }

    std::vector<Tool> MakeTools(std::optional<std::vector<OpenTab>> tabs, std::filesystem::path const& folder) {
        std::vector<Tool> tools;
        Json offset{{"type", "integer"}, {"description", "Omit for the first page. Use next_offset for the next page."}};
        if (tabs) {
            auto source = std::make_shared<std::vector<OpenTab> const>(std::move(*tabs));
            auto snapshot = [source, cachedId = std::size_t{}, cached = TabSnapshot{}](Json const& args, std::stop_token stop) mutable -> TabSnapshot const& {
                auto id = args.at("id").get<std::size_t>();
                if (!id || id > source->size() || !source->at(id - 1).read) throw std::runtime_error("Tab unavailable");
                if (id != cachedId) {
                    cached = source->at(id - 1).read(stop);
                    cachedId = id;
                }
                return cached;
            };
            auto get = std::make_shared<decltype(snapshot)>(std::move(snapshot));
            Json id{{"type", "integer"}};
            tools.push_back(Define("list_open_tabs", "List Folia Markdown tabs. Supply offset to continue.",
                {{"offset", offset}}, Json::array(), [source](auto const& args, auto) {
                    Json items = Json::array();
                    auto index = Offset(args);
                    for (; index < source->size() && items.size() < 32; ++index)
                        items.push_back({{"id", index + 1}, {"title", source->at(index).title}, {"readable", bool(source->at(index).read)}});
                    return Json{{"tabs", items}, {"next_offset", index < source->size() ? Json(index) : Json(nullptr)}};
                }));
            tools.push_back(Define("read_open_tab", "Read Markdown at a UTF-8 byte offset, including unsaved edits.",
                {{"id", id}, {"offset", offset}}, Json::array({"id"}), [get](auto const& args, auto stop) {
                    return Page((*get)(args, stop).text, Offset(args));
                }));
            tools.push_back(Define("search_open_tab", "Find literal text throughout an open document. Matches include snippets and byte offsets for read_open_tab.",
                {{"id", id}, {"query", {{"type", "string"}}}, {"offset", offset}}, Json::array({"id", "query"}), [get](auto const& args, auto stop) {
                    std::string_view text((*get)(args, stop).text);
                    return Search(args, text.size(), [text](auto at, auto length) { return text.substr(at, length); }, stop);
                }));
            tools.push_back(Define("outline_open_tab", "List parsed Markdown headings and levels. Search a heading to locate its section.",
                {{"id", id}, {"offset", offset}}, Json::array({"id"}), [get](auto const& args, auto stop) {
                    auto const& outline = (*get)(args, stop).outline;
                    Json items = Json::array();
                    auto index = Offset(args);
                    for (; index < outline.size() && items.size() < 32; ++index) {
                        auto const& [level, heading] = outline[index];
                        auto length = (std::min)(heading.size(), std::size_t{192});
                        while (length < heading.size() && length && (static_cast<unsigned char>(heading[length]) & 0xc0) == 0x80) --length;
                        items.push_back({{"level", level}, {"heading", heading.substr(0, length)}});
                    }
                    return Json{{"headings", items}, {"next_offset", index < outline.size() ? Json(index) : Json(nullptr)}};
                }));
        }
        if (!folder.empty()) {
            auto workspace = std::make_shared<Folder>(folder);
            Json path{{"type", "string"}, {"description", "Workspace-relative path"}};
            tools.push_back(Define("list_workspace", "List a page of directory entries. Omit path for the root directory.",
                {{"path", path}, {"offset", offset}}, Json::array(), [workspace](auto const& args, auto stop) { return workspace->List(args, stop); }));
            tools.push_back(Define("read_workspace_file", "Read a page of a UTF-8 text file within the approved workspace.",
                {{"path", path}, {"offset", offset}}, Json::array({"path"}), [workspace](auto const& args, auto) { return workspace->Read(args); }));
            tools.push_back(Define("search_workspace_file", "Find literal text in a workspace file and return matching snippets and UTF-8 byte offsets.",
                {{"path", path}, {"query", {{"type", "string"}}}, {"offset", offset}}, Json::array({"path", "query"}),
                [workspace](auto const& args, auto stop) { return workspace->Find(args, stop); }));
        }
        return tools;
    }
}
