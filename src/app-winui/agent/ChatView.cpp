#include "pch.h"
#include "agent/ChatView.h"
#include "providers/HostedSearch.h"
#include "agent/ChatControls.h"
#include "workspace/WorkspaceIcons.h"
#include <shobjidl.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <wil/resource.h>

namespace winrt::Folia::chat {
    using namespace models;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {
        void Label(Button const& button, wchar_t const* key) {
            auto name = Localize(key);
            Automation::AutomationProperties::SetName(button, name);
            ToolTipService::SetToolTip(button, box_value(name));
        }

    }

    ChatView::ChatView(Conversation conversation, ViewHost host)
        : conversation_(std::move(conversation)), host_(std::move(host)) {}

    ChatView::~ChatView() {
        cancellation_.request_stop();
    }

    std::shared_ptr<ChatView> ChatView::Create(Conversation conversation, ViewHost host, folia::ThemeProfile const& theme) {
        auto view = std::shared_ptr<ChatView>(new ChatView(std::move(conversation), std::move(host)));
        view->SetTheme(theme);
        view->Build();
        return view;
    }

    void ChatView::SetTheme(folia::ThemeProfile const& theme) {
        theme_ = theme;
        palette_.Apply(theme.colors);
        if (transcript_) transcript_->SetTheme(theme);
    }

    void ChatView::Build() {
        const auto weak = weak_from_this();
        auto click = [weak](auto member) {
            return [weak, member](auto const&, auto const&) { if (auto view = weak.lock()) ((*view).*member)(); };
        };
        transcript_ = ChatTranscript::Create(conversation_.id, theme_);
        transcript_->SetTitle(to_hstring(conversation_.title));
        transcript_->openDocument = [weak](auto const& link, auto const& documents) {
            if (auto view = weak.lock(); view && view->host_.openDocument) view->host_.openDocument(link, documents);
        };
        transcript_->retry = [weak] { if (auto self = weak.lock()) self->StartTurn(true); };
        Put(root_, transcript_->Element(), 0);
        if (auto last = transcript_->LastReply(); last && !transcript_->EndsWithCompaction()) {
            usageModel_ = last->modelId;
            usedTokens_ = last->usage.inputTokens;
        }
        composer_.Spacing(4);
        composer_.MaxWidth(760);
        composer_.Margin({12, 0, 12, 10});
        empty_.Text(Localize(L"ChatEmpty"));
        empty_.FontSize(20);
        empty_.HorizontalAlignment(HorizontalAlignment::Center);
        empty_.Margin({0, 0, 0, 14});
        composer_.Children().Append(empty_);
        status_.TextWrapping(TextWrapping::Wrap);
        status_.Style(settings_ui::ResourceStyle(L"SettingsHintTextStyle"));
        Report({});
        composer_.Children().Append(status_);
        Border card;
        card.Style(settings_ui::ResourceStyle(L"ChatComposerCardStyle"));
        StackPanel editor;
        editor.Spacing(3);
        input_.Style(settings_ui::ResourceStyle(L"ChatComposerInputStyle"));
        auto clear = Microsoft::UI::Xaml::Media::SolidColorBrush(Microsoft::UI::Colors::Transparent());
        for (auto key : {L"TextControlBackground", L"TextControlBackgroundPointerOver", L"TextControlBackgroundFocused",
                         L"TextControlBorderBrush", L"TextControlBorderBrushPointerOver", L"TextControlBorderBrushFocused"})
            input_.Resources().Insert(box_value(hstring(key)), clear);
        input_.AcceptsReturn(true);
        input_.TextWrapping(TextWrapping::Wrap);
        input_.MinHeight(42);
        input_.MaxHeight(162);
        input_.PlaceholderText(Localize(L"ChatPrompt"));
        input_.MaxLength(maxQuestionBytes);
        extras_ = ChatComposerExtras::Create(input_, [weak] {
            if (auto view = weak.lock()) return view->host_.tabs();
            return std::vector<OpenTab>{};
        },
            [weak] { if (auto view = weak.lock()) return view->conversation_.folder; return std::filesystem::path{}; },
            [weak](hstring const& message) { if (auto view = weak.lock()) view->Report(message); });
        input_.TextChanging([weak](auto const&, auto const&) {
            if (auto view = weak.lock(); view && view->host_.keep && !view->input_.Text().empty()) view->KeepPreview();
        });
        extras_->imageAdded = [weak] { if (auto view = weak.lock()) view->KeepPreview(); };
        extras_->command = [weak](auto const& name) { if (auto self = weak.lock()) self->Command(name); };
        editor.Children().Append(extras_->Element());
        editor.Children().Append(input_);
        input_.TextCompositionStarted([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->composing_ = true; });
        input_.TextCompositionEnded([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->composing_ = false; });
        input_.PreviewKeyDown([weak](auto const&, Input::KeyRoutedEventArgs const& args) {
            auto view = weak.lock();
            if (!view || view->composing_) return;
            using Windows::System::VirtualKey;
            auto shift = Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Shift);
            using Windows::UI::Core::CoreVirtualKeyStates;
            if (static_cast<unsigned>(shift) & static_cast<unsigned>(CoreVirtualKeyStates::Down)) return;
            if (view->extras_->HandleKey(args.Key())) { args.Handled(true); return; }
            if (args.Key() == VirtualKey::Enter) {
                args.Handled(true);
                view->Send();
            }
        });

        Columns(bar_, {false, false, true, false, false});
        bar_.ColumnSpacing(3);
        mention_.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
        mention_.Content(MakeWorkspaceIcon(WorkspaceIcon::Add));
        Label(mention_, L"ChatAddContent");
        MenuFlyout attachments;
        attachments.Placement(Primitives::FlyoutPlacementMode::TopEdgeAlignedLeft);
        MenuFlyoutItem mention, image;
        mention.Icon(SymbolIcon(Symbol::Document));
        image.Icon(SymbolIcon(Symbol::Pictures));
        mention.Text(Localize(L"ChatMention")); mention.KeyboardAcceleratorTextOverride(L"@");
        image.Text(Localize(L"ChatAddImage"));
        mention.Click([weak](auto const&, auto const&) { if (auto self = weak.lock()) self->extras_->BeginMention(); });
        image.Click([weak](auto const&, auto const&) { if (auto self = weak.lock()) self->PickImages(); });
        attachments.Items().Append(mention); attachments.Items().Append(image);
        attachments.Opening([weak, image](auto const&, auto const&) {
            if (auto self = weak.lock()) {
                auto i = self->modelPicker_->SelectedIndex();
                image.IsEnabled(i >= 0 && self->routes_.at(i).model.vision);
            }
        });
        mention_.Flyout(attachments);
        Put(bar_, mention_, 0);
        accessChip_.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
        accessLabel_.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
        accessLabel_.FontSize(12);
        accessLabel_.TextTrimming(TextTrimming::CharacterEllipsis);
        accessChip_.Content(accessLabel_);
        Label(accessChip_, L"ChatAccess");
        Put(bar_, accessChip_, 0, 1);
        usage_.Name(L"ChatContextUsage");
        usage_.Style(settings_ui::ResourceStyle(L"ChatModelPickerSecondaryLabelStyle"));
        usage_.FontSize(11);
        usage_.Margin({2, 0, 4, 0});
        contextMeter_.Orientation(Orientation::Horizontal);
        contextMeter_.HorizontalAlignment(HorizontalAlignment::Right);
        contextMeter_.VerticalAlignment(VerticalAlignment::Center);
        contextRing_.IsIndeterminate(false);
        contextRing_.Width(16); contextRing_.Height(16);
        contextRing_.MinWidth(0); contextRing_.MinHeight(0);
        Grid meter;
        Border track;
        track.Style(settings_ui::ResourceStyle(L"ChatContextTrackStyle"));
        Put(meter, track, 0);
        Put(meter, contextRing_, 0);
        contextMeter_.Children().Append(meter);
        contextMeter_.Children().Append(usage_);
        Put(transcript_->Header(), contextMeter_, 0, 2);
        if (host_.rename) {
            Button rename;
            rename.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
            FontIcon pencil;
            pencil.Glyph(L"\uE70F"); pencil.FontSize(13);
            rename.Content(pencil);
            Label(rename, L"ChatRename");
            rename.Click([weak](auto const&, auto const&) { if (auto self = weak.lock()) self->host_.rename(); });
            Put(transcript_->Header(), rename, 0, 3);
        }
        modelPicker_ = std::make_unique<ChatModelPicker>(
            [weak](int index) {
                if (auto view = weak.lock(); view && !view->busy_) {
                    view->SelectModel(index);
                    view->SaveOptions();
                }
            },
            [weak](std::string level) {
                if (auto view = weak.lock(); view && !view->busy_) {
                    view->conversation_.effort = std::move(level);
                    view->SaveOptions();
                }
            },
            [weak] { if (auto view = weak.lock()) view->host_.settings(); });
        modelPicker_->SetPalette(palette_);
        Put(bar_, modelPicker_->Element(), 0, 3);
        split_.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
        auto splitIcon = MakeWorkspaceIcon(WorkspaceIcon::SplitEditor);
        splitIcon.Width(15); splitIcon.Height(15);
        split_.Content(splitIcon);
        Label(split_, L"SplitEditor");
        split_.Visibility(host_.split ? Visibility::Visible : Visibility::Collapsed);
        split_.Click([weak](auto const&, auto const&) { if (auto self = weak.lock(); self && self->host_.split) self->host_.split(); });
        Put(transcript_->Header(), split_, 0, 4);
        openTabs_.Text(Localize(L"ChatAllowOpenTabs"));
        for (auto const& [button, icon, key] : {std::tuple{stop_, WorkspaceIcon::Stop, L"ChatStop"},
            std::tuple{send_, WorkspaceIcon::Send, L"ChatSend"}}) {
            button.Style(settings_ui::ResourceStyle(L"ChatSendStyle"));
            button.Background(palette_.accent);
            button.Foreground(palette_.onAccent);
            button.Content(MakeWorkspaceIcon(icon, palette_.onAccent));
            Label(button, key);
        }
        ToolTipService::SetToolTip(send_, box_value(Localize(L"ChatSendKeys")));
        Put(bar_, stop_, 0, 4);
        Put(bar_, send_, 0, 4);
        editor.Children().Append(bar_);
        card.Child(editor);
        composer_.Children().Append(card);
        Put(root_, composer_, 0);
        composer_.SizeChanged([weak](auto const&, auto const& args) {
            if (auto view = weak.lock()) {
                view->transcript_->SetInset(args.NewSize().Height + 10);
                bool narrow = args.NewSize().Width < 450;
                view->usage_.Visibility(narrow ? Visibility::Collapsed : Visibility::Visible);
                view->accessLabel_.MaxWidth(narrow ? (std::max)(48., args.NewSize().Width * .22) : 150.);
                view->modelPicker_->Element().MaxWidth((std::max)(60., (std::min)(230., args.NewSize().Width * 0.44)));
            }
        });
        openTabs_.IsChecked(conversation_.openTabs);
        web_.IsChecked(conversation_.search);
        SetFolder(conversation_.folder);
        openTabs_.Click(click(&ChatView::SaveOptions));
        web_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock(); view && !view->busy_) {
                view->conversation_.search = view->web_.IsChecked();
                view->SaveOptions();
            }
        });
        readPages_.Text(Localize(L"ChatAllowPageRead"));
        readPages_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock(); view && !view->busy_) {
                view->conversation_.readPages = view->readPages_.IsChecked();
                view->SaveOptions();
            }
        });
        send_.Click(click(&ChatView::Send));
        stop_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock()) view->cancellation_.request_stop();
        });
        RefreshProfiles();
        RefreshAccessLabel();
        SetBusy(false);
        ArrangeComposer();
    }

    void ChatView::SetTitle(std::string title) {
        conversation_.title = std::move(title);
        transcript_->SetTitle(to_hstring(conversation_.title));
    }

    void ChatView::Activate() {
        if (!busy_) RefreshProfiles();
        input_.Focus(FocusState::Programmatic);
    }

    void ChatView::RefreshProfiles() {
        auto providers = Profiles();
        routes_.clear();
        std::vector<hstring> choices;
        int active = -1;
        auto models = Models();
        std::unordered_map<std::string, std::size_t> nameCount;
        for (auto const& model : models) ++nameCount[model.Label()];
        for (auto& model : models) {
            auto profile = std::ranges::find(providers, model.provider, &ModelProfile::id);
            if (profile == providers.end()) continue;
            auto label = model.Label();
            if (nameCount.at(label) > 1) label += " · " + profile->name;
            choices.push_back(to_hstring(label));
            if (model.id == conversation_.model) active = static_cast<int>(routes_.size());
            const bool hostedSearch = HostedSearchFormat(model, *profile).has_value();
            auto pages = HostedPageAccess(model, *profile);
            routes_.push_back({std::move(model), hostedSearch, pages});
        }
        if (active < 0 && conversation_.model.empty() && !routes_.empty()) active = 0;
        modelPicker_->SetModels(choices, active);
        SelectModel(active);
    }

    void ChatView::SelectModel(int index) {
        auto route = index >= 0 ? &routes_.at(index) : nullptr;
        auto chosen = route ? &route->model : nullptr;
        if (chosen && conversation_.model != chosen->id) {
            conversation_.model = chosen->id;
            conversation_.effort.clear();
        }
        const bool hostedSearch = route && route->hostedSearch;
        auto pages = route ? route->pages : folia::ai::PageAccess::None;
        web_.Text(Localize(pages == folia::ai::PageAccess::WithSearch ? L"ChatSearchAndBrowse" :
            hostedSearch ? L"ChatAllowSearch" : L"ChatSearchUnavailable"));
        const bool independent = folia::ai::SeparatePageTool(pages);
        readPages_.Visibility(independent ? Visibility::Visible : Visibility::Collapsed);
        readPages_.IsChecked(independent && conversation_.readPages);
        readPages_.IsEnabled(independent && !busy_);
        web_.IsEnabled(hostedSearch && !busy_);
        extras_->SetVision(chosen && chosen->vision);
        web_.IsChecked(hostedSearch && conversation_.search);
        if (chosen) modelPicker_->SetReasoning(chosen->efforts, chosen->effort, conversation_.effort);
        else modelPicker_->SetReasoning({}, {}, {});
        UpdateUsage();
    }

    void ChatView::RefreshAccessLabel() {
        auto const& folder = conversation_.folder;
        accessLabel_.Text(folder.empty() ? Localize(openTabs_.IsChecked() ? L"ChatOpenTabs" : L"ChatAccess")
            : hstring(folder.filename().empty() ? folder.native() : folder.filename().native()));
    }

    void ChatView::UpdateUsage() {
        auto index = modelPicker_->SelectedIndex();
        contextMeter_.Visibility(index < 0 ? Visibility::Collapsed : Visibility::Visible);
        if (index < 0) return;
        auto const& model = routes_.at(index).model;
        auto used = usageModel_ == model.id ? usedTokens_ : std::nullopt;
        auto limit = model.maxInputTokens;
        auto compact = [](std::uint64_t n) {
            if (n < 1000) return to_hstring(n);
            return to_hstring(std::format("{:.1f}k", n / 1000.));
        };
        auto label = used ? compact(*used) : hstring(L"—");
        if (limit) label = label + L" / " + compact(*limit);
        contextRing_.Value(used && limit && *limit ? (std::min)(100., 100. * *used / *limit) : 0.);
        usage_.Text(label);
        auto hint = LocalizeFormat(L"ChatUsageHint", {used ? to_hstring(*used) : Localize(L"ChatNotReported"),
            limit ? to_hstring(*limit) : Localize(L"ChatNotConfigured")});
        ToolTipService::SetToolTip(contextMeter_, box_value(hint));
        Automation::AutomationProperties::SetName(contextMeter_, hint);
    }

    void ChatView::ArrangeComposer() {
        bool empty = transcript_->Empty();
        empty_.Visibility(empty ? Visibility::Visible : Visibility::Collapsed);
        composer_.VerticalAlignment(empty ? VerticalAlignment::Center : VerticalAlignment::Bottom);
    }

    void ChatView::KeepPreview() {
        if (auto keep = std::exchange(host_.keep, {})) keep();
    }

    void ChatView::SaveOptions() {
        if (busy_) return;
        conversation_.openTabs = openTabs_.IsChecked();
        RefreshAccessLabel();
        try { SaveConversationOptions(conversation_); KeepPreview(); }
        catch (std::exception const& error) { Report(to_hstring(error.what())); }
    }

    void ChatView::SetFolder(std::filesystem::path path) {
        if (busy_) return;
        conversation_.folder = std::move(path);
        auto const& folder = conversation_.folder;
        if (auto previous = accessChip_.Flyout().try_as<MenuFlyout>()) previous.Items().Clear();
        MenuFlyout menu;
        menu.Items().Append(openTabs_);
        MenuFlyoutSubItem workspace;
        workspace.Text(folder.empty() ? Localize(L"ChatWorkspace") : hstring(folder.native()));
        menu.Items().Append(workspace);
        menu.Items().Append(web_);
        menu.Items().Append(readPages_);
        const auto weak = weak_from_this();
        auto add = [&](wchar_t const* label, auto action) {
            MenuFlyoutItem item;
            item.Text(Localize(label));
            item.Click([weak, action](auto const&, auto const&) {
                if (auto view = weak.lock()) action(*view);
            });
            workspace.Items().Append(item);
        };
        add(L"ChatNoWorkspace", [](auto& view) { view.SetFolder({}); view.SaveOptions(); });
        add(L"ChatUseCurrentFolder", [](auto& view) {
            auto folder = view.host_.currentFolder();
            if (folder.empty()) view.Report(Localize(L"ChatNoCurrentFolder"));
            else { view.SetFolder(std::move(folder)); view.SaveOptions(); }
        });
        add(L"ChatChooseFolder", [](auto& view) { view.PickFolder(); });
        accessChip_.Flyout(menu);
        RefreshAccessLabel();
    }

    fire_and_forget ChatView::PickFolder() {
        auto weak = weak_from_this();
        try {
            Windows::Storage::Pickers::FolderPicker picker;
            picker.as<IInitializeWithWindow>()->Initialize(host_.window);
            picker.FileTypeFilter().Append(L"*");
            auto folder = co_await picker.PickSingleFolderAsync();
            if (auto view = weak.lock(); view && folder) {
                view->SetFolder(std::wstring(folder.Path()));
                view->SaveOptions();
            }
        } catch (hresult_error const& error) {
            if (auto view = weak.lock()) view->Report(error.message());
        }
    }

    void ChatView::Report(hstring const& message) {
        status_.Text(message);
        status_.Visibility(message.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    void ChatView::SetBusy(bool busy) {
        busy_ = busy;
        modelPicker_->SetEnabled(!busy);
        extras_->SetSending(busy);
        for (auto const& control : {accessChip_, mention_, send_}) control.IsEnabled(!busy);
        auto index = modelPicker_->SelectedIndex();
        web_.IsEnabled(!busy && index >= 0 && routes_.at(index).hostedSearch);
        readPages_.IsEnabled(!busy && index >= 0 && folia::ai::SeparatePageTool(routes_.at(index).pages));
        send_.Visibility(busy ? Visibility::Collapsed : Visibility::Visible);
        stop_.Visibility(busy ? Visibility::Visible : Visibility::Collapsed);
    }

    void ChatView::Command(std::string const& name) {
        if (name == "/new" && host_.newChat) host_.newChat();
        else if (name == "/model" && !busy_) modelPicker_->Open();
        else if (name == "/access" && !busy_) accessChip_.Flyout().ShowAt(accessChip_);
        else if (name == "/compact" && !busy_) CompactContext();
        else if (name == "/latest") transcript_->Latest();
    }

    fire_and_forget ChatView::PickImages() {
        auto weak = weak_from_this();
        try {
            Windows::Storage::Pickers::FileOpenPicker picker;
            picker.as<IInitializeWithWindow>()->Initialize(host_.window);
            for (auto ext : {L".png", L".jpg", L".jpeg", L".webp", L".bmp"}) picker.FileTypeFilter().Append(ext);
            auto files = co_await picker.PickMultipleFilesAsync();
            if (auto view = weak.lock(); view && files.Size()) {
                auto items = single_threaded_vector<Windows::Storage::IStorageItem>();
                for (auto file : files) items.Append(file.as<Windows::Storage::IStorageItem>());
                Windows::ApplicationModel::DataTransfer::DataPackage data;
                data.SetStorageItems(items);
                view->extras_->PasteImages(data.GetView());
            }
        } catch (hresult_error const& error) { if (auto view = weak.lock()) view->Report(error.message()); }
    }
}
