#include "pch.h"
#include "agent/ChatStore.h"
#include "agent/ChatFiles.h"
#include "storage/AssetPaths.h"
#include "storage/JsonRecords.h"
#include <winrt/Windows.Security.Cryptography.h>

namespace winrt::Folia::chat {
    using namespace records;

    namespace storage {
        std::mutex files;
        std::filesystem::path LogFolder() { return AssetPath(L"agent-chat") / L"conversations"; }
    }

    using namespace storage;
    namespace {
        constexpr std::size_t MaxImageBytes = 1536 * 1024;

        Conversation DecodeChat(Json const& data, std::string id) {
            if (data.at("schemaVersion") != 1) throw std::invalid_argument("Unsupported conversation version");
            return {std::move(id), data.at("title").get<std::string>(), data.at("model").get<std::string>(),
                data.at("effort").get<std::string>(), std::wstring(to_hstring(data.at("folder").get<std::string>())),
                data.at("openTabs").get<bool>(), data.at("search").get<bool>(), data.at("readPages").get<bool>()};
        }

        void WriteConversation(std::filesystem::path const& path, Conversation const& conversation, bool autoTitle) {
            Write(path, {{"schemaVersion", 1}, {"title", conversation.title}, {"model", conversation.model},
                 {"effort", conversation.effort}, {"folder", to_string(hstring(conversation.folder.native()))},
                 {"openTabs", conversation.openTabs}, {"search", conversation.search}, {"readPages", conversation.readPages}, {"autoTitle", autoTitle}});
        }
    }

    std::vector<Conversation> Conversations() {
        std::lock_guard lock(files);
        return List<Conversation>(LogFolder(), DecodeChat);
    }

    bool CanGenerateTitle(std::string const& id) {
        std::lock_guard lock(files);
        return Read(Path(LogFolder(), id, L".json")).at("autoTitle").get<bool>();
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
            WriteConversation(path, conversation, stored.at("autoTitle").get<bool>());
    }

    Conversation RenameConversation(std::string const& id, std::string title, TitleSource source) {
        std::lock_guard lock(files);
        auto path = Path(LogFolder(), id, L".json");
        auto stored = Read(path);
        auto conversation = DecodeChat(stored, id);
        if (source == TitleSource::User || stored.at("autoTitle").get<bool>()) {
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

    std::filesystem::path AttachmentPath(std::string const& chat, std::string const& id) {
        return Path(Path(LogFolder(), chat, L".media"), id, L".png");
    }

    std::string StoreImage(std::string const& chat, std::span<std::uint8_t const> png) {
        if (png.empty() || png.size() > MaxImageBytes) throw std::invalid_argument("Invalid attachment size");
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
        if (!size || size > MaxImageBytes) throw std::runtime_error("Invalid attachment size");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream stream(path, std::ios::binary);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) throw std::runtime_error("Could not read attachment");
        using Windows::Security::Cryptography::CryptographicBuffer;
        return {folia::ai::ContentKind::Image, {}, "image/png", to_string(CryptographicBuffer::EncodeToBase64String(
            CryptographicBuffer::CreateFromByteArray(bytes)))};
    }

}
