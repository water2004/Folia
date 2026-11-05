#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include "localization/Localization.h"
#include "settings/SettingsView.h"
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
    if (auto value = parent.try_as<T>(); value && value.Header()
        && winrt::unbox_value<winrt::hstring>(value.Header()) == label) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Field<T>(Media::VisualTreeHelper::GetChild(parent, i), label)) return found;
    return nullptr;
}
Button Action(DependencyObject const& parent, winrt::hstring const& label) {
    if (auto button = parent.try_as<Button>(); button != nullptr) {
        auto value = button.Content().try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (value != nullptr && value.Type() == winrt::Windows::Foundation::PropertyType::String)
            if (value.GetString() == label) return button;
    }
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Action(Media::VisualTreeHelper::GetChild(parent, i), label)) return found;
    return nullptr;
}
void InvokeAction(Button const& button) {
    auto peer = Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(button);
    peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
}
void FetchModels(Button const& button, completion_tests::Server& server) {
    auto previous = server.receivedRequests.load();
    button.StartBringIntoView();
    Pump([&] { button.UpdateLayout(); return button.ActualWidth() > 0; });
    bool ready = false;
    button.DispatcherQueue().TryEnqueue([&] { ready = true; });
    Pump([&] { return ready; });
    bool clicked = false;
    auto click = button.Click(winrt::auto_revoke, [&](auto const&, auto const&) { clicked = true; });
    InvokeAction(button);
    try { Pump([&] { return clicked; }); }
    catch (std::exception const&) { throw std::runtime_error("Model fetch did not receive its native Button.Click event."); }
    try { Pump([&] { return server.receivedRequests.load() > previous; }); }
    catch (std::exception const&) { throw std::runtime_error("Model fetch " + std::to_string(previous + 1)
        + " did not reach the local HTTP fixture; enabled=" + std::to_string(button.IsEnabled())
        + ", label=" + winrt::to_string(winrt::unbox_value<winrt::hstring>(button.Content()))
        + ", status=" + winrt::to_string(button.Parent().as<Grid>().Parent().as<StackPanel>().Children().GetAt(
            button.Parent().as<Grid>().Parent().as<StackPanel>().Children().Size() - 1).as<TextBlock>().Text())); }
    try { Pump([&] { return button.IsEnabled(); }); }
    catch (std::exception const&) { throw std::runtime_error("Model fetch did not return its UI completion."); }
}
struct CompletionPage {
    Window window;
    Grid root;
    std::shared_ptr<winrt::Folia::SettingsView> view;
    FrameworkElement page{nullptr};
    CompletionPage(std::string base, std::string model = {}, std::string effort = {}) {
        winrt::Folia::AppSettings initial;
        initial.completion = {true, std::move(base), std::move(model), 0, std::move(effort)};
        view = std::make_shared<winrt::Folia::SettingsView>(initial,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
        window.Content(root); root.Children().Append(view->Root()); window.AppWindow().Resize({1120, 900}); window.Activate();
        Pump([&] { return root.IsLoaded() && root.ActualWidth() > 100; });
        for (auto entry : view->Root().MenuItems())
            if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"completion") view->Root().SelectedItem(entry);
        root.UpdateLayout(); page = view->Root().Content().as<FrameworkElement>();
        Pump([&] { root.UpdateLayout(); return First<TextBlock>(page).ActualWidth() > 0; });
        Field<PasswordBox>(page, winrt::Folia::Localize(L"AIKey")).Password(L"fixture-key");
    }
    ~CompletionPage() { view->Detach(); window.Close(); }
};
suite settings_style_tests = [] {
    "model refresh preserves an available saved choice and never switches to an unavailable replacement"_test = [] {
        completion_tests::Server server(R"({"data":[{"id":"fixture","effort":{"supported_levels":["low","high"]}},{"id":"other"}]})",
            200, false, "application/json", {}, 37, {R"({"data":[{"id":"other"}]})"});
        CompletionPage fixture(server.baseUrl, "fixture", "high");
        auto model = Field<ComboBox>(fixture.page, winrt::Folia::Localize(L"AIModel"));
        auto reasoning = Field<ComboBox>(fixture.page, winrt::Folia::Localize(L"AIReasoning"));
        auto fetch = Action(fixture.page, winrt::Folia::Localize(L"AIFetchModels"));
        expect(model.Items().Size() == 1_u && model.SelectedIndex() == 0_i);
        FetchModels(fetch, server);
        expect(model.Items().Size() == 2_u && model.SelectedIndex() == 0_i);
        expect(winrt::unbox_value<winrt::hstring>(reasoning.SelectedItem().as<ComboBoxItem>().Tag()) == L"high");
        FetchModels(fetch, server);
        auto service = fetch.Parent().as<Grid>().Parent().as<StackPanel>();
        auto fetchStatus = service.Children().GetAt(service.Children().Size() - 1).as<TextBlock>();
        expect(model.Items().Size() == 1_u && model.SelectedIndex() == -1_i) << winrt::to_string(fetchStatus.Text());
        expect(reasoning.Items().Size() == 1_u && reasoning.SelectedIndex() == 0_i);
    };
    "model discovery empty and failed responses remain selectable only by explicit available choices"_test = [] {
        for (auto status : {200, 503}) {
            completion_tests::Server server(R"({"data":[]})", status, false, "application/json");
            CompletionPage fixture(server.baseUrl);
            auto model = Field<ComboBox>(fixture.page, winrt::Folia::Localize(L"AIModel"));
            auto fetch = Action(fixture.page, winrt::Folia::Localize(L"AIFetchModels"));
            FetchModels(fetch, server);
            expect(server.received.load()); expect(model.Items().Size() == 0_u);
            expect(model.SelectedIndex() == -1_i);
            expect(Field<ComboBox>(fixture.page, winrt::Folia::Localize(L"AIReasoning")).Items().Size() == 1_u);
        }
    };
    "leaving completion settings cancels pending model discovery"_test = [] {
        completion_tests::Server server({}, 200, true, "application/json");
        CompletionPage fixture(server.baseUrl);
        auto fetch = Action(fixture.page, winrt::Folia::Localize(L"AIFetchModels"));
        InvokeAction(fetch); Pump([&] { return server.received.load(); });
        expect(!fetch.IsEnabled());
        fixture.view->Root().SelectedItem(fixture.view->Root().MenuItems().GetAt(0));
        Pump([&] { return server.client.load() == INVALID_SOCKET; });
        expect(fetch.IsEnabled());
    };
    "actual credential edits cancel pending model discovery"_test = [] {
        for (bool clear : {false, true}) {
            completion_tests::Server server({}, 200, true, "application/json");
            CompletionPage fixture(server.baseUrl);
            auto fetch = Action(fixture.page, winrt::Folia::Localize(L"AIFetchModels"));
            InvokeAction(fetch); Pump([&] { return server.received.load(); });
            expect(!fetch.IsEnabled());
            if (clear) First<CheckBox>(fixture.page).IsChecked(true);
            else Field<PasswordBox>(fixture.page, winrt::Folia::Localize(L"AIKey")).Password(L"changed-fixture-key");
            Pump([&] { return server.client.load() == INVALID_SOCKET; });
            expect(fetch.IsEnabled());
        }
    };
    "completion metadata requests retire when settings service changes"_test = [] {
        completion_tests::Server server({}, 200, true, "application/json");
        Window window; Grid root; window.Content(root);
        winrt::Folia::AppSettings initial; initial.completion.baseUrl = server.baseUrl; initial.completion.model = "fixture";
        auto view = std::make_shared<winrt::Folia::SettingsView>(initial,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.Activate(); Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        for (auto entry : navigation.MenuItems())
            if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"completion") navigation.SelectedItem(entry);
        root.UpdateLayout(); auto page = navigation.Content().as<FrameworkElement>();
        Field<PasswordBox>(page, winrt::Folia::Localize(L"AIKey")).Password(L"fixture-key");
        auto fetch = Action(page, winrt::Folia::Localize(L"AIFetchModels")); InvokeAction(fetch);
        Pump([&] { return server.received.load(); }); expect(!fetch.IsEnabled());
        Field<TextBox>(page, winrt::Folia::Localize(L"AIBaseUrl")).Text(L"https://another.example/v1");
        Pump([&] { return fetch.IsEnabled(); });
        Pump([&] { return server.client.load() == INVALID_SOCKET; });
        expect(Field<ComboBox>(page, winrt::Folia::Localize(L"AIReasoning")).Items().Size() == 1_u);
    };
    "completion settings save independent budgets eagerness and protocol reasoning choices"_test = [] {
        completion_tests::Server server(R"({"data":[{"id":"fixture","name":"Fixture Model","effort":{"supported_levels":["low","high","max"]}},{"id":"plain"}]})", 200, false, "application/json");
        Window window; Grid root; window.Content(root);
        std::optional<winrt::Folia::AppSettings> applied;
        winrt::Folia::AppSettings initial;
        initial.completion.baseUrl = server.baseUrl; initial.completion.model = "fixture";
        auto view = std::make_shared<winrt::Folia::SettingsView>(initial,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [&](auto const& settings) -> std::optional<winrt::hstring> { applied = settings; return {}; }, [] {});
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.Activate(); Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        for (auto entry : navigation.MenuItems())
            if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"completion") navigation.SelectedItem(entry);
        root.UpdateLayout(); auto page = navigation.Content().as<FrameworkElement>();
        auto protocol = Field<ComboBox>(page, winrt::Folia::Localize(L"AIProtocol"));
        auto reasoning = Field<ComboBox>(page, winrt::Folia::Localize(L"AIReasoning"));
        auto eagerness = Field<ComboBox>(page, winrt::Folia::Localize(L"AIEagerness"));
        auto context = Field<TextBox>(page, winrt::Folia::Localize(L"AIContextKB"));
        auto output = Field<TextBox>(page, winrt::Folia::Localize(L"AIOutputTokens"));
        expect(protocol != nullptr && reasoning != nullptr && eagerness != nullptr && context != nullptr && output != nullptr);
        expect(context.Text() == L"7" && output.Text() == L"128");
        expect(eagerness.SelectedIndex() == 1_i);
        expect(reasoning.Items().Size() == 1_u) << "there must be no preset reasoning levels";
        protocol.SelectedIndex(2); expect(reasoning.SelectedIndex() == 0_i); expect(reasoning.Items().Size() == 1_u);
        auto key = Field<PasswordBox>(page, winrt::Folia::Localize(L"AIKey")); key.Password(L"fixture-key");
        auto fetch = Action(page, winrt::Folia::Localize(L"AIFetchModels")); expect(fetch != nullptr);
        FetchModels(fetch, server);
        auto model = Field<ComboBox>(page, winrt::Folia::Localize(L"AIModel"));
        expect(!model.IsEditable()); expect(model.Items().Size() == 2_u);
        expect(model.SelectedIndex() == -1_i) << "fetch must not silently select a new model";
        model.SelectedIndex(0); expect(reasoning.Items().Size() == 4_u);
        model.SelectedIndex(1); expect(reasoning.Items().Size() == 1_u);
        model.SelectedIndex(0); expect(reasoning.Items().Size() == 4_u);
        expect(fetch.IsEnabled()); expect(server.request.starts_with("GET /v1/models HTTP/1.1\r\n"));
        reasoning.SelectedIndex(2); // high from the upstream fixture
        key.Password(L""); // Do not persist fixture credentials when applying settings.
        eagerness.SelectedIndex(2); context.Text(L"16"); output.Text(L"512");
        auto save = Action(page, winrt::Folia::Localize(L"Save")); expect(save != nullptr);
        InvokeAction(save);
        Pump([&] { return applied.has_value(); });
        expect(applied->completion.contextKB == 16_u && applied->completion.outputTokens == 512_u);
        expect(applied->completion.reasoningEffort == "high");
        expect(applied->completion.model == "fixture") << "save uses model ID, not its display name";
        expect(applied->completion.eagerness == folia::completion::Eagerness::Proactive);
        view->Reset(*applied, folia::Theme::Light);
        root.UpdateLayout(); page = navigation.Content().as<FrameworkElement>();
        expect(Field<TextBox>(page, winrt::Folia::Localize(L"AIContextKB")).Text() == L"16");
        expect(Field<ComboBox>(page, winrt::Folia::Localize(L"AIModel")).SelectedIndex() == 0_i);
    };
    "settings pages share typography and render AI forms without enclosing borders"_test = [] {
        Window window; Grid root; window.Content(root); window.Title(L"Folia settings style test");
        auto view = std::make_shared<winrt::Folia::SettingsView>(winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        for (auto entry : navigation.MenuItems()) {
            navigation.SelectedItem(entry); root.UpdateLayout();
            auto page = navigation.Content().as<FrameworkElement>();
            auto heading = First<TextBlock>(page); expect(heading != nullptr);
            if (heading) expect(heading.FontSize() == 24.0) << "page " << winrt::to_string(winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()));
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
            for (auto entry : navigation.MenuItems())
                if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"completion") navigation.SelectedItem(entry);
            for (int width : {1120, 720}) {
                HWND handle{}; winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
                auto scale = double(GetDpiForWindow(handle)) / 96.;
                window.AppWindow().Resize({static_cast<int>(width * scale), static_cast<int>(900 * scale)});
                Pump([&] { return std::abs(root.ActualWidth() - width) < 20.; }); root.UpdateLayout();
                auto scroll = navigation.Content().as<ScrollViewer>();
                auto panel = First<StackPanel>(scroll.Content().as<UIElement>());
                for (auto child : panel.Children()) expect(!child.try_as<Border>()) << "AI sections must not use a page-sized frame";
                auto description = Description(panel, winrt::Folia::Localize(L"AICompletionDescription"));
                expect(description != nullptr);
                if (description) {
                    auto expected = root.Resources().Lookup(winrt::box_value(L"FoliaShellMutedForegroundBrush")).as<Media::SolidColorBrush>();
                    expect(description.Foreground().as<Media::SolidColorBrush>().Color() == expected.Color()) << "description theme brush";
                }
                auto endpoint = Field<TextBox>(panel, winrt::Folia::Localize(L"AIBaseUrl"));
                auto key = Field<PasswordBox>(panel, winrt::Folia::Localize(L"AIKey"));
                expect(endpoint != nullptr && key != nullptr);
                if (endpoint && key) {
                    expect(endpoint.ActualWidth() > 100.0) << "endpoint visible width " << endpoint.ActualWidth();
                    expect(std::abs(endpoint.ActualWidth() - key.ActualWidth()) < 1.0) << "aligned field widths";
                    auto a = endpoint.TransformToVisual(panel).TransformPoint({0, 0});
                    auto b = key.TransformToVisual(panel).TransformPoint({0, 0});
                    expect(std::abs(a.X - b.X) < 1.0);
                    expect(a.X + endpoint.ActualWidth() <= panel.ActualWidth() + 1.0);
                }
                // Settle native NavigationView selection/theme animations for
                // the artifact; functional layout assertions above are immediate.
                auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                Pump([&] { return std::chrono::steady_clock::now() >= settled; });
                Media::Imaging::RenderTargetBitmap image; Await(image.RenderAsync(root));
                auto pixels = Await(image.GetPixelsAsync());
                auto name = std::string(theme == ElementTheme::Light ? "light-" : "dark-") + std::to_string(width) + ".png";
                visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/settings-styles" / name, image.PixelWidth(), image.PixelHeight(), pixels);
            }
        }
    };
};
}
