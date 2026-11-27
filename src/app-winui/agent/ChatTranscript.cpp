#include "pch.h"
#include "agent/ChatTranscript.h"
#include "agent/ChatControls.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Storage.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using folia::ai::MessageRole;
    using folia::ai::agent::ProgressKind;
    namespace {
        void SmallButton(Button const& button, hstring const& content, wchar_t const* name) {
            button.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
            button.Content(box_value(content));
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
        StackPanel Disclosure(hstring const& label, TextBlock const& body) {
            StackPanel panel;
            Button toggle;
            SmallButton(toggle, label, L"ChatThinking");
            Automation::AutomationProperties::SetName(toggle, label);
            ToolTipService::SetToolTip(toggle, box_value(label));
            toggle.HorizontalAlignment(HorizontalAlignment::Left);
            toggle.Padding({3, 0, 6, 0});
            body.Margin({12, 1, 4, 6});
            body.Visibility(Visibility::Collapsed);
            StackPanel header;
            header.Orientation(Orientation::Horizontal);
            header.Spacing(5);
            auto title = Text(label, true);
            title.IsTextSelectionEnabled(false);
            header.Children().Append(title);
            FontIcon arrow;
            arrow.FontSize(9);
            arrow.Opacity(0.7);
            arrow.Glyph(L"\uE76C");
            header.Children().Append(arrow);
            toggle.Height(25);
            toggle.Content(header);
            toggle.Click([body, arrow](auto const&, auto const&) {
                bool expanded = body.Visibility() == Visibility::Collapsed;
                body.Visibility(expanded ? Visibility::Visible : Visibility::Collapsed);
                arrow.Glyph(expanded ? L"\uE70D" : L"\uE76C");
            });
            panel.Children().Append(toggle);
            panel.Children().Append(body);
            return panel;
        }
        hstring ToolLabel(std::string_view description) {
            bool failed = description.starts_with("! ");
            if (failed) description.remove_prefix(2);
            auto split = description.find(' ');
            auto name = description.substr(0, split);
            auto target = split == description.npos ? std::string_view{} : description.substr(split + 1);
            constexpr std::pair<std::string_view, wchar_t const*> labels[]{
                {"list_open_tabs", L"ChatToolTabs"}, {"read_open_tab", L"ChatToolReadTab"},
                {"outline_open_tab", L"ChatToolOutline"}, {"search_open_tab", L"ChatToolSearchTab"},
                {"list_workspace", L"ChatToolDirectory"}, {"read_workspace_file", L"ChatToolReadFile"},
                {"web_search", L"ChatToolWebSearch"}, {"search_workspace_file", L"ChatToolFindFile"}};
            auto found = std::ranges::find(labels, name, &std::pair<std::string_view, wchar_t const*>::first);
            auto title = found == std::end(labels) ? to_hstring(name) : Localize(found->second);
            if (!target.empty()) title = title + L"  " + to_hstring(target);
            if (failed) title = title + L" · " + Localize(L"ChatToolFailed");
            return title;
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
        Rows(root_, {false, true});
        nav_.Orientation(Orientation::Horizontal);
        nav_.HorizontalAlignment(HorizontalAlignment::Center);
        nav_.Spacing(6);
        SmallButton(older_, L"‹", L"ChatOlderMessages");
        SmallButton(newer_, L"›", L"ChatNewerMessages");
        pageLabel_.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
        for (auto element : {older_.as<UIElement>(), pageLabel_.as<UIElement>(), newer_.as<UIElement>()}) nav_.Children().Append(element);
        Put(root_, nav_, 0);
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
        SmallButton(latest_, L"↓", L"ChatLatest");
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
        latest_.Visibility(!boundaries_.empty() || (!following_ && scroll_.ScrollableHeight() - scroll_.VerticalOffset() > 32)
            ? Visibility::Visible : Visibility::Collapsed);
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
        for (auto& surface : surfaces_) surface->SetTheme(theme);
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

    void ChatTranscript::Show(bool navigate) {
        auto end = boundaries_.empty() ? std::optional<std::uintmax_t>{} : boundaries_.back();
        auto page = ReadHistoryPage(id_, end);
        bool append = !navigate && page.before == page_.before && page_.entries.size() <= page.entries.size() &&
            std::equal(page_.entries.begin(), page_.entries.end(), page.entries.begin(), [](auto const& a, auto const& b) {
                return a.role == b.role && a.markdown == b.markdown && a.images == b.images;
            });
        auto start = append ? page_.entries.size() : 0;
        if (!append) { messages_.Children().Clear(); surfaces_.clear(); }
        page_ = std::move(page);
        for (auto const& entry : std::span(page_.entries).subspan(start)) Append(entry);
        live_.Visibility(busy_ && boundaries_.empty() ? Visibility::Visible : Visibility::Collapsed);
        working_.Visibility(live_.Visibility());
        if (navigate) {
            following_ = false;
            scroll_.ChangeView(nullptr, 0., nullptr, true);
        } else Follow();
        UpdateNavigation();
    }

    void ChatTranscript::Append(Entry const& entry) {
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
            folia::ai::agent::StepEnd start;
            auto render = [&](folia::ai::agent::StepEnd end) {
                auto reason = std::string_view(entry.reasoning).substr(start.reasoning, end.reasoning - start.reasoning);
                if (!reason.empty()) row.Children().Append(Disclosure(Localize(L"ChatThinking"), Text(to_hstring(reason), true)));
                auto markdown = std::string_view(entry.markdown).substr(start.text, end.text - start.text);
                if (!markdown.empty()) {
                    auto surface = ChatReplySurface::Create(std::string(markdown), theme_);
                    row.Children().Append(surface->Element());
                    surfaces_.push_back(std::move(surface));
                }
                for (auto const& tool : std::span(entry.activities).subspan(start.tools, end.tools - start.tools))
                    row.Children().Append(Disclosure(ToolLabel((tool.failed ? "! " : "") + tool.name + " " + tool.target), Text(to_hstring(tool.detail), true)));
                start = end;
            };
            for (auto end : entry.steps) render(end);
            render({entry.markdown.size(), entry.reasoning.size(), entry.activities.size()});
            if (entry.interrupted) row.Children().Append(Text(Localize(L"ChatInterrupted"), true));
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
                    SmallButton(sources, Localize(L"ChatSources"), L"ChatSources");
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

    void ChatTranscript::Begin() {
        busy_ = true;
        live_.Children().Clear();
        live_.Visibility(Visibility::Visible);
        working_.Text(Localize(L"ChatWorking"));
        working_.Visibility(Visibility::Visible);
        Follow();
    }
    void ChatTranscript::Progress(ProgressKind kind, hstring const& value) {
        if (!busy_) return;
        if (kind == ProgressKind::StepStarted) {
            thinking_ = nullptr; thinkingRow_ = nullptr; liveText_ = nullptr; liveTool_ = nullptr;
            working_.Text(Localize(L"ChatWorking"));
        } else if (kind == ProgressKind::Reading) {
            liveTool_ = StackPanel();
            liveTool_.Children().Append(Text(ToolLabel(to_string(value)), true));
            live_.Children().Append(liveTool_);
        } else if (kind == ProgressKind::ToolResult) {
            if (liveTool_) {
                auto item = folia::ai::Json::parse(to_string(value));
                auto title = ToolLabel((item.value("failed", false) ? "! " : "") + item.value("action", ""));
                liveTool_.Children().Clear();
                liveTool_.Children().Append(Disclosure(title, Text(to_hstring(item.value("detail", "")), true)));
            }
        } else if (kind == ProgressKind::Compacting) working_.Text(Localize(L"ChatCompacting"));
        else if (kind == ProgressKind::ReasoningSnapshot) {
            if (!thinking_) {
                thinking_ = Text({}, true);
                thinkingRow_ = Disclosure(Localize(L"ChatThinking"), thinking_);
                live_.Children().Append(thinkingRow_);
            }
            thinking_.Text(value);
        } else if (kind == ProgressKind::TextSnapshot) {
            if (!liveText_) { liveText_ = Text({}); live_.Children().Append(liveText_); }
            liveText_.Text(value);
            working_.Text({});
        }
        Follow();
    }
    void ChatTranscript::Finish() {
        busy_ = false;
        live_.Children().Clear();
        live_.Visibility(Visibility::Collapsed);
        working_.Visibility(Visibility::Collapsed);
        thinking_ = nullptr; thinkingRow_ = nullptr; liveText_ = nullptr; liveTool_ = nullptr;
        Refresh(false);
    }
}
