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
            Flyout flyout; AutoSuggestBox box;
            box.Width(280); flyout.Content(box);
            auto opened = std::make_shared<unsigned>(0);
            auto closed = std::make_shared<unsigned>(0);
            flyout.Opened([opened](auto const&, auto const&) { ++*opened; std::fprintf(stderr, "language picker event=Opened\n"); });
            flyout.Closing([](auto const&, auto const&) { std::fprintf(stderr, "language picker event=Closing\n"); });
            flyout.Closed([closed](auto const&, auto const&) { ++*closed; std::fprintf(stderr, "language picker event=Closed\n"); });
            EditorCodeLanguageController controller;
            std::size_t commands = 0;
            controller.Attach(session, renderer, panel, flyout, box, [&](auto const& command) {
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
            expect(box.Text() == L"cpp");
            expect(static_cast<bool>(box.FontFamily().Source() == winrt::to_hstring(renderer.Theme().typography.code.family)));
            expect(box.FontSize() == renderer.Theme().typography.code.size);
            box.Text(L"python");
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
            std::fprintf(stderr, "language picker before reopen: open=%d loaded=%d suggestions=%d\n",
                flyout.IsOpen(), box.IsLoaded(), box.IsSuggestionListOpen());
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
            renderer.SetInvalidateCallback({});
        };
    };
}
