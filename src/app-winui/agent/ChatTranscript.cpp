#include "pch.h"
#include "agent/ChatMotion.h"
#include "agent/ChatTranscript.h"
#include "agent/ChatControls.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Storage.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using folia::ai::MessageRole;
    using folia::ai::agent::ProgressKind;
    namespace {
        void SmallButton(Button const& button, Windows::Foundation::IInspectable const& content, wchar_t const* name) {
            button.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
            button.Content(content);
            Automation::AutomationProperties::SetName(button, Localize(name));
            ToolTipService::SetToolTip(button, box_value(Localize(name)));
        }
        TextBlock Text(hstring const& value, bool muted = false) {
            TextBlock text;
            text.Style(settings_ui::ResourceStyle(muted ? L"ChatModelPickerSecondaryLabelStyle" : L"ChatModelPickerLabelStyle"));
            text.FontSize(muted ? 12 : 15);
            text.TextWrapping(TextWrapping::Wrap);
            text.TextTrimming(TextTrimming::None);
            text.IsTextSelectionEnabled(true);
            text.Text(value);
            return text;
        }
        void Copy(hstring const& value) {
            using namespace Windows::ApplicationModel::DataTransfer;
            DataPackage package;
            package.SetText(value);
            Clipboard::SetContent(package);
        }
        fire_and_forget Thumbnail(Image image, std::filesystem::path path) {
            try {
                auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(path.native());
                auto stream = co_await file.OpenReadAsync();
                Media::Imaging::BitmapImage bitmap;
                bitmap.DecodePixelWidth(320);
                co_await bitmap.SetSourceAsync(stream);
                image.Source(bitmap);
            } catch (hresult_error const&) {}
        }
    }

    std::shared_ptr<ChatTranscript> ChatTranscript::Create(std::string id, folia::ThemeProfile const& theme) {
        auto view = std::make_shared<ChatTranscript>();
        view->id_ = std::move(id);
        view->theme_ = theme;
        view->Build();
        view->Refresh(true);
        return view;
    }

    void ChatTranscript::Build() {
        auto weak = weak_from_this();
        Rows(root_, {false,true});
        Columns(header_, {true,false,false,false,false});
        header_.MinHeight(32);
        header_.Padding({12,0,8,0});
        header_.ColumnSpacing(6);
        title_.Style(settings_ui::ResourceStyle(L"ChatModelPickerLabelStyle"));
        title_.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        Put(header_, title_, 0);
        Put(header_, nav_, 0, 1);
        Put(root_, header_, 0);
        nav_.Orientation(Orientation::Horizontal);
        nav_.HorizontalAlignment(HorizontalAlignment::Center);
        nav_.Spacing(6);
        SmallButton(older_, MakeWorkspaceIcon(WorkspaceIcon::ChevronLeft), L"ChatOlderMessages");
        SmallButton(newer_, MakeWorkspaceIcon(WorkspaceIcon::ChevronRight), L"ChatNewerMessages");
        pageLabel_.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
        for (auto element : {older_.as<UIElement>(), pageLabel_.as<UIElement>(), newer_.as<UIElement>()}) nav_.Children().Append(element);
        older_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock(); view && view->page_.before) {
                view->boundaries_.push_back(view->page_.before);
                view->Show(true);
            }
        });
        newer_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock(); view && !view->boundaries_.empty()) {
                view->boundaries_.pop_back();
                view->Show(true);
            }
        });
        scroll_.Name(L"ConversationScroll");
        scroll_.HorizontalScrollMode(ScrollMode::Disabled);
        scroll_.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll_.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll_.HorizontalContentAlignment(HorizontalAlignment::Left);
        scroll_.ZoomMode(ZoomMode::Disabled);
        column_.MaxWidth(768);
        column_.Padding({18, 18, 18, 140});
        column_.Spacing(10);
        messages_.Spacing(16);
        live_.Spacing(4);
        live_.Visibility(Visibility::Collapsed);
        column_.Children().Append(messages_);
        column_.Children().Append(live_);
        working_ = Text({}, true);
        column_.Children().Append(working_);
        // Give the scroll presenter a viewport-wide content root. Otherwise its
        // alignment offset can depend on the intrinsic width of an earlier page.
        Grid page;
        page.HorizontalAlignment(HorizontalAlignment::Left);
        column_.HorizontalAlignment(HorizontalAlignment::Center);
        page.Children().Append(column_);
        scroll_.Content(page);
        scroll_.SizeChanged([page, column = column_](auto const&, SizeChangedEventArgs const& args) {
            auto width = args.NewSize().Width;
            page.Width(width);
            column.Width((std::min)(static_cast<double>(width), column.MaxWidth()));
        });
        Put(root_, scroll_, 1);
        SmallButton(latest_, MakeWorkspaceIcon(WorkspaceIcon::ArrowDown), L"ChatLatest");
        latest_.Style(settings_ui::ResourceStyle(L"DefaultButtonStyle"));
        latest_.Width(32); latest_.Height(32); latest_.Padding({0,0,0,0}); latest_.CornerRadius({16,16,16,16});
        latest_.VerticalAlignment(VerticalAlignment::Bottom);
        latest_.HorizontalAlignment(HorizontalAlignment::Center);
        latest_.Visibility(Visibility::Collapsed);
        Put(root_, latest_, 1);
        latest_.Click([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->Latest(); });
        scroll_.ViewChanging([weak](auto const&, ScrollViewerViewChangingEventArgs const& args) {
            if (auto view = weak.lock(); view && args.NextView().VerticalOffset() + 1 < view->scroll_.VerticalOffset())
                view->following_ = false;
        });
        scroll_.ViewChanged([weak](auto const&, auto const&) {
            if (auto view = weak.lock()) {
                auto y = view->scroll_.VerticalOffset();
                if (y + 1 < view->offset_) view->following_ = false;
                if (y >= view->scroll_.ScrollableHeight() - 2 && view->boundaries_.empty()) view->following_ = true;
                view->offset_ = y;
                view->UpdateNavigation();
            }
        });
        column_.SizeChanged([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->Follow(); });
    }

    void ChatTranscript::UpdateNavigation() {
        nav_.Visibility(page_.before || !boundaries_.empty() ? Visibility::Visible : Visibility::Collapsed);
        older_.IsEnabled(page_.before != 0);
        newer_.IsEnabled(!boundaries_.empty());
        pageLabel_.Text(boundaries_.empty() ? Localize(L"ChatRecent") :
            LocalizeFormat(L"ChatEarlierPage", {to_hstring(boundaries_.size())}));
        const bool show = !boundaries_.empty() || (!following_ && scroll_.ScrollableHeight() - scroll_.VerticalOffset() > 32);
        const bool appearing = show && latest_.Visibility() == Visibility::Collapsed;
        latest_.Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        if (appearing) Reveal(latest_, 0, 5);
    }

    void ChatTranscript::Follow() {
        if (posted_ || !following_ || !boundaries_.empty()) return;
        posted_ = true;
        root_.DispatcherQueue().TryEnqueue([weak = weak_from_this()] {
            if (auto view = weak.lock()) {
                view->posted_ = false;
                if (view->following_ && view->boundaries_.empty())
                    view->scroll_.ChangeView(nullptr, view->scroll_.ScrollableHeight(), nullptr, true);
            }
        });
    }

    void ChatTranscript::Latest() {
        following_ = true;
        if (!boundaries_.empty()) { boundaries_.clear(); Show(false); }
        Follow();
        UpdateNavigation();
    }
    void ChatTranscript::SetTheme(folia::ThemeProfile const& theme) {
        theme_ = theme;
        if (liveTurn_) liveTurn_->SetTheme(theme);
        for (auto const& turn : turns_) turn->SetTheme(theme);
    }
    void ChatTranscript::SetInset(double inset) {
        auto padding = column_.Padding();
        padding.Bottom = inset + 16;
        column_.Padding(padding);
        latest_.Margin({0, 0, 0, inset + 3});
    }
    void ChatTranscript::Refresh(bool latest) {
        if (latest) { boundaries_.clear(); following_ = true; }
        if (boundaries_.empty()) Show(false);
    }
    Entry const* ChatTranscript::LastReply() const {
        auto it = std::ranges::find(page_.entries | std::views::reverse, MessageRole::Assistant, &Entry::role);
        return it == page_.entries.rend() ? nullptr : &*it;
    }

    Entry const* ChatTranscript::LastQuestion() const {
        auto it = std::ranges::find(page_.entries | std::views::reverse, MessageRole::User, &Entry::role);
        return it == page_.entries.rend() ? nullptr : &*it;
    }

    void ChatTranscript::Show(bool navigate) {
        auto end = boundaries_.empty() ? std::optional<std::uintmax_t>{} : boundaries_.back();
        auto page = ReadHistoryPage(id_, end);
        bool append = !navigate && page.compactions == page_.compactions && page.before == page_.before && page_.entries.size() <= page.entries.size() &&
            std::equal(page_.entries.begin(), page_.entries.end(), page.entries.begin(), [](auto const& a, auto const& b) {
                return a.role == b.role && a.markdown == b.markdown && a.images == b.images;
            });
        auto start = append ? page_.entries.size() : 0;
        if (retryButton_ && (!append || start < page.entries.size())) {
            retryButton_.Visibility(Visibility::Collapsed);
            retryButton_ = nullptr;
        }
        if (!append) { messages_.Children().Clear(); turns_.clear(); }
        page_ = std::move(page);
        for (auto i = start; i <= page_.entries.size(); ++i) {
            if ((!append || i > start) && std::ranges::contains(page_.compactions, i))
                messages_.Children().Append(Text(Localize(L"ChatCompacted"), true));
            if (i < page_.entries.size()) Append(page_.entries[i], boundaries_.empty() && i + 1 == page_.entries.size());
        }
        live_.Visibility(busy_ && boundaries_.empty() ? Visibility::Visible : Visibility::Collapsed);
        working_.Visibility(live_.Visibility());
        if (navigate) {
            following_ = false;
            scroll_.ChangeView(nullptr, 0., nullptr, true);
        } else Follow();
        UpdateNavigation();
    }

    void ChatTranscript::Append(Entry const& entry, bool latest) {
        StackPanel row;
        row.Spacing(5);
        if (entry.role == MessageRole::User) {
            Border bubble;
            bubble.Style(settings_ui::ResourceStyle(L"ChatUserBubbleStyle"));
            bubble.MaxWidth(570);
            bubble.Margin({28, 0, 0, 0});
            StackPanel body;
            body.Spacing(5);
            for (auto const& id : entry.images) {
                Image image;
                image.MaxWidth(220); image.Height(104);
                image.Stretch(Media::Stretch::Uniform);
                body.Children().Append(image);
                Thumbnail(image, AttachmentPath(id_, id));
            }
            if (!entry.markdown.empty()) body.Children().Append(Text(to_hstring(entry.markdown)));
            bubble.Child(body);
            row.Children().Append(bubble);
        } else {
            auto turn = latest && finishing_ && liveTurn_ ? liveTurn_ : MakeTurn();
            turn->Finish(entry);
            row.Children().Append(turn->Element());
            turns_.push_back(std::move(turn));
            if (entry.interrupted || entry.outputLimit || !entry.error.empty()) {
                auto message = entry.outputLimit ? Localize(L"ChatOutputLimit") :
                    entry.error.empty() ? Localize(L"ChatInterrupted") : to_hstring(entry.error);
                row.Children().Append(Text(message, true));
                if (latest && !entry.outputLimit) {
                    Button again;
                    SmallButton(again, box_value(Localize(L"ChatRetry")), L"ChatRetry");
                    again.HorizontalAlignment(HorizontalAlignment::Left);
                    again.Click([weak = weak_from_this()](auto const&, auto const&) {
                        if (auto view = weak.lock(); view && view->retry) view->retry();
                    });
                    row.Children().Append(again);
                    retryButton_ = again;
                }
            }
            if (!entry.sources.empty()) {
                MenuFlyout menu;
                for (auto const& source : entry.sources) {
                    if (!source.url.starts_with("https://") && !source.url.starts_with("http://")) continue;
                    MenuFlyoutItem item;
                    item.Text(to_hstring(source.title.empty() ? source.url : source.title));
                    item.Click([url = to_hstring(source.url)](auto const&, auto const&) -> fire_and_forget {
                        try { co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri(url)); }
                        catch (hresult_error const&) {}
                    });
                    menu.Items().Append(item);
                }
                if (menu.Items().Size()) {
                    Button sources;
                    SmallButton(sources, box_value(Localize(L"ChatSources")), L"ChatSources");
                    sources.Flyout(menu);
                    row.Children().Append(sources);
                }
            }
        }
        MenuFlyout menu;
        MenuFlyoutItem copy;
        copy.Text(Localize(L"Copy"));
        copy.Click([text = to_hstring(entry.markdown)](auto const&, auto const&) { Copy(text); });
        menu.Items().Append(copy);
        row.ContextFlyout(menu);
        messages_.Children().Append(row);
    }

    std::shared_ptr<ChatTurnView> ChatTranscript::MakeTurn() {
        return std::make_shared<ChatTurnView>(theme_, [weak = weak_from_this()](auto const& link, auto const& documents) {
            if (auto self = weak.lock(); self && self->openDocument) self->openDocument(link, documents);
        });
    }
    void ChatTranscript::Begin() {
        busy_ = true;
        if (retryButton_) retryButton_.IsEnabled(false);
        live_.Children().Clear();
        liveTurn_ = MakeTurn();
        live_.Children().Append(liveTurn_->Element());
        live_.Visibility(Visibility::Visible);
        working_.Text(Localize(L"ChatWorking"));
        working_.Visibility(Visibility::Visible);
        Follow();
    }
    void ChatTranscript::Progress(ProgressKind kind, hstring const& value) {
        if (!busy_ || !liveTurn_) return;
        liveTurn_->Progress(kind, value);
        if (liveTurn_->Element().Children().Size()) working_.Visibility(Visibility::Collapsed);
        Follow();
    }
    void ChatTranscript::Finish() {
        busy_ = false;
        finishing_ = true;
        live_.Children().Clear();
        live_.Visibility(Visibility::Collapsed);
        working_.Visibility(Visibility::Collapsed);
        Refresh(false);
        finishing_ = false;
        liveTurn_.reset();
    }
}
