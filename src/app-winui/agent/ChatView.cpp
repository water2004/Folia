#include "pch.h"
#include "agent/ChatView.h"
#include "agent/ChatControls.h"
#include "providers/ModelValidation.h"
#include "workspace/WorkspaceIcons.h"
#include "windows/ai/ModelHttpClient.h"
#include <shobjidl.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <wil/resource.h>

namespace winrt::Folia::chat {
    using namespace models;
    using folia::ai::MessageRole;
    using folia::ai::agent::Options;
    using folia::ai::agent::ProgressKind;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {
        constexpr std::size_t maxQuestion = 16'000;
        std::mutex runningMutex;
        std::set<std::string> running;

        void Label(Button const& button, wchar_t const* key) {
            auto name = Localize(key);
            Automation::AutomationProperties::SetName(button, name);
            ToolTipService::SetToolTip(button, box_value(name));
        }

        void Chip(Button const& button, TextBlock const& text, WorkspaceIcon icon) {
            button.Style(settings_ui::ResourceStyle(L"ChatActionStyle"));
            StackPanel row;
            row.Orientation(Orientation::Horizontal);
            row.Spacing(5);
            auto glyph = MakeWorkspaceIcon(icon);
            glyph.Width(14); glyph.Height(14);
            row.Children().Append(glyph);
            text.FontSize(12);
            text.VerticalAlignment(VerticalAlignment::Center);
            text.TextTrimming(TextTrimming::CharacterEllipsis);
            row.Children().Append(text);
            auto arrow = MakeWorkspaceIcon(WorkspaceIcon::Chevron);
            arrow.Width(11); arrow.Height(11);
            row.Children().Append(arrow);
            button.Content(row);
        }

        std::string TitleFrom(std::string_view text) {
            auto line = text.substr(0, text.find_first_of("\r\n"));
            auto end = std::min<std::size_t>(line.size(), 80);
            while (end < line.size() && end && (static_cast<unsigned char>(line[end]) & 0xc0) == 0x80) --end;
            return std::string(line.substr(0, end));
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
        if (transcript_) transcript_->SetTheme(theme);
    }

    void ChatView::Build() {
        const auto weak = weak_from_this();
        auto click = [weak](auto member) {
            return [weak, member](auto const&, auto const&) { if (auto view = weak.lock()) ((*view).*member)(); };
        };
        transcript_ = ChatTranscript::Create(conversation_.id, theme_);
        Put(root_, transcript_->Element(), 0);
        if (auto last = transcript_->LastReply()) {
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
        input_.MaxLength(maxQuestion);
        extras_ = ChatComposerExtras::Create(input_, host_.tabs,
            [weak] { if (auto view = weak.lock()) return view->conversation_.folder; return std::filesystem::path{}; },
            [weak](hstring const& message) { if (auto view = weak.lock()) view->Report(message); });
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
        mention_.Content(box_value(L"@"));
        Label(mention_, L"ChatMention");
        mention_.Click([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->extras_->BeginMention(); });
        Put(bar_, mention_, 0);
        Chip(accessChip_, accessLabel_, WorkspaceIcon::Shield);
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
        Put(bar_, contextMeter_, 0, 2);
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
        Put(bar_, modelPicker_->Element(), 0, 3);
        openTabs_.Text(Localize(L"ChatAllowOpenTabs"));
        web_.Text(Localize(L"ChatAllowSearch"));
        for (auto const& [button, icon, key] : {std::tuple{stop_, WorkspaceIcon::Stop, L"ChatStop"},
            std::tuple{send_, WorkspaceIcon::Send, L"ChatSend"}}) {
            button.Style(settings_ui::ResourceStyle(L"ChatSendStyle"));
            button.Content(MakeWorkspaceIcon(icon, L"TextOnAccentFillColorPrimaryBrush"));
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
                view->accessLabel_.Visibility(narrow ? Visibility::Collapsed : Visibility::Visible);
                view->accessLabel_.MaxWidth(150);
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
        send_.Click(click(&ChatView::Send));
        stop_.Click([weak](auto const&, auto const&) {
            if (auto view = weak.lock()) view->cancellation_.request_stop();
        });
        RefreshProfiles();
        RefreshAccessLabel();
        SetBusy(false);
        ArrangeComposer();
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
            const bool hostedSearch = CanUseHostedSearch(model, *profile);
            routes_.push_back({std::move(model), hostedSearch});
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
        web_.Visibility(hostedSearch ? Visibility::Visible : Visibility::Collapsed);
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

    void ChatView::SaveOptions() {
        if (busy_) return;
        conversation_.openTabs = openTabs_.IsChecked();
        RefreshAccessLabel();
        try { SaveConversationOptions(conversation_); }
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
        send_.Visibility(busy ? Visibility::Collapsed : Visibility::Visible);
        stop_.Visibility(busy ? Visibility::Visible : Visibility::Collapsed);
    }

    void ChatView::Send() {
        if (busy_) return;
        if (extras_->Pasting()) { Report(Localize(L"ChatPreparingImage")); return; }
        try {
            auto question = FieldText(input_);
            if (question.empty() && extras_->Images().empty()) return;
            auto active = modelPicker_->SelectedIndex();
            if (active < 0) { Report(Localize(L"ChatNeedsProvider")); return; }
            auto [model, profile] = Resolve(routes_.at(active).model.id);
            if (!model.vision && !extras_->Images().empty()) { Report(Localize(L"ChatNeedsVision")); return; }
            if (question.size() > maxQuestion) {
                Report(Localize(L"ChatInvalidPrompt"));
                return;
            }
            {
                std::lock_guard lock(runningMutex);
                if (!running.insert(conversation_.id).second) {
                    Report(Localize(L"ChatAlreadyRunning"));
                    return;
                }
            }
            auto release = wil::scope_exit([&] { std::lock_guard lock(runningMutex); running.erase(conversation_.id); });
            std::optional<std::vector<OpenTab>> tabs;
            auto available = host_.tabs();
            if (!openTabs_.IsChecked()) std::erase_if(available, [&](auto const& tab) { return !extras_->Mentions(tab); });
            if (openTabs_.IsChecked() || !available.empty()) tabs = std::move(available);
            SaveConversationOptions(conversation_);
            if (transcript_->Empty()) conversation_ = RenameConversation(conversation_.id,
                question.empty() ? to_string(Localize(L"ChatImageMessage")) : TitleFrom(question), true);
            Entry entry{MessageRole::User, std::move(question)};
            auto rollback = wil::scope_exit([&] {
                for (auto const& image : entry.images) DeleteFileW(AttachmentPath(conversation_.id, image).c_str());
            });
            for (auto const& image : extras_->Images()) entry.images.push_back(StoreImage(conversation_.id, image.png));
            AppendEntry(conversation_.id, entry);
            rollback.release();
            extras_->Clear();
            input_.Text({});
            transcript_->Refresh(true);
            transcript_->Begin();
            ArrangeComposer();
            host_.changed(conversation_);
            cancellation_ = {};
            SetBusy(true);
            Report({});
            Execute(weak_from_this(), conversation_, std::move(profile), std::move(model), std::move(tabs),
                cancellation_.get_token(), root_.DispatcherQueue());
            release.release();
        } catch (std::exception const& error) { SetBusy(false); Report(to_hstring(error.what())); }
        catch (hresult_error const& error) { SetBusy(false); Report(error.message()); }
    }

    fire_and_forget ChatView::Execute(std::weak_ptr<ChatView> weak, Conversation conversation, ModelProfile profile, ConfiguredModel model,
        std::optional<std::vector<OpenTab>> tabs, std::stop_token stop,
        Microsoft::UI::Dispatching::DispatcherQueue queue) {
        auto release = wil::scope_exit([&] { std::lock_guard lock(runningMutex); running.erase(conversation.id); });
        co_await resume_background();
        std::wstring error;
        std::optional<Entry> answer;
        Entry partial{MessageRole::Assistant};
        bool started = false;
        try {
            auto history = LoadHistory(conversation.id);
            auto tools = MakeTools(std::move(tabs), conversation.folder);
            Options options{model.upstream, model.ResolveEffort(conversation.effort),
                            conversation.search && CanUseHostedSearch(model, profile)};
            options.requestReasoningSummary = profile.protocol == folia::ai::ApiProtocol::Responses;
            // A UTF-8 byte budget conservatively bounds text tokens without guessing the upstream tokenizer.
            if (model.maxInputTokens) options.inputBytes = *model.maxInputTokens;
            if (model.maxOutputTokens) options.outputTokens = (std::min)(options.outputTokens, *model.maxOutputTokens);
            if (model.vision) options.loadImage = [&](std::string const& image) { return LoadImage(conversation.id, image); };
            std::string preview, reasoning;
            auto nextPaint = std::chrono::steady_clock::time_point{};
            auto publish = [&](ProgressKind kind, std::string_view text) {
                bool delta = kind == ProgressKind::TextDelta || kind == ProgressKind::ReasoningDelta;
                if (kind == ProgressKind::StepStarted) { preview.clear(); reasoning.clear(); nextPaint = {}; }
                if (delta) {
                    auto& part = kind == ProgressKind::TextDelta ? preview : reasoning;
                    part.append(text);
                    auto now = std::chrono::steady_clock::now();
                    if (now < nextPaint) return;
                    nextPaint = now + std::chrono::milliseconds(45);
                    text = part;
                    kind = kind == ProgressKind::TextDelta ? ProgressKind::TextSnapshot : ProgressKind::ReasoningSnapshot;
                }
                auto value = to_hstring(text);
                queue.TryEnqueue([weak, kind, value] {
                    if (auto view = weak.lock(); view && view->busy_) {
                        if (kind == ProgressKind::InputUsage) {
                            view->usedTokens_ = std::stoull(value.c_str());
                            view->usageModel_ = view->conversation_.model;
                            view->UpdateUsage();
                        } else view->transcript_->Progress(kind, value);
                    }
                });
            };
            auto request = [&](auto const& wire, auto token, folia::ai::agent::Progress const& progress) {
                return folia::platform::models::Request(profile.baseUrl, profile.protocol, wire, ProfileKey(profile), token, {},
                    profile.fullUrl, [&](folia::ai::StreamEvent const& event) {
                        if (!progress) return;
                        if (event.kind == folia::ai::StreamEventKind::TextDelta) progress(ProgressKind::TextDelta, event.text);
                        if (event.kind == folia::ai::StreamEventKind::ReasoningDelta) progress(ProgressKind::ReasoningDelta, event.text);
                    });
            };
            started = true;
            answer = folia::ai::agent::Run(history, options, tools, request, publish, stop, &partial);
            answer->modelId = model.id;
            started = false;
            AppendEntry(conversation.id, *answer, &history);
        } catch (std::exception const& exception) { answer.reset(); error = to_hstring(exception.what()); }
        catch (hresult_error const& exception) { answer.reset(); error = exception.message(); }
        if (!answer && started) {
            partial.modelId = model.id;
            partial.interrupted = true;
            try { AppendEntry(conversation.id, partial); }
            catch (std::exception const& exception) { error = to_hstring(exception.what()); }
        }
        queue.TryEnqueue([weak, answer = std::move(answer), error = std::move(error), cancelled = stop.stop_requested()]() mutable {
            auto view = weak.lock();
            if (!view) return;
            view->SetBusy(false);
            if (answer) {
                view->usageModel_ = answer->modelId;
                view->usedTokens_ = answer->usage.inputTokens;
                view->UpdateUsage();
            }
            view->transcript_->Finish();
            view->Report(cancelled ? Localize(L"ChatStopped") : hstring(error));
        });
    }
}
