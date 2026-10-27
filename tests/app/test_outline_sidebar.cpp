#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/interaction/EditorSidebarController.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

import folia.markdown.instrumentation;

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    struct SidebarFixture {
        Hosting::WindowsXamlManager xaml = Hosting::WindowsXamlManager::InitializeForCurrentThread();
        SwapChainPanel panel;
        ListView list;
        EditorSession session;
        EditorSurfaceRenderer renderer;
        EditorTextInputController input;
        EditorSidebarController sidebar;
        int navigationRenders = 0;

        explicit SidebarFixture(winrt::hstring const& source) {
            session.SetText(source);
            renderer.Initialize(panel);
            renderer.Resize(panel, 800, 300);
            renderer.Render(session.RenderFrame());
            sidebar.Attach(session, renderer, input, list, [this] {
                ++navigationRenders;
                Render();
            });
            sidebar.SyncViewport();
        }
        ~SidebarFixture() {
            sidebar.Detach();
        }
        void Render() {
            renderer.Render(session.RenderFrame());
            sidebar.SyncViewport();
        }
        void At(folia::TextPosition position) {
            renderer.ScrollToPosition(position, EditorScrollAlignment::Start);
            Render();
            renderer.ScrollToPosition(position, EditorScrollAlignment::Start);
            Render();
        }
    };

    std::wstring Body() {
        std::wstring body;
        for (int i = 0; i < 12; ++i) body += L"Paragraph with **bold** text and a [link](https://example.com).\n\n";
        return body;
    }

    suite outline_sidebar_tests = [] {
        "outline sidebar wraps full Unicode titles within narrow widths"_test = [] {
            auto title = std::wstring{L"Long heading 中文标题 emoji 😀 with several words and another long description"};
            SidebarFixture fixture(winrt::hstring(L"# Root\n\n### " + title + L"\n"));
            auto& list = fixture.list;
            expect(ScrollViewer::GetHorizontalScrollMode(list) == ScrollMode::Disabled);
            expect(ScrollViewer::GetHorizontalScrollBarVisibility(list) == ScrollBarVisibility::Disabled);
            for (float width : {180.f, 260.f}) {
                auto text = list.ItemTemplate().LoadContent().as<TextBlock>();
                // Measure the real template without opening a test window.
                // Data binding is activated only when a container is loaded.
                text.Text(winrt::unbox_value<winrt::hstring>(list.Items().GetAt(1)));
                text.Measure({width, 800});
                text.Arrange({0, 0, width, text.DesiredSize().Height});
                expect(text.Text() == title) << "label";
                expect(text.TextWrapping() == TextWrapping::Wrap) << "wrap";
                expect(text.TextTrimming() == TextTrimming::None) << "trimming";
                expect(text.ActualWidth() <= width) << "width";
                expect(text.ActualHeight() > 30.0) << "multiline height";
            }
        };

        "outline sidebar follows scrolling without navigating or changing the document"_test = [] {
            auto body = Body();
            SidebarFixture f(winrt::hstring(body + L"# Repeated\n\n" + body
                + L"> ## Repeated\n> Nested content\n\n" + body + L"# Last\n\nShort.\n"));
            auto source = f.session.Text();
            auto selection = f.session.Selection();
            auto headings = f.session.RenderModel().outline.flat_items();
            expect(fatal(headings.size() == 3_u));
            expect(f.list.SelectedIndex() == -1);
            auto items = f.list.ItemsSource();
            folia::reset_core_operation_counters();
            for (int index : {0, 1, 0, 1}) {
                f.At(headings[index]->position);
                expect(f.list.SelectedIndex() == index);
                expect(f.list.ItemsSource() == items) << "scrolling must reuse the outline projection";
            }
            f.renderer.SetScrollOffset(f.renderer.MaximumScrollOffset());
            f.Render();
            expect(f.list.SelectedIndex() == 2);
            f.renderer.SetScrollOffset(0);
            f.Render();
            expect(f.list.SelectedIndex() == -1);
            expect(f.navigationRenders == 0);
            expect(f.session.Selection() == selection);
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(f.session.Text() == source);
        };

        "outline sidebar click navigation targets stable heading ids and clears on replacement"_test = [] {
            auto body = Body();
            SidebarFixture f(winrt::hstring(L"# Same\n\n" + body + L"> ## Same\n> content\n\n" + body + L"# Tail\n"));
            auto headings = f.session.RenderModel().outline.flat_items();
            auto nested = headings[1]->position;
            expect(f.list.IsItemClickEnabled());
            f.sidebar.SelectOutline(f.list.Items().GetAt(1));
            expect(f.session.Selection().active.container_id == nested.container_id);
            expect(f.list.SelectedIndex() == 1);
            expect(f.navigationRenders > 0 && f.navigationRenders <= 2);
            f.renderer.ScrollBy(100);
            f.Render();
            expect(f.list.SelectedIndex() == 1);
            auto scrolled = f.renderer.ScrollOffset();
            f.sidebar.SelectOutline(f.list.Items().GetAt(1));
            expect(f.renderer.ScrollOffset() < scrolled) << "already highlighted item still navigates";
            f.session.SetSelection({nested.container_id, 0}, {nested.container_id, 4});
            expect(f.session.ExecuteCommand(folia::Command::InsertText(U"Renamed")));
            f.Render();
            expect(winrt::unbox_value<winrt::hstring>(f.list.Items().GetAt(1)) == L"Renamed");
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            f.Render();
            expect(winrt::unbox_value<winrt::hstring>(f.list.Items().GetAt(1)) == L"Same");
            f.session.SetText(L"Plain document, without headings.\n");
            f.Render();
            expect(f.list.Items().Size() == 0_u);
            expect(f.list.SelectedIndex() == -1);
            f.sidebar.Detach();
            expect(f.list.Items().Size() == 0_u);
        };

        "outline sidebar follows the source view without moving its caret"_test = [] {
            auto body = Body();
            SidebarFixture f(winrt::hstring(L"# Root\n\n" + body + L"## Second\n\n" + body));
            expect(f.session.EnterSourceMode());
            f.Render();
            auto selection = f.session.Selection();
            auto headings = f.session.RenderModel().outline.flat_items();
            expect(fatal(headings.size() == 2_u));
            f.At(headings[1]->position);
            expect(f.list.SelectedIndex() == 1) << "source viewport";
            expect(f.session.Selection() == selection);
            expect(f.navigationRenders == 0);
        };

        "outline sidebar keeps the section across table list quote and math owners"_test = [] {
            auto body = Body();
            SidebarFixture f(winrt::hstring(L"# Section\n\n"
                L"| Left | Right |\n| --- | --- |\n| one | two |\n\n"
                L"- list **text**\n- another item\n\n"
                L"> Quoted paragraph\n\n"
                L"$$\nx+1\n$$\n\n"
                L"Paragraph with $x+1$ inline.\n\n" + body + L"# Next\n\n" + body));
            auto const& model = f.session.RenderModel();
            auto headings = model.outline.flat_items();
            auto end = model.editable_index.at(headings[1]->position.container_id.v);
            auto original = f.session.Selection();
            for (std::size_t index = 0; index < end; ++index) {
                f.At({model.editable_order[index], 0});
                expect(f.list.SelectedIndex() == 0) << "editable owner" << index;
            }
            expect(f.session.Selection() == original);
            expect(f.navigationRenders == 0);
        };
    };
}
