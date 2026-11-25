#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "settings/views/SettingsView.h"
#include "visual_window.h"
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    template<class T> T First(DependencyObject const& element, std::wstring_view name = {}) {
        if (auto value = element.try_as<T>(); value && (name.empty() || value.Name() == name)) return value;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto value = First<T>(Media::VisualTreeHelper::GetChild(element, i), name)) return value;
        return nullptr;
    }

    suite settings_width_tests = [] {
        "theme import requires both a valid profile and a PNG preview"_test = [] {
            TestDirectory directory;
            auto const profilePath = winrt::Folia::BuiltinThemeProfilePath(folia::Theme::Light);
            std::ifstream input(profilePath, std::ios::binary);
            expect(fatal(bool(input)));
            std::string profile{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
            auto id = directory.path.filename().string();
            id = "folia-test-" + id.substr(1, id.size() - 2);
            profile.replace(profile.find("folia.light"), std::string_view("folia.light").size(), id);
            auto source = directory.path / L"sample.json";
            auto preview = directory.path / L"sample.png";
            std::ofstream(source, std::ios::binary) << profile;
            winrt::Folia::ThemeCatalog catalog;
            catalog.Refresh();
            std::string imported;
            expect(catalog.Import(source, imported).has_value());
            std::ofstream(preview, std::ios::binary) << "not a PNG";
            expect(catalog.Import(source, imported).has_value());
            auto builtInPreview = profilePath;
            std::filesystem::copy_file(builtInPreview.replace_extension(L".png"), preview,
                std::filesystem::copy_options::overwrite_existing);
            auto error = catalog.Import(source, imported);
            expect(!error) << (error ? winrt::to_string(*error) : std::string{});
            if (error) return;
            struct Cleanup {
                winrt::Folia::ThemeCatalog& catalog;
                std::string const& id;
                ~Cleanup() { catalog.Remove(id); }
            } cleanup{catalog, imported};
            expect(imported == id);
            expect(std::filesystem::exists(catalog.Preview(id, folia::Theme::Light)));
            expect(catalog.Find(id) && !catalog.Find(id)->builtIn);
        };
        "settings center the shared content column and keep native list scrollbars at the page edge"_test = [] { try {
            Window window;
            window.Title(L"Folia settings layout test");
            Grid host, root;
            host.Children().Append(root);
            window.Content(host);
            root.RequestedTheme(ElementTheme::Light);
            root.Background(Application::Current().Resources().Lookup(
                winrt::box_value(L"ApplicationPageBackgroundThemeBrush")).as<Media::Brush>());
            // Exercise wide logical viewports independently of the monitor size.
            root.HorizontalAlignment(HorizontalAlignment::Left);
            root.VerticalAlignment(VerticalAlignment::Top);
            root.Width(1120);
            root.Height(800);
            ContentControl pageHost;
            auto view = std::make_shared<winrt::Folia::SettingsView>(pageHost, winrt::Folia::AppSettings{},
                std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
                folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; });
            struct Cleanup {
                Window window;
                std::shared_ptr<winrt::Folia::SettingsView> view;
                ~Cleanup() { view->Detach(); window.Close(); }
            } cleanup{window, view};
            root.Children().Append(pageHost);
            window.Activate();
            Pump([&] { return root.IsLoaded(); });
            using Page = winrt::Folia::SettingsView::Page;
            constexpr std::pair<Page, std::string_view> pages[] = {
                {Page::Home, "home"}, {Page::General, "general"}, {Page::Themes, "themes"}, {Page::Shortcuts, "shortcuts"},
                {Page::Latex, "latex"}, {Page::Completion, "completion"}, {Page::Licenses, "licenses"}, {Page::About, "about"},
            };
            for (auto [category, name] : pages) {
                view->Navigate(category);
                for (const auto width : {1120, 1800, 2400, 720, 1120}) {
                    root.Width(width);
                    root.UpdateLayout();
                    expect(std::abs(root.ActualWidth() - width) < 1.) << name << width;
                    auto page = view->PageContent().as<FrameworkElement>();
                    auto content = First<ContentPresenter>(page, L"PageContent");
                    expect(fatal(content != nullptr));
                    const auto origin = content.TransformToVisual(page).TransformPoint({0, 0});
                    expect(std::abs(content.ActualWidth() - (std::min)(1008., page.ActualWidth())) < 2.) << name << width;
                    expect(std::abs(origin.X + content.ActualWidth() / 2 - page.ActualWidth() / 2) < 2.) << name << width;
                    if (name == "shortcuts" || name == "latex") {
                        auto list = First<ListView>(page);
                        Pump([&] { return list.ContainerFromIndex(0) != nullptr; });
                        auto scroll = First<ScrollViewer>(list);
                        expect(fatal(scroll != nullptr));
                        const auto scrollPoint = scroll.TransformToVisual(page).TransformPoint({0, 0});
                        expect(std::abs(scrollPoint.X) < 2.);
                        expect(std::abs(scrollPoint.X + scroll.ActualWidth() - page.ActualWidth()) < 2.) << name;
                        expect(scroll.ActualHeight() > page.ActualHeight() - 48.) << name;
                        auto row = list.ContainerFromIndex(0).as<ListViewItem>().ContentTemplateRoot().as<ContentPresenter>();
                        const auto rowPoint = row.TransformToVisual(page).TransformPoint({0, 0});
                        expect(std::abs(row.ActualWidth() - content.ActualWidth() + 48.) < 2.) << name << width;
                        expect(std::abs(rowPoint.X - origin.X - 24.) < 2.) << name << width;
                    }
                }
                if (name == "themes") {
                    root.Width(1800);
                    root.Height(700);
                    root.UpdateLayout();
                    auto page = view->PageContent().as<FrameworkElement>();
                    auto preview = First<winrt::Microsoft::UI::Xaml::Controls::Image>(page);
                    expect(fatal(preview != nullptr));
                    Pump([&] { auto bitmap = preview.Source().try_as<Media::Imaging::BitmapImage>(); return bitmap && bitmap.PixelWidth() > 0; });
                    expect(preview.Stretch() == Media::Stretch::Uniform) << "previews preserve the image aspect ratio";
                    auto picker = First<ComboBox>(page);
                    expect(fatal(picker && picker.ActualWidth() <= 241.));
                    picker.IsDropDownOpen(true);
                    auto popup = picker.as<winrt::Microsoft::UI::Xaml::Controls::IControlProtected>()
                        .GetTemplateChild(L"PopupBorder").as<Border>();
                    auto popupBrush = popup.Background().try_as<Media::SolidColorBrush>();
                    expect(popupBrush && popupBrush.Color().A == 255 && popupBrush.Opacity() == 1.) << "theme popup has an opaque surface";
                    picker.IsDropDownOpen(false);
                    auto button = First<Button>(page);
                    expect(fatal(button != nullptr));
                    for (const auto width : {1800, 720, 1800}) {
                        root.Width(width);
                        for (const auto height : {700, 480, 360, 700}) {
                            root.Height(height);
                            root.UpdateLayout();
                            for (auto const& action : button.Parent().as<StackPanel>().Children()) {
                                auto control = action.as<Button>();
                                const auto bounds = control.TransformToVisual(page).TransformBounds({0, 0,
                                    static_cast<float>(control.ActualWidth()), static_cast<float>(control.ActualHeight())});
                                expect(bounds.X >= 0 && bounds.X + bounds.Width <= page.ActualWidth() + 1.) << "theme actions fit horizontally";
                                expect(bounds.Y >= 0 && bounds.Y + bounds.Height <= page.ActualHeight() + 1.)
                                    << "theme actions leave the viewport at " << width << 'x' << height;
                            }
                        }
                    }
                }
                if (name == "licenses") {
                    auto page = view->PageContent().as<FrameworkElement>();
                    auto text = First<RichEditBox>(page);
                    auto selector = First<ComboBox>(page);
                    expect(fatal(text != nullptr && selector != nullptr));
                    expect(text.IsReadOnly()) << "license text is read-only";
                    auto peer = Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(text);
                    expect(fatal(peer != nullptr));
                    expect(!peer.GetName().empty()) << "license document has an accessibility name";
                    auto content = [&] {
                        winrt::hstring value;
                        text.Document().GetText(winrt::Microsoft::UI::Text::TextGetOptions::None, value);
                        return value;
                    };
                    expect(std::wstring_view{content()}.starts_with(L"MIT License")) << "MIT license is the initial document";
                    selector.SelectedIndex(1);
                    expect(content().size() > 1'000'000u) << "the bundled notices are loaded in full";
                    selector.SelectedIndex(0);
                    expect(std::wstring_view{content()}.starts_with(L"MIT License")) << "switching back restores the MIT license";
                }
            }
        } catch (winrt::hresult_error const& error) {
            throw std::runtime_error(winrt::to_string(error.message()) + " HRESULT " + std::to_string(error.code().value));
        } };
    };
}
