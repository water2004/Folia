#include "pch.h"
#include "agent/ChatStore.h"
#include "storage/AssetPaths.h"
#include "storage/JsonRecords.h"
#include <wil/resource.h>
#include <winrt/Windows.Security.Cryptography.h>

namespace winrt::Folia::chat {
    using namespace records;
    using folia::ai::MessageRole;

    namespace {
        std::mutex files;

        auto LogFolder() { return AssetPath(L"agent-chat") / L"conversations"; }

        Conversation DecodeChat(Json const& data, std::string id) {
            return {std::move(id), data.at("title").get<std::string>(), data.value("model", ""),
                data.value("effort", ""), std::wstring(to_hstring(data.value("folder", ""))),
                data.value("openTabs", false), data.value("search", false)};
        }

        void WriteConversation(std::filesystem::path const& path, Conversation const& conversation, bool autoTitle) {
            Write(path, {{"title", conversation.title}, {"model", conversation.model},
                 {"effort", conversation.effort}, {"folder", to_string(hstring(conversation.folder.native()))},
                 {"openTabs", conversation.openTabs}, {"search", conversation.search}, {"autoTitle", autoTitle}});
        }
    }

    std::vector<Conversation> Conversations() {
        std::lock_guard lock(files);
        return List<Conversation>(LogFolder(), DecodeChat);
    }

    bool CanGenerateTitle(std::string const& id) {
        std::lock_guard lock(files);
        return Read(Path(LogFolder(), id, L".json")).value("autoTitle", false);
    }

    Conversation LoadConversation(std::string const& id) {
        std::lock_guard lock(files);
        return DecodeChat(Read(Path(LogFolder(), id, L".json")), id);
    }

    Conversation NewConversation(std::string title) {
        std::lock_guard lock(files);
        Conversation conversation;
        conversation.id = NewId();
        conversation.title = std::move(title);
        WriteConversation(Path(LogFolder(), conversation.id, L".json"), conversation, true);
        return conversation;
    }

    void SaveConversationOptions(Conversation& conversation) {
        std::lock_guard lock(files);
        auto path = Path(LogFolder(), conversation.id, L".json");
        auto stored = Read(path);
        conversation.title = stored.at("title").get<std::string>();
        if (conversation != DecodeChat(stored, conversation.id))
            WriteConversation(path, conversation, stored.value("autoTitle", false));
    }

    Conversation RenameConversation(std::string const& id, std::string title, TitleSource source) {
        std::lock_guard lock(files);
        auto path = Path(LogFolder(), id, L".json");
        auto stored = Read(path);
        auto conversation = DecodeChat(stored, id);
        if (source == TitleSource::User || stored.value("autoTitle", false)) {
            conversation.title = std::move(title);
            WriteConversation(path, conversation, source == TitleSource::Prompt);
        }
        return conversation;
    }

    void DeleteConversation(std::string const& id) {
        std::lock_guard lock(files);
        auto path = Path(LogFolder(), id, L".jsonl");
        std::filesystem::remove(path);
        std::filesystem::remove(Path(LogFolder(), id, L".json"));
        std::filesystem::remove_all(Path(LogFolder(), id, L".media"));
    }

    namespace {
        Entry DecodeEntry(Json const& event) {
            auto role = event.at("role").get<std::string>();
            if (role != "assistant" && role != "user") throw std::invalid_argument("Unknown message role");
            Entry entry{role == "assistant" ? MessageRole::Assistant : MessageRole::User, event.at("markdown").get<std::string>()};
            if (auto sources = event.find("sources"); sources != event.end() && sources->is_array())
                for (auto const& item : *sources)
                    entry.sources.push_back({item.at("title").get<std::string>(), item.at("url").get<std::string>()});
            entry.reasoning = event.value("reasoning", "");
            for (auto const& caption : event.value("reasoningSummaries", Json::array())) {
                auto offset = caption.at("offset").get<std::size_t>();
                if (offset < entry.reasoning.size()) entry.reasoningSummaries.push_back({offset, caption.at("text").get<std::string>()});
            }
            entry.modelId = event.value("modelId", "");
            if (event.contains("inputTokens")) entry.usage.inputTokens = event.at("inputTokens").get<std::uint64_t>();
            if (event.contains("outputTokens")) entry.usage.outputTokens = event.at("outputTokens").get<std::uint64_t>();
            if (auto items = event.find("tools"); items != event.end() && items->is_array())
                for (auto const& tool : *items)
                    entry.activities.push_back({tool.at("name").get<std::string>(), tool.value("target", ""), tool.value("failed", false), tool.value("detail", ""), tool.value("pending", false)});
            folia::ai::agent::StepEnd last;
            for (auto const& item : event.value("steps", Json::array())) {
                folia::ai::agent::StepEnd end{item.at(0), item.at(1), item.at(2)};
                if (end.text < last.text || end.text > entry.markdown.size() ||
                    end.reasoning < last.reasoning || end.reasoning > entry.reasoning.size() ||
                    end.tools < last.tools || end.tools > entry.activities.size()) break;
                if (item.size() > 3) {
                    auto notice = item.at(3).get<std::string>();
                    if (notice == "compact") end.notice = folia::ai::agent::StepEnd::Notice::Compaction;
                    if (notice == "retry") { end.notice = folia::ai::agent::StepEnd::Notice::Retry; end.attempt = item.at(4); }
                }
                entry.steps.push_back(end);
                last = end;
            }
            for (auto const& doc : event.value("documents", Json::array()))
                entry.documents.push_back({doc.value("tab", ""), doc.value("path", ""), doc.value("token", "")});
            entry.error = event.value("error", std::string{});
            entry.outputLimit = event.value("outputLimit", false);
            entry.interrupted = event.value("interrupted", false);
            entry.images = event.value("images", std::vector<std::string>{});
            return entry;
        }

        // Journal offsets point to line starts. Scan back in blocks so opening
        // the newest turns never parses the rest of a long conversation.
        class ReverseJournal {
        public:
            ReverseJournal(std::filesystem::path const& path, std::optional<std::uintmax_t> before)
                : stream_(path, std::ios::binary) {
                if (!stream_) {
                    if (std::filesystem::exists(path)) throw std::runtime_error("Could not read conversation");
                    return;
                }
                auto size = std::filesystem::file_size(path);
                end_ = (std::min)(size, before.value_or(size));
            }

            std::optional<std::pair<Json, Entry>> Previous() {
                for (;;) {
                    std::vector<std::string> suffix;
                    std::size_t total = 0;
                    while (buffer_.find_last_of('\n') == buffer_.npos && end_) {
                        total += buffer_.size();
                        if (!buffer_.empty()) suffix.push_back(std::move(buffer_));
                        auto bytes = (std::min)(end_, std::uintmax_t{16384});
                        end_ -= bytes;
                        stream_.seekg(end_);
                        buffer_.resize(static_cast<std::size_t>(bytes));
                        if (!stream_.read(buffer_.data(), bytes)) throw std::runtime_error("Could not read conversation");
                    }
                    if (buffer_.empty() && suffix.empty()) return {};
                    auto split = buffer_.rfind('\n');
                    auto start = split == buffer_.npos ? 0 : split + 1;
                    auto line = buffer_.substr(start);
                    buffer_.resize(start ? start - 1 : 0);
                    line.reserve(line.size() + total);
                    for (auto const& part : suffix | std::views::reverse) line.append(part);
                    auto event = Json::parse(line, nullptr, false);
                    if (!event.is_object()) continue;
                    try {
                        auto entry = DecodeEntry(event);
                        return std::pair{std::move(event), std::move(entry)};
                    } catch (Json::exception const&) {} catch (std::invalid_argument const&) {}
                }
            }
            std::uintmax_t Position() const { return end_ + buffer_.size(); }
        private:
            std::ifstream stream_;
            std::uintmax_t end_ = 0;
            std::string buffer_;
        };
    }

    HistoryPage ReadHistoryPage(std::string const& id, std::optional<std::uintmax_t> before) {
        std::lock_guard lock(files);
        ReverseJournal journal(Path(LogFolder(), id, L".jsonl"), before);
        HistoryPage page;
        unsigned turns = 0;
        while (auto record = journal.Previous()) {
            auto user = record->second.role == MessageRole::User;
            page.entries.push_back(std::move(record->second));
            if (user && ++turns == 4) {
                auto boundary = journal.Position();
                if (journal.Previous()) page.before = boundary;
                break;
            }
        }
        std::ranges::reverse(page.entries);
        return page;
    }

    History LoadHistory(std::string const& id) {
        std::lock_guard lock(files);
        ReverseJournal journal(Path(LogFolder(), id, L".jsonl"), {});
        History history;
        std::optional<std::size_t> needed;
        while (auto record = journal.Previous()) {
            auto& [event, entry] = *record;
            if (!needed && event.contains("retained") && event.contains("summary")) {
                needed = history.entries.size() + event.at("retained").get<std::size_t>();
                history.summary = event.at("summary").get<std::string>();
                history.retired = event.at("summarized").get<std::size_t>();
                history.summarized = 0;
            } else if (!needed && history.summary.empty() && event.contains("summary")) {
                history.summary = event.at("summary").get<std::string>();
                history.summarized = event.value("summarized", std::size_t{});
            }
            history.entries.push_back(std::move(entry));
            if (needed && history.entries.size() >= *needed) break;
        }
        std::ranges::reverse(history.entries);
        return history;
    }

    std::filesystem::path AttachmentPath(std::string const& chat, std::string const& id) {
        return Path(Path(LogFolder(), chat, L".media"), id, L".png");
    }

    std::string StoreImage(std::string const& chat, std::span<std::uint8_t const> png) {
        auto id = NewId();
        auto path = AttachmentPath(chat, id);
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(png.data()), png.size());
        stream.close();
        if (!stream) { DeleteFileW(path.c_str()); throw std::runtime_error("Could not save image"); }
        return id;
    }

    folia::ai::ContentPart LoadImage(std::string const& chat, std::string const& id) {
        auto path = AttachmentPath(chat, id);
        auto size = std::filesystem::file_size(path);
        if (!size || size > 1536 * 1024) throw std::runtime_error("Invalid attachment size");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream stream(path, std::ios::binary);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) throw std::runtime_error("Could not read attachment");
        using Windows::Security::Cryptography::CryptographicBuffer;
        return {folia::ai::ContentKind::Image, {}, "image/png", to_string(CryptographicBuffer::EncodeToBase64String(
            CryptographicBuffer::CreateFromByteArray(bytes)))};
    }

    void AppendEntry(std::string const& id, Entry const& entry, History const* checkpoint, std::size_t skippedTail) {
        std::lock_guard lock(files);
        auto path = Path(LogFolder(), id, L".jsonl");
        if (!std::filesystem::exists(Path(LogFolder(), id, L".json")))
            throw std::runtime_error("Conversation was removed");
        Json value{{"role", entry.role == MessageRole::User ? "user" : "assistant"}, {"markdown", entry.markdown}};
        if (!entry.sources.empty()) {
            value["sources"] = Json::array();
            for (auto const& source : entry.sources)
                value["sources"].push_back({{"title", source.title}, {"url", source.url}});
        }
        if (!entry.reasoning.empty()) value["reasoning"] = entry.reasoning;
        if (!entry.modelId.empty()) value["modelId"] = entry.modelId;
        if (entry.usage.inputTokens) value["inputTokens"] = *entry.usage.inputTokens;
        if (entry.usage.outputTokens) value["outputTokens"] = *entry.usage.outputTokens;
        for (auto const& tool : entry.activities)
            value["tools"].push_back({{"name", tool.name}, {"target", tool.target}, {"failed", tool.failed}, {"detail", tool.detail}, {"pending", tool.pending}});
        for (auto const& caption : entry.reasoningSummaries)
            value["reasoningSummaries"].push_back({{"offset", caption.offset}, {"text", caption.text}});
        for (auto const& end : entry.steps) {
            Json marker = {end.text, end.reasoning, end.tools};
            if (end.notice == folia::ai::agent::StepEnd::Notice::Compaction) marker.push_back("compact");
            if (end.notice == folia::ai::agent::StepEnd::Notice::Retry) { marker.push_back("retry"); marker.push_back(end.attempt); }
            value["steps"].push_back(std::move(marker));
        }
        for (auto const& doc : entry.documents) value["documents"].push_back({{"tab", doc.tabKey}, {"path", doc.path}, {"token", doc.token}});
        if (!entry.error.empty()) value["error"] = entry.error;
        if (entry.outputLimit) value["outputLimit"] = true;
        if (entry.interrupted) value["interrupted"] = true;
        if (!entry.images.empty()) value["images"] = entry.images;
        if (checkpoint && (checkpoint->summarized || checkpoint->retired)) {
            value["summary"] = checkpoint->summary;
            value["summarized"] = checkpoint->retired + checkpoint->summarized;
            value["retained"] = checkpoint->entries.size() - checkpoint->summarized + skippedTail + 1;
        }
        // A leading separator lets the next event recover after a partial tail write.
        auto line = "\n" + value.dump() + "\n";
        wil::unique_hfile file(CreateFileW(path.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
        DWORD count{};
        if (!file ||
            !WriteFile(file.get(), line.data(), static_cast<DWORD>(line.size()), &count, nullptr) || count != line.size())
            throw std::runtime_error("Could not save conversation message");
    }
}
