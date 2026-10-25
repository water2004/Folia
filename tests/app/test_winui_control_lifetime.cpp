#include "pch.h"
#include "support/folia_test.hpp"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include "editor/interaction/EditorLatexCompletionController.h"

using namespace boost::ut;
using namespace winrt::Folia;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
    [[noreturn]] void ReportCompletionError(char const* stage, winrt::hresult_error const& error)
    {
        throw std::runtime_error(std::format("{}: HRESULT 0x{:08X}: {}", stage,
            static_cast<std::uint32_t>(error.code().value), winrt::to_string(error.message())));
    }

    struct CompletionFixture
    {
        Hosting::WindowsXamlManager xaml{Hosting::WindowsXamlManager::InitializeForCurrentThread()};
        EditorSession session;
        EditorSurfaceRenderer renderer;
        Canvas overlay;
        Border popup;
        TextBlock prefix;
        ListView list;
        std::shared_ptr<LatexCommandCatalog> catalog = std::make_shared<LatexCommandCatalog>();
        EditorLatexCompletionController controller;
        std::u32string accepted;
        bool closedBeforeInsert = false;
        ResourceDictionary resources{nullptr};
        bool insertedAccent = false;

        CompletionFixture() try
        {
            // WindowsXamlManager supplies a minimal host, not Folia's App.xaml
            // resource dictionaries. Supply the one app resource used by this
            // controller and restore the host when the fixture is destroyed.
            resources = Application::Current().Resources();
            auto key = winrt::box_value(L"AccentTextFillColorPrimaryBrush");
            if (!resources.HasKey(key))
            {
                resources.Insert(key, Media::SolidColorBrush(winrt::Windows::UI::Color{255, 0, 120, 215}));
                insertedAccent = true;
            }
            // Positioning needs a rendered caret; this test deliberately uses
            // no surface, and exercises the production popup's real controls.
            controller.Attach(session, renderer, nullptr, overlay, popup, prefix, list, catalog,
                [this](auto, auto, std::u32string_view snippet)
                {
                    accepted = snippet;
                    closedBeforeInsert = list.Items().Size() == 0 && !overlay.IsHitTestVisible();
                    return false; // Do not persist usage in the user's catalog.
                });
        }
        catch (winrt::hresult_error const& error) { ReportCompletionError("completion fixture initialization", error); }

        ~CompletionFixture()
        {
            controller.Detach();
            if (insertedAccent) resources.Remove(winrt::box_value(L"AccentTextFillColorPrimaryBrush"));
        }

        void Query(winrt::hstring const& source, std::size_t offset) try
        {
            session.SetText(source);
            auto caret = session.Selection().active;
            caret.source_offset = offset;
            session.SetSelection(caret, caret);
            controller.Refresh();
        }
        catch (winrt::hresult_error const& error) { ReportCompletionError("completion query/refresh", error); }

        Grid Row(std::uint32_t index = 0) { return list.Items().GetAt(index).as<Grid>(); }
        TextBlock Trigger(Grid const& row)
        {
            return row.Children().GetAt(0).as<StackPanel>().Children().GetAt(0).as<TextBlock>();
        }
    };

    suite winui_control_lifetime_tests = []
    {
        "latex popup refresh reuses live visual trees and updates narrowed candidates"_test = []
        {
            CompletionFixture fixture;
            fixture.Query(L"$\\a$", 3);
            expect(fixture.controller.Active());
            auto count = fixture.list.Items().Size();
            expect(count > 1_u);
            if (!count) return;
            auto row = fixture.Row();
            auto trigger = fixture.Trigger(row);
            for (int i = 0; i < 20; ++i) fixture.controller.Refresh();
            expect(fixture.Row() == row);
            expect(fixture.Trigger(fixture.Row()) == trigger);
            expect(fixture.list.Items().Size() == count);

            fixture.Query(L"$\\alpha$", 7);
            expect(fixture.controller.Active());
            expect(fixture.list.Items().Size() < count);
            expect(fixture.Row() == row);
            expect(fixture.Trigger(fixture.Row()) == trigger);
            expect(trigger.Text() == L"\\alpha");
            auto query = fixture.catalog->QueryAt(U"$\\alpha$", 7);
            expect(query.has_value());
            if (query)
                expect(static_cast<bool>(winrt::unbox_value<winrt::hstring>(row.Tag()) ==
                    winrt::to_hstring(query->candidates.front().command.id)));
        };

        "latex popup cancel and detach release their rows"_test = []
        {
            CompletionFixture fixture;
            fixture.Query(L"$\\alpha$", 7);
            expect(fixture.list.Items().Size() > 0_u);
            auto weakRow = winrt::make_weak(fixture.Row());
            fixture.controller.Cancel();
            expect(fixture.list.Items().Size() == 0_u);
            expect(!fixture.overlay.IsHitTestVisible());
            expect(fixture.popup.Visibility() == Visibility::Collapsed);
            expect(!weakRow.get());
            fixture.Query(L"$\\alpha$", 7);
            weakRow = winrt::make_weak(fixture.Row());
            fixture.controller.Detach();
            expect(fixture.list.Items().Size() == 0_u);
            expect(!weakRow.get());
        };

        "latex popup accept releases visuals before invoking source insertion"_test = []
        {
            CompletionFixture fixture;
            fixture.Query(L"$\\alpha$", 7);
            expect(fixture.controller.Active());
            auto weakRow = winrt::make_weak(fixture.Row());
            fixture.controller.AcceptSelected();
            expect(fixture.accepted == U"\\alpha$0");
            expect(fixture.closedBeforeInsert);
            expect(!fixture.controller.Active());
            expect(fixture.popup.Visibility() == Visibility::Collapsed);
            expect(!weakRow.get());
        };
    };
}
