#pragma once

#include "agent/ChatModelPicker.h"
#include "agent/ChatStore.h"
#include "agent/ChatTools.h"
#include "settings/storage/ModelCatalog.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include "agent/ChatComposerExtras.h"
#include "agent/ChatTranscript.h"
#include "agent/ChatPalette.h"

namespace winrt::Folia::chat {
    using models::ConfiguredModel;
    using models::ProviderConnection;
    struct ViewHost {
        HWND window{};
        std::function<std::vector<OpenTab>()> tabs;
        std::function<std::filesystem::path()> currentFolder;
        std::function<void()> settings;
        std::function<void()> newChat, rename, split, keep;
        std::function<void(std::string const&, ChatTranscript::Documents const&)> openDocument;
        std::function<void(Conversation const&)> changed;
    };

    class ChatView : public std::enable_shared_from_this<ChatView> {
    public:
        static std::shared_ptr<ChatView> Create(Conversation, ViewHost, folia::ThemeProfile const&, MediaDisplaySettings const&);
        ~ChatView();
        Microsoft::UI::Xaml::FrameworkElement Element() const { return root_; }
        void Activate();
        void Rehost(ViewHost host) { host_ = std::move(host); }
        void SetTitle(std::string title);
        void ShowSplit(bool available) { split_.Visibility(available && host_.split ? Microsoft::UI::Xaml::Visibility::Visible : Microsoft::UI::Xaml::Visibility::Collapsed); }
        void SetTheme(folia::ThemeProfile const&);
        void SetMediaDisplaySettings(MediaDisplaySettings const& media) { transcript_->SetMediaDisplaySettings(media); }

    private:
        static constexpr std::size_t maxQuestionBytes = 16'000;
        ChatView(Conversation, ViewHost);
        void Build(folia::ThemeProfile const&, MediaDisplaySettings const&);
        void RefreshAccessLabel();
        void SelectModel(int index);
        void RefreshProfiles();
        void UpdateUsage();
        void ArrangeComposer();
        void Send();
        void StartTurn(bool retry);
        fire_and_forget CompactContext();
        void Command(std::string const&);
        fire_and_forget PickImages();
        void SetBusy(bool);
        void KeepPreview();
        void SaveOptions();
        void Report(hstring const&);
        fire_and_forget PickFolder();
        void SetFolder(std::filesystem::path);
        static fire_and_forget GenerateTitle(std::weak_ptr<ChatView>, std::string, ProviderConnection, ConfiguredModel,
            std::string, Microsoft::UI::Dispatching::DispatcherQueue);
        static fire_and_forget Execute(std::weak_ptr<ChatView>, Conversation, ProviderConnection, ConfiguredModel,
            std::optional<std::vector<OpenTab>>, bool retry, std::stop_token, Microsoft::UI::Dispatching::DispatcherQueue);

        Conversation conversation_;
        std::optional<std::uint64_t> usedTokens_;
        std::optional<std::uint64_t> cachedTokens_;
        std::string usageModel_;
        ViewHost host_;
        ChatPalette palette_;
        struct Route {
            ConfiguredModel model;
            bool hostedSearch = false;
            folia::ai::PageAccess pages = folia::ai::PageAccess::None;
        };
        std::vector<Route> routes_;
        std::stop_source cancellation_;
        bool busy_ = false, composing_ = false;
        std::shared_ptr<ChatTranscript> transcript_;
        std::shared_ptr<ChatComposerExtras> extras_;
        Microsoft::UI::Xaml::Controls::Grid root_, bar_;
        Microsoft::UI::Xaml::Controls::StackPanel composer_;
        std::unique_ptr<ChatModelPicker> modelPicker_;
        Microsoft::UI::Xaml::Controls::TextBox input_;
        Microsoft::UI::Xaml::Controls::StackPanel contextMeter_;
        Microsoft::UI::Xaml::Controls::ProgressRing contextRing_;
        Microsoft::UI::Xaml::Controls::TextBlock status_, empty_, accessLabel_, usage_;
        Microsoft::UI::Xaml::Controls::Button accessChip_, send_, stop_, mention_, split_;
        Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem openTabs_, web_, readPages_;
    };
}
