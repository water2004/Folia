#include "pch.h"
#include "settings/storage/ModelCatalog.h"
#include <wil/resource.h>
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include "localization/Localization.h"
#include "settings/views/SettingsView.h"
#include "theme/ThemeConfig.h"
#include "../support/CompletionHttpServer.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <microsoft.ui.xaml.window.h>

namespace {
using namespace boost::ut;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace document_tests;
template<class T> T First(DependencyObject const& parent) {
    if (auto value = parent.try_as<T>()) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = First<T>(Media::VisualTreeHelper::GetChild(parent, i))) return found;
    return nullptr;
}
TextBlock Description(DependencyObject const& parent, winrt::hstring const& text) {
    if (auto value = parent.try_as<TextBlock>(); value && value.Text() == text) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Description(Media::VisualTreeHelper::GetChild(parent, i), text)) return found;
    return nullptr;
}
template<class T> T Field(DependencyObject const& parent, winrt::hstring const& label) {
    if (auto value = parent.try_as<T>(); value && (Automation::AutomationProperties::GetName(value) == label
        || (value.Header() && winrt::unbox_value<winrt::hstring>(value.Header()) == label))) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Field<T>(Media::VisualTreeHelper::GetChild(parent, i), label)) return found;
    return nullptr;
}
Button Action(DependencyObject const& parent, winrt::hstring const& label) {
    if (auto button = parent.try_as<Button>(); button != nullptr) {
        if (Automation::AutomationProperties::GetName(button) == label) return button;
        auto value = button.Content().try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (value != nullptr && value.Type() == winrt::Windows::Foundation::PropertyType::String)
            if (value.GetString() == label) return button;
    }
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Action(Media::VisualTreeHelper::GetChild(parent, i), label)) return found;
    return nullptr;
}
CheckBox Effort(DependencyObject const& parent, winrt::hstring const& label) {
    if (auto choices = First<VariableSizedWrapGrid>(parent))
        for (auto const& child : choices.Children())
            if (auto check = child.try_as<CheckBox>(); check && winrt::unbox_value<winrt::hstring>(check.Content()) == label)
                return check;
    return nullptr;
}
void InvokeAction(Button const& button) {
    auto peer = Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(button);
    peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
}
struct ModelSettingsPage {
    Window window;
    Grid root;
    std::shared_ptr<winrt::Folia::SettingsView> view;
    FrameworkElement page{nullptr};
    std::optional<winrt::Folia::AppSettings> applied;
    ModelSettingsPage(winrt::Folia::SettingsView::Page kind, winrt::Folia::AppSettings initial = {}) {
        ContentControl host;
        view = std::make_shared<winrt::Folia::SettingsView>(host, initial,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [this](auto const& settings) -> std::optional<winrt::hstring> {
                applied = settings; return {};
            });
        window.Content(root);
        root.RequestedTheme(ElementTheme::Light);
        root.Background(Media::SolidColorBrush({255, 247, 248, 250}));
        root.Children().Append(host);
        Resize(1024, 740);
        window.Activate();
        Pump([&] { return root.IsLoaded() && root.ActualWidth() > 100; });
        view->Navigate(kind);
        root.UpdateLayout();
        page = view->PageContent().as<FrameworkElement>();
    }
    void Resize(int width, int height) {
        HWND handle{};
        winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
        auto scale = double(GetDpiForWindow(handle)) / 96.;
        window.AppWindow().Resize({static_cast<int>(width * scale), static_cast<int>(height * scale)});
    }
    ~ModelSettingsPage() { view->Detach(); window.Close(); }
};

void SaveSettingsImage(Grid const& root, std::string_view name) {
    auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
    Pump([&] { return std::chrono::steady_clock::now() >= settled; });
    root.UpdateLayout();
    Media::Imaging::RenderTargetBitmap image;
    Await(image.RenderAsync(root));
    auto pixels = Await(image.GetPixelsAsync());
    auto directory = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
        / "build/tests/settings-styles";
    visual_tests::SavePng(directory / name, image.PixelWidth(), image.PixelHeight(), pixels);
}

suite settings_style_tests = [] {
    "document media settings select a Mermaid theme independently of Folia"_test = [] {
        Window window;
        Grid root;
        window.Content(root);
        std::optional<winrt::Folia::AppSettings> applied;
        ContentControl pageHost;
        auto view = std::make_shared<winrt::Folia::SettingsView>(pageHost, winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [&](auto const& settings) -> std::optional<winrt::hstring> {
                applied = settings;
                return {};
            });
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(pageHost);
        window.Activate();
        Pump([&] { return root.IsLoaded(); });
        view->Navigate(winrt::Folia::SettingsView::Page::General);
        root.UpdateLayout();
        auto page = view->PageContent().as<FrameworkElement>();
        expect(First<Expander>(page) == nullptr);
        expect(Description(page, winrt::Folia::Localize(L"WindowAppearance")) != nullptr);
        expect(Description(page, winrt::Folia::Localize(L"DocumentMedia")) != nullptr);
        auto width = Field<ComboBox>(page, winrt::Folia::Localize(L"ImageMaximumWidth"));
        auto theme = Field<ComboBox>(page, winrt::Folia::Localize(L"MermaidTheme"));
        expect(fatal(width && theme));
        expect(winrt::unbox_value<winrt::hstring>(width.SelectedItem()) == L"100%");
        expect(theme.SelectedIndex() == 0_i);
        for (std::uint32_t i = 0; i < theme.Items().Size(); ++i)
            if (winrt::unbox_value<winrt::hstring>(theme.Items().GetAt(i)) == L"forest")
                theme.SelectedIndex(static_cast<int>(i));
        Pump([&] { return applied && applied->media.mermaidTheme == "forest"; });
        width.SelectedIndex(1);
        Pump([&] { return applied && applied->media.imageMaxWidthPercent == 50; });
        expect(applied->media.mermaidTheme == "forest" && applied->themeId == "system");
        theme.SelectedIndex(0);
        Pump([&] { return applied && applied->media.mermaidTheme.empty(); });
        expect(applied->media.imageMaxWidthPercent == 50);
    };

    "completion saves the chosen model and independent input prediction budgets"_test = [] {
        using namespace winrt::Folia;
        models::ModelProfile draft{{}, "Responses settings fixture", "https://fixture.invalid/v1"};
        draft.models.push_back({{}, "Named Responses model", {}, "fixture", "none", false, {"none", "high"}});
        auto provider = models::SaveProfile(std::move(draft), std::nullopt);
        auto const& model = provider.models.front();
        auto cleanup = wil::scope_exit([&] { models::DeleteProfile(provider.id); });
        AppSettings initial;
        initial.completionModelId = model.id;
        initial.completion.enabled = true;
        ModelSettingsPage fixture(SettingsView::Page::Completion, initial);
        auto choices = Field<ComboBox>(fixture.page, Localize(L"AIModel"));
        auto reasoning = Field<ComboBox>(fixture.page, Localize(L"AIReasoning"));
        auto cache = Field<ToggleSwitch>(fixture.page, Localize(L"AIExplicitCache"));
        auto context = Field<TextBox>(fixture.page, Localize(L"AIContextKB"));
        auto output = Field<TextBox>(fixture.page, Localize(L"AIOutputTokens"));
        if (!expect(choices && reasoning && cache && context && output)) return;
        auto fields = context.Parent().try_as<StackPanel>();
        expect(fields && output.Parent() == fields);
        if (fields) expect(std::abs(context.ActualWidth() - fields.ActualWidth()) < 1.);
        expect(winrt::unbox_value<winrt::hstring>(choices.SelectedItem()) == L"Named Responses model");
        expect(Field<PasswordBox>(fixture.page, Localize(L"AIKey")) == nullptr);
        expect(!reasoning.IsEditable());
        expect(reasoning.SelectedIndex() == 0_i);
        expect(reasoning.Items().Size() == 3_u);
        auto modelCard = choices.Parent().as<Grid>();
        auto reasoningCard = reasoning.Parent().as<Grid>();
        auto modelRight = choices.TransformToVisual(fixture.root).TransformPoint(
            {static_cast<float>(choices.ActualWidth()), 0}).X;
        auto reasoningRight = reasoning.TransformToVisual(fixture.root).TransformPoint(
            {static_cast<float>(reasoning.ActualWidth()), 0}).X;
        expect(modelCard != reasoningCard && std::abs(modelRight - reasoningRight) < 1.0f);
        SaveSettingsImage(fixture.root, "completion.png");
        reasoning.SelectedIndex(2);
        expect(cache.Visibility() == Visibility::Visible);
        context.Text(L"10");
        cache.IsOn(true);
        InvokeAction(Action(fixture.page, Localize(L"Save")));
        Pump([&] { return fixture.applied.has_value(); });
        expect(fixture.applied->completionModelId == model.id);
        expect(fixture.applied->completion.contextKB == 10_u);
        expect(fixture.applied->completion.explicitPromptCache);
        expect(fixture.applied->completion.reasoningEffort == "high");
    };

    "provider settings stage a model and its reasoning configuration until the provider is saved"_test = [] {
        using namespace winrt::Folia;
        auto provider = models::SaveProfile({{}, "Model settings fixture", "https://fixture.invalid/v1"}, std::nullopt);
        auto cleanup = wil::scope_exit([&] { models::DeleteProfile(provider.id); });
        ModelSettingsPage fixture(SettingsView::Page::Providers);
        auto card = Action(fixture.page, L"Model settings fixture");
        if (!expect(card != nullptr)) return;
        expect(Description(card, L"fixture.invalid") != nullptr);
        expect(Description(card, L"MS") != nullptr);
        expect(Field<TextBox>(fixture.page, Localize(L"AIBaseUrl")) == nullptr);
        InvokeAction(card);
        Pump([&] { return Action(fixture.page, Localize(L"ChatAddModel")) != nullptr; });
        auto back = Action(fixture.root, Localize(L"ChatProviders"));
        auto providerName = Field<TextBox>(fixture.page, Localize(L"ChatProviderName"));
        auto providerProtocol = Field<ComboBox>(fixture.page, Localize(L"AIProtocol"));
        if (!expect(back && providerName && providerProtocol)) return;
        auto connection = providerName.Parent().as<Grid>().Parent().as<StackPanel>();
        auto protocolCard = providerProtocol.Parent().as<Grid>().Parent().as<Border>();
        expect(protocolCard.Parent() == connection);
        fixture.root.UpdateLayout();
        Pump([&] { return connection.ActualWidth() > 600; });
        expect(Grid::GetColumn(providerProtocol) == 2_i && Grid::GetRow(providerProtocol) == 0_i);
        auto base = Field<TextBox>(fixture.page, Localize(L"AIBaseUrl"));
        if (!expect(base != nullptr)) return;
        auto key = Field<PasswordBox>(fixture.page, Localize(L"AIKey"));
        expect(key && key.Parent().try_as<StackPanel>() && base.Parent().as<Grid>().Parent() == key.Parent());
        expect(std::abs(base.ActualWidth() - connection.ActualWidth()) < 1.) << "endpoint fills the plain form";
        auto fullUrl = Field<ToggleSwitch>(fixture.page, Localize(L"ChatUseFullUrl"));
        auto catalogUrl = Field<TextBox>(fixture.page, Localize(L"ChatCatalogUrl"));
        if (!expect(fullUrl && catalogUrl)) return;
        auto connectionFields = connection.Children();
        expect(connectionFields.GetAt(connectionFields.Size() - 2) == base.Parent());
        expect(connectionFields.GetAt(connectionFields.Size() - 1) == catalogUrl);
        expect(catalogUrl.Visibility() == Visibility::Visible && !catalogUrl.IsEnabled());
        expect(Description(fixture.page, Localize(L"ChatConnectionOptions")) != nullptr);
        expect(First<Expander>(fixture.page) == nullptr);
        fullUrl.IsOn(true);
        Pump([&] { return catalogUrl.IsEnabled(); });
        catalogUrl.Text(L"https://fixture.invalid/catalog");
        fullUrl.IsOn(false);
        Pump([&] { return !catalogUrl.IsEnabled(); });
        expect(catalogUrl.Visibility() == Visibility::Visible && catalogUrl.Text() == L"https://fixture.invalid/catalog");
        fixture.Resize(510, 740);
        Pump([&] { return connection.ActualWidth() < 490; });
        expect(Grid::GetColumn(providerProtocol) == 2_i && Grid::GetRow(providerProtocol) == 0_i);
        expect(std::abs(protocolCard.ActualWidth() - connection.ActualWidth()) < 1.);
        fixture.Resize(1024, 740);
        Pump([&] { return connection.ActualWidth() > 600; });
        InvokeAction(Action(fixture.page, Localize(L"ChatAddModel")));
        Pump([&] { return Field<TextBox>(fixture.page, Localize(L"ChatModelName")) != nullptr; });
        auto name = Field<TextBox>(fixture.page, Localize(L"ChatModelName"));
        auto upstream = Field<TextBox>(fixture.page, Localize(L"ChatUpstreamModel"));
        auto level = Field<ComboBox>(fixture.page, Localize(L"ChatDefaultEffort"));
        auto none = Effort(fixture.page, L"none");
        auto high = Effort(fixture.page, L"high");
        if (!expect(name && upstream && none && high && level)) return;
        expect(Field<TextBox>(fixture.page, Localize(L"AIBaseUrl")) == nullptr);
        expect(Action(fixture.root, L"Model settings fixture") != nullptr);
        name.Text(L"Manual model");
        upstream.Text(L"custom-model");
        expect(!none.IsChecked().GetBoolean() && !high.IsChecked().GetBoolean());
        expect(!level.IsEnabled());
        none.IsChecked(true);
        Pump([&] { return level.Items().Size() == 2 && level.IsEnabled(); });
        high.IsChecked(true);
        Pump([&] { return level.Items().Size() == 3; });
        level.SelectedIndex(1);
        SaveSettingsImage(fixture.root, "provider-model.png");
        InvokeAction(Action(fixture.page, Localize(L"ChatApplyModel")));
        Pump([&] { return Description(fixture.page, L"custom-model · none, high") != nullptr; });
        expect(std::ranges::none_of(models::Models(), [&](auto const& model) { return model.provider == provider.id; }));
        InvokeAction(Action(fixture.page, Localize(L"ChatSaveProvider")));
        Pump([&] { return std::ranges::any_of(models::Models(), [&](auto const& model) { return model.provider == provider.id; }); });
        auto configured = models::Models();
        auto found = std::ranges::find(configured, provider.id, &models::ConfiguredModel::provider);
        expect(found->upstream == "custom-model");
        expect(found->effort == "none");
        expect(found->ResolveEffort("high") == "high");
        expect(found->ResolveEffort("unsupported") == "none");
        Pump([&] { return Action(fixture.page, L"Model settings fixture") != nullptr; });
        auto reopened = Action(fixture.page, L"Model settings fixture");
        SaveSettingsImage(fixture.root, "provider-list.png");
        InvokeAction(reopened);
        Pump([&] { return Action(fixture.page, L"Manual model") != nullptr; });
        fixture.root.UpdateLayout();
        SaveSettingsImage(fixture.root, "provider-detail.png");
        auto toggle = Field<ToggleSwitch>(fixture.page, LocalizeFormat(L"ChatToggleModel", {L"Manual model"}));
        if (!expect(toggle && toggle.IsOn())) return;
        auto modelLink = Action(fixture.page, L"Manual model");
        expect(modelLink && modelLink.Parent() == toggle.Parent());
        if (!modelLink) return;
        auto modelRow = modelLink.Parent().as<Grid>();
        expect(Grid::GetColumnSpan(modelLink) == 4_i);
        expect(modelRow.Children().GetAt(modelRow.Children().Size() - 1) == toggle);
        expect(modelLink.IsTabStop() && toggle.IsTabStop());
        InvokeAction(modelLink);
        Pump([&] { return Field<TextBox>(fixture.page, Localize(L"ChatModelName")) != nullptr; });
        InvokeAction(Action(fixture.page, Localize(L"Cancel")));
        Pump([&] { return Action(fixture.page, L"Manual model") != nullptr; });
        toggle = Field<ToggleSwitch>(fixture.page, LocalizeFormat(L"ChatToggleModel", {L"Manual model"}));
        if (!expect(toggle && toggle.IsOn())) return;
        toggle.IsOn(false);
        expect(models::Resolve(found->id).first.enabled);
        InvokeAction(Action(fixture.page, Localize(L"ChatSaveProvider")));
        Pump([&] { return Action(fixture.page, L"Model settings fixture") != nullptr; });
        expect(throws([&] { (void)models::Resolve(found->id); }));
        auto allProviders = models::Profiles();
        auto stored = std::ranges::find(allProviders, provider.id, &models::ModelProfile::id);
        if (expect(stored != allProviders.end()) && expect(stored->models.size() == 1_u)) {
            expect(!stored->models.front().enabled);
            expect(stored->models.front().effort == "none");
        }
    };

    "model settings fill only metadata from the upstream model explicitly chosen by the user"_test = [] {
        using namespace winrt::Folia;
        const auto body = R"({"data":[
            {"id":"fixture-one","display_name":"First fixture","max_input_tokens":100000,"max_tokens":4096,
             "capabilities":{"image_input":{"supported":true},"server_tools":{"web_search":{"supported":true}},
             "effort":{"supported":true,"low":{"supported":true},"high":{"supported":true}}}},
            {"id":"fixture-two"}],"has_more":false})";
        completion_tests::Server server(body, 200, false, "application/json", {}, 512, {body});
        models::ModelProfile profile{{}, "Discovery settings fixture", server.baseUrl + "/custom/generate",
            folia::ai::ApiProtocol::AnthropicMessages};
        profile.fullUrl = true;
        profile.catalogUrl = server.baseUrl + "/custom/catalog";
        profile.allowWebSearch = true;
        auto provider = models::SaveProfile(std::move(profile), std::nullopt);
        auto cleanup = wil::scope_exit([&] { models::DeleteProfile(provider.id); });
        ModelSettingsPage fixture(SettingsView::Page::Providers);
        auto card = Action(fixture.page, L"Discovery settings fixture");
        if (!expect(card != nullptr)) return;
        InvokeAction(card);
        Pump([&] { return Action(fixture.page, Localize(L"ChatTestConnection")) != nullptr; });
        auto catalogUrl = Field<TextBox>(fixture.page, Localize(L"ChatCatalogUrl"));
        expect(catalogUrl && catalogUrl.IsEnabled() && catalogUrl.Visibility() == Visibility::Visible);
        SaveSettingsImage(fixture.root, "provider-advanced.png");
        auto test = Action(fixture.page, Localize(L"ChatTestConnection"));
        InvokeAction(test);
        Pump([&] { return server.receivedRequests.load() == 1 && test.IsEnabled(); });
        InvokeAction(Action(fixture.page, Localize(L"ChatAddModel")));
        Pump([&] { return Action(fixture.page, Localize(L"AIFetchModels")) != nullptr; });
        auto fetch = Action(fixture.page, Localize(L"AIFetchModels"));
        auto upstream = Field<TextBox>(fixture.page, Localize(L"ChatUpstreamModel"));
        auto name = Field<TextBox>(fixture.page, Localize(L"ChatModelName"));
        auto context = Field<TextBox>(fixture.page, Localize(L"ChatModelContextLimit"));
        auto output = Field<TextBox>(fixture.page, Localize(L"ChatModelOutputLimit"));
        auto vision = Field<ToggleSwitch>(fixture.page, Localize(L"ChatModelVision"));
        auto search = Field<ToggleSwitch>(fixture.page, Localize(L"ChatModelWebSearch"));
        auto catalog = Field<ComboBox>(fixture.page, Localize(L"ChatChooseUpstream"));
        if (!expect(upstream && name && context && output && vision && search && catalog)) return;
        auto fields = name.Parent().try_as<StackPanel>();
        expect(fields && upstream.Parent() == fields && context.Parent() == fields && output.Parent() == fields);
        InvokeAction(fetch);
        Pump([&] { return catalog.Items().Size() == 2; });
        expect(server.requests.size() == 2_u);
        for (auto const& request : server.requests) expect(request.starts_with("GET /custom/catalog HTTP/1.1"));
        expect(upstream.Text().empty());
        catalog.SelectedIndex(0);
        Pump([&] { return upstream.Text() == L"fixture-one"; });
        expect(name.Text() == L"First fixture");
        expect(context.Text() == L"100000");
        expect(output.Text() == L"4096");
        expect(vision.IsOn());
        expect(search.IsOn());
        expect(Effort(fixture.page, L"high").IsChecked().GetBoolean());
        expect(!Effort(fixture.page, L"max").IsChecked().GetBoolean());
        catalog.SelectedIndex(1);
        Pump([&] { return upstream.Text() == L"fixture-two"; });
        expect(name.Text().empty() && context.Text().empty() && output.Text().empty());
        expect(!vision.IsOn());
        expect(!search.IsOn());
        expect(!Effort(fixture.page, L"high").IsChecked().GetBoolean());
        catalog.SelectedIndex(0);
        InvokeAction(Action(fixture.page, Localize(L"ChatApplyModel")));
        Pump([&] { return Action(fixture.page, L"First fixture") != nullptr; });
        expect(Action(fixture.page, L"fixture-two") == nullptr);
        InvokeAction(Action(fixture.page, Localize(L"ChatSaveProvider")));
        Pump([&] { return Action(fixture.page, L"Discovery settings fixture") != nullptr; });
        auto providers = models::Profiles();
        auto saved = std::ranges::find(providers, provider.id, &models::ModelProfile::id);
        if (!expect(saved != providers.end()) || !expect(saved->models.size() == 1_u)) return;
        expect(saved->models.front().upstream == "fixture-one");
        expect(saved->models.front().efforts.size() == 2_u);
        expect(saved->models.front().maxInputTokens.value_or(0) == 100000_u);
        expect(saved->models.front().vision);
        expect(saved->models.front().webSearch);
        expect(saved->fullUrl && saved->allowWebSearch);
    };

    "provider presets supply editable endpoints and scope model search formats"_test = [] {
        using namespace winrt::Folia;
        ModelSettingsPage fixture(SettingsView::Page::Providers);
        InvokeAction(Action(fixture.page, Localize(L"ChatAddProvider")));
        Pump([&] { return Field<ComboBox>(fixture.page, Localize(L"AIProviderType")) != nullptr; });
        auto type = Field<ComboBox>(fixture.page, Localize(L"AIProviderType"));
        auto protocol = Field<ComboBox>(fixture.page, Localize(L"AIProtocol"));
        auto name = Field<TextBox>(fixture.page, Localize(L"ChatProviderName"));
        auto address = Field<TextBox>(fixture.page, Localize(L"AIBaseUrl"));
        auto allow = Field<ToggleSwitch>(fixture.page, Localize(L"ChatProviderWebSearch"));
        if (!expect(type && protocol && name && address && allow)) return;
        auto select = [](ComboBox const& box, winrt::hstring const& label) {
            for (unsigned i = 0; i < box.Items().Size(); ++i)
                if (winrt::unbox_value<winrt::hstring>(box.Items().GetAt(i)) == label) {
                    box.SelectedIndex(i);
                    return true;
                }
            return false;
        };
        expect(select(type, L"Anthropic"));
        expect(!protocol.IsEnabled() && protocol.Items().Size() == 1_u && name.Text() == L"Anthropic");
        expect(address.Text() == L"https://api.anthropic.com/v1");
        expect(select(type, L"Google Gemini"));
        expect(!allow.IsEnabled() && !protocol.IsEnabled());
        expect(select(type, L"DeepSeek"));
        expect(allow.IsEnabled() && protocol.IsEnabled() && protocol.Items().Size() == 3_u);
        expect(name.Text() == L"DeepSeek");
        name.Text(L"Proxy settings fixture");
        address.Text(L"https://gateway.example.test/v1");
        expect(select(type, L"OpenAI"));
        expect(select(type, L"DeepSeek"));
        expect(name.Text() == L"Proxy settings fixture" && address.Text() == L"https://gateway.example.test/v1");
        allow.IsOn(true);
        fixture.root.UpdateLayout();
        SaveSettingsImage(fixture.root, "preset-connection.png");
        InvokeAction(Action(fixture.page, Localize(L"ChatAddModel")));
        Pump([&] { return Field<ToggleSwitch>(fixture.page, Localize(L"ChatModelWebSearch")) != nullptr; });
        auto search = Field<ToggleSwitch>(fixture.page, Localize(L"ChatModelWebSearch"));
        auto format = Field<ComboBox>(fixture.page, Localize(L"AISearchFormat"));
        if (!expect(search && format)) return;
        expect(!format.IsEnabled());
        search.IsOn(true);
        expect(format.IsEnabled() && format.Items().Size() == 3_u);
        expect(select(format, Localize(L"AISearchFormatStandard")));
        Field<TextBox>(fixture.page, Localize(L"ChatUpstreamModel")).Text(L"aliased-model");
        InvokeAction(Action(fixture.page, Localize(L"ChatApplyModel")));
        Pump([&] { return Action(fixture.page, L"aliased-model") != nullptr; });
        InvokeAction(Action(fixture.page, Localize(L"ChatSaveProvider")));
        Pump([&] { return Action(fixture.page, L"Proxy settings fixture") != nullptr; });
        auto configured = models::Profiles();
        auto saved = std::ranges::find(configured, "Proxy settings fixture", &models::ModelProfile::name);
        if (!expect(saved != configured.end())) return;
        auto cleanup = wil::scope_exit([&] { models::DeleteProfile(saved->id); });
        expect(saved->providerType == "deepseek" && saved->protocol == models::ApiProtocol::Responses);
        expect(saved->models.size() == 1_u && models::HostedSearchFormat(saved->models.front(), *saved) == folia::ai::WebSearchDialect::Standard);
    };

    "provider connection check cancels when the configured credentials change"_test = [] {
        using namespace winrt::Folia;
        completion_tests::Server server({}, 200, true, "application/json");
        ModelSettingsPage fixture(SettingsView::Page::Providers);
        InvokeAction(Action(fixture.page, Localize(L"ChatAddProvider")));
        Pump([&] { return Field<TextBox>(fixture.page, Localize(L"AIBaseUrl")) != nullptr; });
        Field<TextBox>(fixture.page, Localize(L"AIBaseUrl")).Text(winrt::to_hstring(server.baseUrl));
        Field<PasswordBox>(fixture.page, Localize(L"AIKey")).Password(L"fixture-key");
        auto fetch = Action(fixture.page, Localize(L"ChatTestConnection"));
        InvokeAction(fetch);
        Pump([&] { return server.received.load(); });
        expect(!fetch.IsEnabled());
        Field<PasswordBox>(fixture.page, Localize(L"AIKey")).Password(L"changed-key");
        Pump([&] { return server.client.load() == INVALID_SOCKET; });
        expect(fetch.IsEnabled());
    };

    "settings pages share typography and render AI forms without enclosing borders"_test = [] {
        auto provider = winrt::Folia::models::SaveProfile(
            {{}, "Theme settings fixture", "https://fixture.invalid/v1"}, std::nullopt);
        auto removeProvider = wil::scope_exit([&] { winrt::Folia::models::DeleteProfile(provider.id); });
        Window window; Grid root; window.Content(root); window.Title(L"Folia settings style test");
        ContentControl pageHost;
        auto view = std::make_shared<winrt::Folia::SettingsView>(pageHost, winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; });
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(pageHost); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        using Page = winrt::Folia::SettingsView::Page;
        for (auto category : {Page::Home, Page::General, Page::Themes, Page::Shortcuts, Page::Latex, Page::Completion, Page::Providers, Page::Licenses, Page::About}) {
            view->Navigate(category); root.UpdateLayout();
            auto page = view->PageContent().as<FrameworkElement>();
            auto heading = First<TextBlock>(page); expect(heading != nullptr);
            if (heading) expect(heading.FontSize() == 24.0) << "page " << static_cast<int>(category);
        }
        for (auto theme : {ElementTheme::Light, ElementTheme::Dark}) {
            auto profile = winrt::Folia::LoadThemeProfile(theme == ElementTheme::Light ? folia::Theme::Light : folia::Theme::Dark).profile;
            auto const& colors = profile.colors;
            auto brush = [&](wchar_t const* key, folia::Color color) {
                winrt::Windows::UI::Color value{color.a, color.r, color.g, color.b};
                auto resources = root.Resources();
                if (resources.HasKey(winrt::box_value(key))) resources.Lookup(winrt::box_value(key)).as<Media::SolidColorBrush>().Color(value);
                else resources.Insert(winrt::box_value(key), Media::SolidColorBrush(value));
            };
            brush(L"FoliaShellForegroundBrush", colors.shell_fg); brush(L"FoliaShellMutedForegroundBrush", colors.shell_muted_fg);
            brush(L"FoliaShellAccentBrush", colors.shell_accent); brush(L"FoliaShellLayerBrush", colors.shell_layer_bg);
            brush(L"FoliaShellBorderBrush", colors.shell_border);
            root.Resources().Insert(winrt::box_value(L"FoliaUiFontFamily"), Media::FontFamily(winrt::to_hstring(profile.typography.ui.family)));
            root.Resources().Insert(winrt::box_value(L"FoliaUiFontSize"), winrt::box_value(double(profile.typography.ui.size)));
            root.Resources().Insert(winrt::box_value(L"FoliaMonoFontFamily"), Media::FontFamily(winrt::to_hstring(profile.typography.ui_monospace.family)));
            root.Background(Media::SolidColorBrush({colors.shell_bg.a, colors.shell_bg.r, colors.shell_bg.g, colors.shell_bg.b}));
            root.RequestedTheme(theme);
            Pump([&] { return root.ActualTheme() == theme; }); root.UpdateLayout();
            view->Navigate(Page::Providers);
            for (int width : {1120, 720}) {
                HWND handle{}; winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
                auto scale = double(GetDpiForWindow(handle)) / 96.;
                window.AppWindow().Resize({static_cast<int>(width * scale), static_cast<int>(900 * scale)});
                Pump([&] { return std::abs(root.ActualWidth() - width) < 20.; }); root.UpdateLayout();
                auto scroll = view->PageContent().as<ScrollViewer>();
                auto panel = First<StackPanel>(scroll.Content().as<UIElement>());
                for (auto child : panel.Children()) expect(!child.try_as<Border>()) << "AI sections must not use a page-sized frame";
                auto description = Description(panel, winrt::Folia::Localize(L"ChatProvidersDescription"));
                expect(description != nullptr);
                if (description) {
                    auto expected = root.Resources().Lookup(winrt::box_value(L"FoliaShellMutedForegroundBrush")).as<Media::SolidColorBrush>();
                    expect(description.Foreground().as<Media::SolidColorBrush>().Color() == expected.Color()) << "description theme brush";
                }
                auto add = Action(panel, winrt::Folia::Localize(L"ChatAddProvider"));
                expect(add && add.HorizontalAlignment() == HorizontalAlignment::Right);
                if (add) {
                    auto label = Description(add, winrt::Folia::Localize(L"ChatAddProvider"));
                    auto icon = First<Shapes::Path>(add);
                    if (expect(label && icon))
                        expect(label.Foreground().as<Media::SolidColorBrush>().Color() ==
                            icon.Stroke().as<Media::SolidColorBrush>().Color());
                }
                expect(Action(panel, L"Theme settings fixture") != nullptr);
                expect(Field<TextBox>(panel, winrt::Folia::Localize(L"AIBaseUrl")) == nullptr);
                auto name = std::string(theme == ElementTheme::Light ? "light-" : "dark-") + std::to_string(width) + ".png";
                SaveSettingsImage(root, name);
            }
        }
    };
};
}
