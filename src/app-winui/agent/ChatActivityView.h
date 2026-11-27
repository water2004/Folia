#pragma once

namespace winrt::Folia::chat {
    // One disclosure covers an uninterrupted run of activity between Markdown passages.
    class ChatActivityView : public std::enable_shared_from_this<ChatActivityView> {
    public:
        static std::shared_ptr<ChatActivityView> Create();
        Microsoft::UI::Xaml::Controls::StackPanel Element() const { return root_; }
        void StartStep() { currentReason_.reset(); }
        void Reason(hstring const&);
        void Summary(hstring const&);
        std::size_t StartTool(std::string const&);
        void UpdateTool(std::size_t index, std::string const& description, bool done, bool failed, hstring const& detail);
        void EndTool(std::string const&, bool failed, hstring const&);
        void Status(hstring const& live, hstring const& finished);
        void Complete();
    private:
        enum class Kind { Reasoning, Tool, Status };
        struct Item {
            hstring title, finished, detail;
            Kind kind;
            std::string action;
            bool failed = false, expanded = false;
            Microsoft::UI::Xaml::Controls::TextBlock caption{nullptr}, body{nullptr};
        };
        hstring Caption() const;
        void Refresh();
        void Toggle(std::optional<std::size_t>);
        Microsoft::UI::Xaml::Controls::Button Header(std::optional<std::size_t>, hstring const&, bool);
        Microsoft::UI::Xaml::Controls::StackPanel root_, details_;
        std::vector<Item> items_;
        std::optional<std::size_t> currentReason_, currentTool_;
        Microsoft::UI::Xaml::Controls::TextBlock summary_{nullptr};
        Microsoft::UI::Xaml::Controls::Button summaryButton_{nullptr};
        bool expanded_ = false, complete_ = false;
    };
}
