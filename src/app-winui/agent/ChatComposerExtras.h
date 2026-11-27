#pragma once
#include "agent/ChatTools.h"

namespace winrt::Folia::chat {
    struct DraftImage {
        std::vector<std::uint8_t> png;
    };
    class ChatComposerExtras : public std::enable_shared_from_this<ChatComposerExtras> {
    public:
        using TabList = std::function<std::vector<OpenTab>()>;
        static std::shared_ptr<ChatComposerExtras> Create(Microsoft::UI::Xaml::Controls::TextBox, TabList,
            std::function<std::filesystem::path()>, std::function<void(hstring const&)>);
        ~ChatComposerExtras() { lookup_.request_stop(); }
        Microsoft::UI::Xaml::Controls::StackPanel Element() const { return panel_; }
        bool HandleKey(Windows::System::VirtualKey);
        bool Mentions(OpenTab const&) const;
        bool Pasting() const { return pasting_; }
        void Clear();
        void BeginMention();
        fire_and_forget PasteImages(Windows::ApplicationModel::DataTransfer::DataPackageView);
        void SetVision(bool);
        void SetSending(bool value) { sending_ = value; }
        std::span<DraftImage const> Images() const { return images_; }
    private:
        void Update();
        void Accept(int index);
        void PaintChoices(bool keepSelection = false);
        void PaintImages();
        fire_and_forget FindFiles(std::filesystem::path, std::string, std::stop_token);
        struct Choice { std::string title; std::uint64_t tab = 0; };
        Microsoft::UI::Xaml::Controls::TextBox input_{nullptr};
        Microsoft::UI::Xaml::Controls::StackPanel panel_, thumbnails_;
        Microsoft::UI::Xaml::Controls::ListView list_;
        TabList tabs_;
        std::function<std::filesystem::path()> folder_;
        std::function<void(hstring const&)> report_;
        std::vector<DraftImage> images_;
        std::vector<Microsoft::UI::Xaml::Media::Imaging::BitmapImage> previews_;
        std::vector<Choice> choices_, mentions_;
        std::stop_source lookup_;
        int tokenStart_ = -1, caret_ = -1;
        bool accepting_ = false, vision_ = false, sending_ = false, pasting_ = false;
    };
}
