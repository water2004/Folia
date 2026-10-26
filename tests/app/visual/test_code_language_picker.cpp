#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "editor/interaction/EditorCodeLanguageController.h"
#include "visual_window.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    using document_tests::Pump;

    suite code_language_picker = [] {
        "code language picker opens explicitly commits transactionally and closes on scrolling"_test = [] {
            auto window = visual_tests::Window();
            auto root = window.Content().as<Grid>();
            Grid host;
            host.Width(600); host.Height(300);
            host.HorizontalAlignment(HorizontalAlignment::Left);
            host.VerticalAlignment(VerticalAlignment::Top);
            host.Margin({20, 100, 0, 0});
            Grid::SetRowSpan(host, 6); Canvas::SetZIndex(host, 100);
            SwapChainPanel panel;
            host.Children().Append(panel);
            root.Children().Append(host);
            struct RemoveHost {
                Grid root, host;
                ~RemoveHost() { std::uint32_t index = 0; if (root.Children().IndexOf(host, index)) root.Children().RemoveAt(index); }
            } remove{root, host};
            root.UpdateLayout();
            Pump([&] { return panel.IsLoaded() && panel.ActualWidth() > 100; });
            EditorSession session;
            auto theme = folia::default_theme_profile(root.ActualTheme() == ElementTheme::Dark ? folia::Theme::Dark : folia::Theme::Light);
            session.SetTheme(theme);
            auto source = std::wstring{L"```cpp\n"};
            for (int i = 0; i < 50; ++i) source += L"value\n";
            source += L"```\n";
            session.SetText(winrt::hstring{source});
            auto original = session.Text();
            auto owner = session.RenderModel().blocks.front().id;
            session.SetSelection(folia::TextSelection::caret({owner, 7, folia::TextAffinity::Downstream}));
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel); renderer.SetTheme(theme);
            renderer.Resize(panel, 600, 300); renderer.Render(session.RenderFrame());
            auto content = root.Resources().Lookup(winrt::box_value(L"CodeLanguagePickerTemplate"))
                .as<DataTemplate>().LoadContent().as<Grid>();
            auto box = content.FindName(L"Search").as<TextBox>();
            auto languages = content.FindName(L"Languages").as<ListView>();
            Flyout flyout;
            flyout.FlyoutPresenterStyle(root.Resources().Lookup(winrt::box_value(L"CodeLanguagePickerPresenterStyle")).as<Style>());
            flyout.Content(content);
            auto opened = std::make_shared<unsigned>(0);
            auto closed = std::make_shared<unsigned>(0);
            flyout.Opened([opened](auto const&, auto const&) { ++*opened; std::fprintf(stderr, "language picker event=Opened\n"); });
            flyout.Closing([](auto const&, auto const&) { std::fprintf(stderr, "language picker event=Closing\n"); });
            flyout.Closed([closed](auto const&, auto const&) { ++*closed; std::fprintf(stderr, "language picker event=Closed\n"); });
            EditorCodeLanguageController controller;
            std::size_t commands = 0;
            controller.Attach(session, renderer, panel, flyout, box, languages, [&](auto const& command) {
                ++commands;
                auto changed = session.ExecuteCommand(command);
                renderer.Render(session.RenderFrame());
                controller.Refresh();
                return changed;
            });
            struct Detach { EditorCodeLanguageController& controller; ~Detach() { controller.Detach(); } } detach{controller};
            controller.Refresh();
            expect(!flyout.IsOpen());
            expect(controller.Open(owner));
            std::fprintf(stderr, "language picker stage=first open\n");
            Pump([&] { return *opened >= 1 && flyout.IsOpen() && box.IsLoaded(); });
            expect(box.Text().empty()) << "the current language belongs to the header, not a prefilled query";
            expect(winrt::unbox_value<winrt::hstring>(languages.SelectedItem()) == L"cpp");
            expect(box.FontSize() == 14);
            expect(box.ActualHeight() == 32);
            expect(languages.ActualHeight() <= 224);
            // Capture the settled native item transitions, rather than their
            // first transparent entrance frame. This is test-only UI pumping.
            auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settled; });
            auto popups = Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot());
            expect(popups.Size() == 1_u) << "language choices must stay in one popup";
            if (popups.Size() == 1) {
                auto presenter = popups.GetAt(0).Child().as<FrameworkElement>();
                expect(presenter.ActualWidth() <= 280 && presenter.ActualHeight() <= 290);
                Media::Imaging::RenderTargetBitmap bitmap;
                document_tests::Await(bitmap.RenderAsync(content, static_cast<int>(std::ceil(content.ActualWidth())),
                    static_cast<int>(std::ceil(content.ActualHeight()))));
                auto pixels = document_tests::Await(bitmap.GetPixelsAsync());
                // Popup roots cannot be captured by RenderTargetBitmap. Keep
                // the actual focused content pixels and composite its transparent
                // areas onto the presenter's resolved opaque fallback color.
                // This fixture excludes compositor blur/shadow, not layout/text.
                auto color = renderer.Theme().colors.shell_layer_bg;
                auto brush = presenter.as<Control>().Background();
                if (auto solid = brush.try_as<Media::SolidColorBrush>()) {
                    auto c = solid.Color(); color = folia::Color(c.R, c.G, c.B);
                } else if (auto acrylic = brush.try_as<Media::AcrylicBrush>()) {
                    auto c = acrylic.FallbackColor(); color = folia::Color(c.R, c.G, c.B);
                }
                for (std::uint32_t offset = 0; offset + 3 < pixels.Length(); offset += 4) {
                    auto p = pixels.data() + offset;
                    const auto transparent = 255 - p[3];
                    p[0] = static_cast<std::uint8_t>((std::min)(255, p[0] + color.b * transparent / 255));
                    p[1] = static_cast<std::uint8_t>((std::min)(255, p[1] + color.g * transparent / 255));
                    p[2] = static_cast<std::uint8_t>((std::min)(255, p[2] + color.r * transparent / 255));
                    p[3] = 255;
                }
                expect(bitmap.PixelWidth() > 0 && bitmap.PixelHeight() > 0 && pixels.Length() > 0)
                    << "native picker content snapshot is empty";
                if (pixels.Length() > 0) visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/inline-presentation/language-picker.png", bitmap.PixelWidth(), bitmap.PixelHeight(), pixels);
            }
            box.Text(L"python");
            Pump([&] { return languages.Items().Size() == 1 && languages.SelectedItem()
                && winrt::unbox_value<winrt::hstring>(languages.SelectedItem()) == L"python"; });
            expect(controller.Submit());
            std::fprintf(stderr, "language picker stage=close after submit\n");
            Pump([&] { return !flyout.IsOpen(); });
            expect(commands == 1_u);
            expect(session.TextUtf8().starts_with("```python\n"));
            folia::Command undo; undo.kind = folia::CommandKind::Undo;
            expect(session.ExecuteCommand(undo));
            expect(session.Text() == original);
            renderer.Render(session.RenderFrame());
            auto before = renderer.RawBlockFor(owner);
            std::fprintf(stderr, "language picker before reopen: open=%d loaded=%d\n", flyout.IsOpen(), box.IsLoaded());
            expect(controller.Open(owner));
            std::fprintf(stderr, "language picker stage=reopen after undo\n");
            Pump([&] { return *opened >= 2 && flyout.IsOpen(); });
            box.Text(L"rust");
            renderer.ScrollBy(120);
            renderer.Render(session.RenderFrame());
            controller.Refresh();
            std::fprintf(stderr, "language picker stage=scroll dismissal\n");
            Pump([&] { return *closed >= 2 && !flyout.IsOpen(); });
            auto after = renderer.RawBlockFor(owner);
            expect(before && after && after->headerRect.top < before->headerRect.top - 100);
            expect(session.Text() == original);
            expect(commands == 1_u) << "scroll dismissal must not commit a draft";
            folia::Command custom; custom.kind = folia::CommandKind::SetCodeBlockLanguage; custom.lang = U"MyLanguage";
            expect(session.ExecuteCommand(custom));
            auto customSource = session.Text();
            renderer.SetScrollOffset(0); renderer.Render(session.RenderFrame());
            expect(controller.Open(owner));
            Pump([&] { return *opened >= 3 && flyout.IsOpen(); });
            expect(box.Text().empty());
            expect(winrt::unbox_value<winrt::hstring>(languages.SelectedItem()) == L"MyLanguage");
            expect(controller.Submit());
            Pump([&] { return *closed >= 3 && !flyout.IsOpen(); });
            expect(session.Text() == customSource);
            expect(commands == 1_u) << "accepting the current custom language must be a no-op";
            expect(session.ExecuteCommand(undo));
            expect(session.Text() == original);
            renderer.SetInvalidateCallback({});
        };
    };
}
