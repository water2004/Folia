#include "pch.h"
#include "ui/ControlHelpers.h"
#include "settings/views/ProviderModelForm.h"
#include "settings/views/SettingsCards.h"
#include "settings/views/SettingsView.h"
#include "settings/views/ProviderConnectionPicker.h"
#include "workspace/WorkspaceIcons.h"
#include "windows/completion/CompletionHttpTransport.h"
#include "settings/views/ProviderIdentity.h"
#include <shobjidl.h>
#include <winrt/Windows.Storage.Pickers.h>

namespace winrt::Folia {
    namespace {
        using namespace ui;
        using namespace models;
        using namespace settings_ui;
        namespace transport = folia::platform::completion;
        void AddActionIcon(Button const& button, wchar_t const* label, WorkspaceIcon icon, bool accent = false) {
            auto row = ButtonRow();
            auto glyph = MakeWorkspaceIcon(icon, accent ? L"TextOnAccentFillColorPrimaryBrush" : L"TextFillColorPrimaryBrush");
            glyph.Style(ResourceStyle(L"SettingsProviderActionIconStyle"));
            row.Children().Append(glyph);
            TextBlock text;
            text.Text(Localize(label));
            if (accent) text.Style(ResourceStyle(L"SettingsAccentActionLabelStyle"));
            row.Children().Append(text);
            button.Content(row);
            Automation::AutomationProperties::SetName(button, text.Text());
        }

        struct ProviderSettings : std::enable_shared_from_this<ProviderSettings> {
            enum class ModelQuery { Fetch, Test };
            HWND owner{};
            ContentControl host;
            weak_ref<ScrollViewer> viewport;
            std::function<void(SettingsBreadcrumb::Path)> setNavigationPath;
            ModelProfile draft;
            ModelProfile pendingConnection;
            StackPanel providerForm{nullptr};
            std::shared_ptr<ProviderModelForm> modelForm;
            TextBox name, base, catalogAddress;
            std::shared_ptr<ProviderConnectionPicker> providerPicker;
            PasswordBox key;
            ToggleSwitch fullUrl, search, readPages;
            StackPanel webOptions{nullptr}, webPermissions{nullptr};
            ComboBox webFormat{nullptr};
            std::optional<folia::ai::WebSearchDialect> selectedFormat;
            TextBlock status, connectionStatus;
            StackPanel modelRows = cards::List();
            Button test;
            ContentControl mark;
            MenuFlyoutItem resetMark;
            std::stop_source query;
            std::size_t generation = 0;
            bool fetching = false;
            std::optional<std::wstring> pendingKey;
            bool clearKeyOnSave = false;
            bool editing = false;
            ContentDialog dialog{nullptr};

            ~ProviderSettings() { query.request_stop(); }

            void CancelQuery() {
                ReportConnection({});
                query.request_stop();
                query = {};
                ++generation;
                fetching = false;
                pendingKey.reset();
                test.IsEnabled(true);
                if (modelForm) modelForm->QueryStatus({});
            }

            void Report(hstring const& message, bool error = false) {
                cards::Status(status, message, error);
            }

            void ReportConnection(hstring const& message, bool error = false) {
                cards::Status(connectionStatus, message, error);
            }

            ModelProfile Connection() const {
                auto const& route = providerPicker->Route();
                ModelProfile connection{draft.id, FieldText(name), FieldText(base), route.protocol};
                connection.providerType = providerPicker->Type().id;
                connection.fullUrl = fullUrl.IsOn();
                if (connection.fullUrl) connection.catalogUrl = FieldText(catalogAddress);
                connection.webFormat = selectedFormat;
                connection.allowWebSearch = !route.search.empty() && search.IsOn();
                connection.allowWebFetch = readPages.IsOn() && (selectedFormat ? folia::ai::SeparatePageTool(route.Pages(selectedFormat)) : route.HasPageTool());
                return connection;
            }

            void ConnectionChanged() {
                if (!fetching) return;
                auto current = Connection();
                if (current.baseUrl != pendingConnection.baseUrl || current.protocol != pendingConnection.protocol ||
                    current.providerType != pendingConnection.providerType ||
                    current.fullUrl != pendingConnection.fullUrl || current.catalogUrl != pendingConnection.catalogUrl ||
                    pendingKey != NewKey())
                    CancelQuery();
            }

            std::optional<std::wstring> NewKey() const {
                if (clearKeyOnSave) return std::wstring{};
                auto password = key.Password();
                if (password.empty()) return std::nullopt;
                return std::wstring(password);
            }

            template<class Result>
            Button Command(wchar_t const* label, Result (ProviderSettings::*handler)(), bool primary = false) {
                auto button = Action(label);
                if (primary) button.Style(ResourceStyle(L"SettingsPrimaryButtonStyle"));
                button.Click([weak = weak_from_this(), handler](auto const&, auto const&) {
                    if (auto self = weak.lock()) ((*self).*handler)();
                });
                return button;
            }

            void AddProvider() { OpenEditor({}); }

            hstring ProviderTitle() const {
                auto title = FieldText(name);
                return title.empty() ? Localize(L"ChatAddProvider") : to_hstring(title);
            }

            void SetPage(UIElement const& element, SettingsBreadcrumb::Path path = {}) {
                setNavigationPath(std::move(path));
                host.Content(element);
                if (auto scroll = viewport.get()) scroll.ChangeView(nullptr, 0.0, nullptr, true);
            }

            void ShowProvider() {
                CancelQuery();
                modelForm.reset();
                DrawModels();
                SetPage(providerForm, {{ProviderTitle(), {}}});
            }

            void ShowList() {
                CancelQuery();
                editing = false;
                key.Password({});
                draft = {};
                providerForm = nullptr;
                modelForm.reset();
                auto list = PagePanel();
                auto add = Command(L"ChatAddProvider", &ProviderSettings::AddProvider, true);
                add.HorizontalAlignment(HorizontalAlignment::Right);
                add.VerticalAlignment(VerticalAlignment::Top);
                AddActionIcon(add, L"ChatAddProvider", WorkspaceIcon::Add, true);
                Grid header;
                header.ColumnSpacing(Metric(L"SettingsCardColumnGap"));
                Columns(header, {true, false});
                Put(header, PageHeader(Localize(L"ChatProviders"), Localize(L"ChatProvidersDescription")), 0);
                Put(header, add, 0, 1);
                list.Children().Append(header);
                auto providers = Profiles();
                auto entries = cards::List();
                if (providers.empty()) entries.Children().Append(Hint(Localize(L"ChatNoProviders")));
                for (auto& profile : providers) {
                    cards::Label label{to_hstring(profile.name), ProviderHost(profile), ProviderMark(profile)};
                    entries.Children().Append(cards::Link(label,
                        [weak = weak_from_this(), profile = std::move(profile)] {
                            if (auto self = weak.lock()) self->OpenEditor(profile);
                        }));
                }
                list.Children().Append(entries);
                SetPage(list);
            }

            Button BuildIconPicker() {
                Button button;
                button.Style(ResourceStyle(L"SettingsProviderIconButtonStyle"));
                auto caption = Localize(L"ChatChooseProviderIcon");
                Automation::AutomationProperties::SetName(button, caption);
                ToolTipService::SetToolTip(button, box_value(caption));
                button.Content(mark);
                MenuFlyout menu;
                MenuFlyoutItem choose;
                choose.Text(caption);
                resetMark.Text(Localize(L"ChatResetProviderIcon"));
                menu.Items().Append(choose);
                menu.Items().Append(resetMark);
                button.Flyout(menu);
                auto weak = weak_from_this();
                choose.Click([weak](auto const&, auto const&) {
                    if (auto self = weak.lock()) self->ChooseIcon();
                });
                resetMark.Click([weak](auto const&, auto const&) {
                    if (auto self = weak.lock()) self->ResetIcon();
                });
                return button;
            }

            Grid BuildCredentialAction() {
                Grid row;
                Columns(row, {false, true});
                row.ColumnSpacing(Metric(L"SettingsCardFieldGap"));
                auto action = SecondaryAction(Localize(L"ChatClearProviderKey"));
                auto message = Hint(Localize(L"ChatClearProviderKeyPending"));
                action.VerticalAlignment(VerticalAlignment::Center);
                message.VerticalAlignment(VerticalAlignment::Center);
                message.Visibility(Visibility::Collapsed);
                Put(row, action, 0);
                Put(row, message, 0, 1);
                action.Click([weak = weak_from_this(), message](auto const& sender, auto const&) {
                    if (auto self = weak.lock()) {
                        const bool clear = self->clearKeyOnSave = !self->clearKeyOnSave;
                        sender.template as<Button>().Content(box_value(Localize(
                            clear ? L"ChatUndoClearProviderKey" : L"ChatClearProviderKey")));
                        message.Visibility(clear ? Visibility::Visible : Visibility::Collapsed);
                        self->key.IsEnabled(!clear);
                        self->ConnectionChanged();
                    }
                });
                return row;
            }

            void RefreshWebOptions() {
                if (!webOptions) return;
                auto const& route = providerPicker->Route();
                webOptions.Children().Clear();
                if (route.search.size() > 1) {
                    webFormat = Choice();
                    cards::ChoiceStyle(webFormat);
                    webFormat.Items().Append(box_value(Localize(L"ChatWebFormatAuto")));
                    for (auto format : route.search) {
                        using enum folia::ai::WebSearchDialect;
                        webFormat.Items().Append(box_value(Localize(format == Standard ? L"AISearchFormatStandard" :
                            format == DeepSeek ? L"AISearchFormatDeepSeek" : L"AISearchFormatOpenRouter")));
                    }
                    auto item = selectedFormat ? std::ranges::find(route.search, *selectedFormat) : route.search.end();
                    webFormat.SelectedIndex(item == route.search.end() ? 0 : static_cast<int>(item - route.search.begin()) + 1);
                    webOptions.Children().Append(cards::Setting({Localize(L"AISearchFormat"), {}, cards::Icon(WorkspaceIcon::Source)}, webFormat));
                    webFormat.SelectionChanged([weak = weak_from_this()](auto const& sender, auto const&) {
                        if (auto self = weak.lock()) {
                            auto const index = sender.template as<ComboBox>().SelectedIndex();
                            if (index < 0) return;
                            self->selectedFormat = index ? std::optional(self->providerPicker->Route().search.at(index - 1)) : std::nullopt;
                            self->RefreshWebPermissions();
                        }
                    });
                } else webFormat = nullptr;
                webPermissions = cards::List();
                webOptions.Children().Append(webPermissions);
                RefreshWebPermissions();
            }

            void RefreshWebPermissions() {
                auto const& route = providerPicker->Route();
                const bool allowSearch = search.IsOn(), allowFetch = readPages.IsOn();
                webPermissions.Children().Clear();
                search = cards::Switch(allowSearch);
                readPages = cards::Switch(allowFetch);
                const bool independent = selectedFormat ? folia::ai::SeparatePageTool(route.Pages(selectedFormat)) : route.HasPageTool();
                if (!route.search.empty()) {
                    const auto label = !independent && route.Pages(selectedFormat) == folia::ai::PageAccess::WithSearch
                        ? L"ChatSearchAndBrowse" : L"ChatProviderWebSearch";
                    webPermissions.Children().Append(cards::Setting(
                        {Localize(label), Localize(L"ChatProviderWebSearchHint"), cards::Icon(WorkspaceIcon::Network)},
                        search, cards::Placement::Wrap));
                }
                if (independent)
                    webPermissions.Children().Append(cards::Setting(
                        {Localize(L"ChatAllowPageRead"), Localize(L"ChatProviderPageReadHint"), cards::Icon(WorkspaceIcon::Document)},
                        readPages, cards::Placement::Wrap));
            }

            StackPanel BuildConnection() {
                auto connection = SectionPanel();
                TextField(name, Localize(L"ChatProviderName"));
                Grid providerName;
                providerName.ColumnSpacing(Metric(L"SettingsCardFieldGap"));
                Columns(providerName, {false, true});
                Put(providerName, BuildIconPicker(), 0);
                Put(providerName, name, 0, 1);
                connection.Children().Append(providerName);

                providerPicker = std::make_shared<ProviderConnectionPicker>();
                providerPicker->Build(connection, draft.providerType, draft.protocol,
                    [weak = weak_from_this()](ProviderPreset const& previous, ProviderRoute const& oldRoute) {
                        if (auto self = weak.lock()) {
                            auto const& type = self->providerPicker->Type();
                            auto const& route = self->providerPicker->Route();
                            auto display = FieldText(self->name);
                            if (display.empty() || display == previous.name) self->name.Text(to_hstring(type.name));
                            auto address = FieldText(self->base);
                            if (!self->fullUrl.IsOn() && (address.empty() || address == oldRoute.address))
                                self->base.Text(to_hstring(route.address));
                            self->selectedFormat.reset();
                            self->RefreshWebOptions();
                            self->ConnectionChanged();
                        }
                    });

                TextField(key, Localize(L"AIKey"));
                if (!draft.id.empty()) key.PlaceholderText(Localize(L"ChatKeepProviderKey"));
                connection.Children().Append(key);
                if (!draft.id.empty()) connection.Children().Append(BuildCredentialAction());

                Grid endpoint;
                endpoint.ColumnSpacing(Metric(L"SettingsCardColumnGap"));
                endpoint.RowSpacing(Metric(L"SettingsCardFieldGap"));
                Columns(endpoint, {true, false});
                Rows(endpoint, {false, false, false});
                auto addressLabel = Text(Localize(L"AIBaseUrl"));
                addressLabel.VerticalAlignment(VerticalAlignment::Center);
                Put(endpoint, addressLabel, 0);
                AddActionIcon(test, L"ChatTestConnection", WorkspaceIcon::TestConnection);
                test.Style(ResourceStyle(L"SettingsCardInlineButtonStyle"));
                Put(endpoint, test, 0, 1);
                TextField(base, {});
                Grid::SetColumnSpan(base, 2);
                Put(endpoint, base, 1);
                Grid::SetColumnSpan(connectionStatus, 2);
                Put(endpoint, connectionStatus, 2);
                connection.Children().Append(endpoint);
                TextField(catalogAddress, Localize(L"ChatCatalogUrl"));
                catalogAddress.PlaceholderText(Localize(L"ChatCatalogUrlPlaceholder"));
                connection.Children().Append(catalogAddress);

                auto options = SectionPanel();
                options.Children().Append(cards::Setting(
                    {Localize(L"ChatUseFullUrl"), {}, cards::Icon(WorkspaceIcon::Source)}, fullUrl));
                webOptions = cards::List();
                RefreshWebOptions();
                options.Children().Append(webOptions);
                auto update = [weak = weak_from_this(), addressLabel] {
                    if (auto self = weak.lock()) {
                        const auto full = self->fullUrl.IsOn();
                        auto label = Localize(full ? L"ChatRequestUrl" : L"AIBaseUrl");
                        addressLabel.Text(label);
                        Automation::AutomationProperties::SetName(self->base, label);
                        ToolTipService::SetToolTip(self->base, box_value(Localize(full ? L"ChatFullUrlHint" : L"AIBaseUrlDescription")));
                        self->base.PlaceholderText(full ? L"https://api.example.com/custom/generate" : L"https://api.example.com/v1");
                        self->catalogAddress.IsEnabled(full);
                        self->ConnectionChanged();
                    }
                };
                fullUrl.Toggled([update](auto const&, auto const&) { update(); });
                update();
                auto section = SectionPanel();
                section.Children().Append(cards::Section(Localize(L"AIService"), connection));
                section.Children().Append(cards::Section(Localize(L"ChatConnectionOptions"), options));
                return section;
            }

            void OpenEditor(ModelProfile profile) {
                CancelQuery();
                editing = true;
                draft = std::move(profile);
                name = TextBox{};
                base = TextBox{};
                key = PasswordBox{};
                clearKeyOnSave = false;
                fullUrl = cards::Switch(draft.fullUrl);
                search = cards::Switch(draft.allowWebSearch);
                readPages = cards::Switch(draft.allowWebFetch);
                webOptions = nullptr;
                selectedFormat = draft.webFormat;
                catalogAddress = TextBox{};
                status = TextBlock{};
                connectionStatus = TextBlock{};
                modelRows = cards::List();
                test = Action(L"ChatTestConnection");
                mark = ContentControl{};
                resetMark = MenuFlyoutItem{};
                name.Text(to_hstring(draft.name));
                catalogAddress.Text(to_hstring(draft.catalogUrl));
                base.Text(to_hstring(draft.baseUrl));
                auto form = PagePanel();
                form.Style(ResourceStyle(L"SettingsOptionPageStyle"));
                form.Children().Append(BuildConnection());
                DrawMark();

                auto add = Action(L"ChatAddModel");
                AddActionIcon(add, L"ChatAddModel", WorkspaceIcon::Add);
                form.Children().Append(cards::Section(Localize(L"ChatModels"), modelRows, add));
                auto footer = ButtonRow();
                footer.Children().Append(Command(L"ChatSaveProvider", &ProviderSettings::Save, true));
                footer.Children().Append(Command(L"Cancel", &ProviderSettings::ShowList));
                if (!draft.id.empty())
                    footer.Children().Append(Command(L"ChatRemoveProvider", &ProviderSettings::RemoveProvider));
                form.Children().Append(footer);
                form.Children().Append(status);
                Report({});
                ReportConnection({});
                providerForm = form;
                ShowProvider();

                auto weak = weak_from_this();
                name.TextChanged([weak](auto const&, auto const&) {
                    if (auto self = weak.lock(); self && self->draft.iconPng.empty()) self->DrawMark();
                });
                auto connectionChanged = [weak](auto const&, auto const&) {
                    if (auto self = weak.lock()) self->ConnectionChanged();
                };
                base.TextChanged(connectionChanged);
                catalogAddress.TextChanged(connectionChanged);
                key.PasswordChanged(connectionChanged);
                test.Click([weak](auto const&, auto const&) {
                    if (auto self = weak.lock()) self->StartQuery(ModelQuery::Test);
                });
                add.Click([weak](auto const&, auto const&) {
                    if (auto self = weak.lock()) self->EditModel(std::nullopt);
                });
            }

            void DrawMark() {
                ModelProfile preview;
                preview.name = FieldText(name);
                preview.iconPng = draft.iconPng;
                mark.Content(ProviderMark(preview));
                resetMark.IsEnabled(!preview.iconPng.empty());
            }

            void ResetIcon() {
                draft.iconPng.clear();
                DrawMark();
            }

            fire_and_forget ChooseIcon() {
                auto keep = shared_from_this();
                auto field = name;
                try {
                    Windows::Storage::Pickers::FileOpenPicker picker;
                    check_hresult(picker.as<::IInitializeWithWindow>()->Initialize(owner));
                    picker.FileTypeFilter().Append(L".png");
                    auto file = co_await picker.PickSingleFileAsync();
                    if (!file) co_return;
                    auto png = co_await ImportProviderIcon(file);
                    if (!editing || name != field || !host.IsLoaded()) co_return;
                    draft.iconPng = to_string(png);
                    DrawMark();
                } catch (hresult_error const&) {
                    if (editing && name == field && host.IsLoaded()) Report(Localize(L"ChatInvalidProviderIcon"), true);
                } catch (std::exception const& error) {
                    if (editing && name == field && host.IsLoaded()) Report(to_hstring(error.what()), true);
                }
            }

            void DrawModels() {
                auto children = modelRows.Children();
                children.Clear();
                if (draft.models.empty()) children.Append(Hint(Localize(L"ChatNoProviderModels")));
                for (std::size_t index = 0; index < draft.models.size(); ++index) {
                    auto const& model = draft.models[index];
                    auto title = to_hstring(model.Label());
                    auto detail = model.Label() != model.upstream ? to_hstring(model.upstream) : hstring{};
                    if (!model.efforts.empty()) {
                        std::string joined;
                        for (auto const& level : model.efforts) {
                            if (!joined.empty()) joined += ", ";
                            joined += level;
                        }
                        detail = detail.empty() ? to_hstring(joined) : detail + L" · " + to_hstring(joined);
                    }
                    auto enabled = cards::Switch(model.enabled);
                    Automation::AutomationProperties::SetName(enabled, LocalizeFormat(L"ChatToggleModel", {title}));
                    enabled.Toggled([weak = weak_from_this(), index](auto const& sender, auto const&) {
                        if (auto self = weak.lock()) self->draft.models.at(index).enabled = sender.template as<ToggleSwitch>().IsOn();
                    });
                    children.Append(cards::LinkWithSwitch({title, detail, cards::Icon(WorkspaceIcon::Model)}, enabled,
                        [weak = weak_from_this(), index] { if (auto self = weak.lock()) self->EditModel(index); }));
                }
            }

            void Save() {
                try {
                    auto connection = Connection();
                    transport::CompletionConfig config;
                    config.baseUrl = connection.baseUrl;
                    config.protocol = static_cast<int>(connection.protocol);
                    config.fullUrl = connection.fullUrl;
                    config.catalogUrl = connection.catalogUrl;
                    auto error = transport::ValidateCompletionService(config);
                    if (!error.empty()) throw std::invalid_argument(error);
                    if (connection.name.empty() || connection.name.size() > 128) {
                        Report(Localize(L"ChatInvalidProfile"), true);
                        return;
                    }
                    connection.models = draft.models;
                    connection.iconPng = draft.iconPng;
                    SaveProfile(std::move(connection), NewKey());
                    ShowList();
                } catch (std::exception const& e) { Report(to_hstring(e.what()), true); }
                catch (hresult_error const& e) { Report(e.message(), true); }
            }

            fire_and_forget RemoveProvider() {
                if (dialog) co_return;
                auto keep = shared_from_this();
                try {
                    auto id = draft.id;
                    ContentDialog remove;
                    dialog = remove;
                    remove.XamlRoot(host.XamlRoot());
                    remove.Title(box_value(Localize(L"ChatRemoveProvider")));
                    remove.Content(box_value(Localize(L"ChatRemoveProviderConfirm")));
                    remove.PrimaryButtonText(Localize(L"Remove"));
                    remove.CloseButtonText(Localize(L"Cancel"));
                    if (co_await remove.ShowAsync() == ContentDialogResult::Primary) {
                        DeleteProfile(id);
                        ShowList();
                    }
                } catch (std::exception const& e) { Report(to_hstring(e.what()), true); }
                catch (hresult_error const& e) { Report(e.message(), true); }
                dialog = nullptr;
            }

            void EditModel(std::optional<std::size_t> index) {
                CancelQuery();
                auto model = index ? draft.models.at(*index) : ConfiguredModel{};
                auto title = index ? to_hstring(model.Label()) : Localize(L"ChatAddModel");
                auto weak = weak_from_this();
                auto save = [weak, index](ConfiguredModel result) {
                    if (auto self = weak.lock()) {
                        if (index) self->draft.models.at(*index) = std::move(result);
                        else self->draft.models.push_back(std::move(result));
                        self->ShowProvider();
                    }
                };
                auto back = [weak] { if (auto self = weak.lock()) self->ShowProvider(); };
                auto remove = index ? ProviderModelForm::Command([weak, index] {
                    if (auto self = weak.lock()) {
                        self->draft.models.erase(self->draft.models.begin() + *index);
                        self->ShowProvider();
                    }
                }) : ProviderModelForm::Command{};
                auto fetch = [weak] { if (auto self = weak.lock()) self->StartQuery(ModelQuery::Fetch); };
                modelForm = ProviderModelForm::Create(std::move(model), Connection(),
                    std::move(save), back, std::move(remove), std::move(fetch));
                SetPage(modelForm->Element(), {{ProviderTitle(), std::move(back)}, {title, {}}});
            }

            void StartQuery(ModelQuery action) {
                CancelQuery();
                pendingConnection = Connection();
                fetching = true;
                pendingKey = NewKey();
                test.IsEnabled(false);
                if (modelForm) modelForm->QueryStatus(Localize(L"ChatFetchingModels"), true);
                else ReportConnection(Localize(L"ChatTestingConnection"));
                Query(weak_from_this(), action, pendingConnection, pendingKey, query.get_token(), generation, host.DispatcherQueue());
            }

            static fire_and_forget Query(std::weak_ptr<ProviderSettings> weak, ModelQuery action, ModelProfile profile,
                std::optional<std::wstring> key, std::stop_token stop, std::size_t generation,
                Microsoft::UI::Dispatching::DispatcherQueue queue) {
                co_await resume_background();
                auto start = std::chrono::steady_clock::now();
                std::vector<transport::CompletionModel> offered;
                hstring error;
                try {
                    transport::CompletionConfig config;
                    config.baseUrl = profile.baseUrl;
                    config.protocol = static_cast<int>(profile.protocol);
                    config.fullUrl = profile.fullUrl;
                    config.catalogUrl = profile.catalogUrl;
                    offered = transport::FetchCompletionModels(config, stop,
                        key ? std::move(*key) : profile.id.empty() ? std::wstring{} : ProfileKey(profile), Preset(profile.providerType).catalog);
                } catch (std::exception const& e) { error = to_hstring(e.what()); }
                catch (hresult_error const& e) { error = e.message(); }
                if (stop.stop_requested()) co_return;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
                queue.TryEnqueue([weak, action, generation, elapsed, offered = std::move(offered), error = std::move(error)]() mutable {
                    auto self = weak.lock();
                    if (!self || self->generation != generation || !self->host.IsLoaded()) return;
                    self->CancelQuery();
                    if (action == ModelQuery::Test) {
                        self->ReportConnection(error.empty()
                            ? LocalizeFormat(L"ChatConnectionOK", {to_hstring(elapsed.count())}) : error, !error.empty());
                    } else if (auto form = self->modelForm) {
                        if (!error.empty()) form->QueryStatus(error, false, true);
                        else if (offered.empty()) form->QueryStatus(Localize(L"AINoModels"));
                        else form->SetCatalog(std::move(offered));
                    }
                });
            }

        };
    }

    Microsoft::UI::Xaml::UIElement SettingsView::BuildProvidersPage() {
        using namespace settings_ui;
        auto state = std::make_shared<ProviderSettings>();
        state->owner = owner_;
        state->host.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        state->setNavigationPath = [weak = weak_from_this(), providers = state->weak_from_this()](SettingsBreadcrumb::Path children) {
            if (auto self = weak.lock(); self && self->currentPage_ == Page::Providers)
                self->SetNavigationPath(std::move(children), [providers] { if (auto page = providers.lock()) page->ShowList(); });
        };
        auto page = PageScroll(state->host);
        state->viewport = make_weak(page);
        page.Unloaded([state](auto const&, auto const&) {
            state->CancelQuery();
            state->editing = false;
            if (state->dialog) state->dialog.Hide();
        });
        state->ShowList();
        return page;
    }
}
