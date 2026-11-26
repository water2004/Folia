#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/layout/EditorContentPreparation.h"
#include "editor/interaction/EditorKeyboardController.h"
#include "editor/interaction/EditorPointerController.h"
#include "support/DocumentControllerFixture.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

import folia.markdown.instrumentation;
import folia.core.command;

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;

    suite workspace_rendering_tests = [] {
        "native table rendering keeps the label when a checkbox is added toggled and removed"_test = [] {
            namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
            using Kind = folia::platform::editor::EditorTableActionKind;
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| Status | Unrelated |\n| --- | --- |\n| **Review** | [x]literal |\n");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 4_u));
            const folia::TextPosition cell{spans[2].container_id, 0};
            const folia::TextPosition unrelated{spans[3].container_id, 0};
            session.SetSelection(folia::TextSelection::cells(cell, cell));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::MakeTableCheckbox}));
            expect(session.EditableSource(cell.container_id) == U"[ ] **Review**");
            expect(session.TableCellDetails(cell)->checkbox == false);
            expect(!session.TableCellDetails(unrelated)->checkbox.has_value());

            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            const auto start = renderer.CaretBounds(cell);
            const auto label = renderer.CaretBounds({cell.container_id, 9});
            expect(fatal(start && label));
            expect(label->left > start->left + 20.0f);
            const auto x = start->left + 11.0f;
            const auto y = std::midpoint(start->top, start->bottom);
            renderer.UpdatePointer(x, y);
            renderer.Render(session.RenderFrame());
            const auto controls = renderer.TableControlsAt(x, y);
            expect(fatal(controls.Hit() && controls.Hit()->action.kind == Kind::Checkbox));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::ToggleTableCheckbox}));
            expect(session.EditableSource(cell.container_id) == U"[x] **Review**");
            session.SetSelection(folia::TextSelection::caret({cell.container_id, 1}));
            renderer.Render(session.RenderFrame());
            expect(std::ranges::none_of(renderer.TableControlsAt(x, y).Visible(), [](const auto& control) {
                return control.action.kind == Kind::Checkbox;
            }));
            session.SetSelection(folia::TextSelection::cells(cell, cell));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::MakeTableCheckbox}));
            expect(session.EditableSource(cell.container_id) == U"**Review**");
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.EditableSource(cell.container_id) == U"[x] **Review**");
        };

        "native table rendering keeps grid selection out of inline formatting"_test = [] {
            EditorSession session;
            session.SetText(L"| **left** | **name** |\n| - | - |\n| **left** | **name** |\n");
            const auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 4_u));
            const auto owner = spans[1].container_id;
            session.SetSelection({owner, 2}, {owner, 6});
            expect(session.ActiveInlineStyle().bold);
            session.SetSelection(folia::TextSelection::cells({owner, 0}, {spans[3].container_id, 0}));
            expect(!session.ActiveInlineStyle().bold);
            expect(!session.ExecuteCommand({.kind = folia::CommandKind::ToggleStrong}));
        };

        "native table rendering retargets a short selection animation in both directions"_test = [] {
            EditorTableSelectionAnimation animation;
            animation.Configure(0.080);
            const auto owner = folia::NodeId{30};
            const folia::TextPosition anchor{folia::NodeId{1}, 0};
            const folia::TextPosition endpoint{folia::NodeId{4}, 0};
            const auto single = folia::TextSelection::cells(anchor, anchor);
            const auto range = folia::TextSelection::cells(anchor, endpoint);
            const auto moved = folia::TextSelection::cells(endpoint, endpoint);
            const D2D1_RECT_F first{0, 0, 100, 25};
            const D2D1_RECT_F second{0, 0, 300, 75};
            auto frame = [&](folia::TextSelection selection, D2D1_RECT_F target, double time,
                std::uint64_t revision = 1) {
                animation.BeginFrame(selection, revision, time);
                auto bounds = animation.Sample(owner, target);
                animation.EndFrame();
                return bounds;
            };
            expect(frame(single, first, 0.0).right == first.right && !animation.Animating());
            frame(range, second, 0.01);
            auto growing = frame(range, second, 0.05);
            expect(growing.right == 275.0f && growing.bottom == 68.75f && animation.Animating());
            auto reversed = frame(single, first, 0.05);
            expect(reversed.right == growing.right && reversed.bottom == growing.bottom);
            auto shrinking = frame(single, first, 0.09);
            expect(std::abs(shrinking.right - 121.875f) < 0.01f && animation.Animating());
            expect(frame(single, first, 0.14).right == first.right && !animation.Animating());

            const D2D1_RECT_F translated{100, 25, 200, 50};
            frame(moved, translated, 0.15);
            const auto moving = frame(moved, translated, 0.19);
            expect(std::abs(moving.left - 87.5f) < 0.01f && moving.right - moving.left == 100.0f);
            expect(moving.top > first.top && moving.top < translated.top && animation.Animating());
            expect(frame(moved, translated, 0.24).left == translated.left && !animation.Animating());

            frame(range, second, 0.25);
            expect(frame(range, second, 0.26, 2).right == second.right && !animation.Animating());
            const D2D1_RECT_F resized{0, 0, 240, 75};
            expect(frame(range, resized, 0.27, 2).right == resized.right && !animation.Animating());
            animation.Configure(0.0);
            frame(single, first, 0.30);
            expect(frame(range, second, 0.31).right == second.right && !animation.Animating());
        };

        "native table rendering keeps merged-cell hits and offscreen row navigation"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            std::wstring source = L"| A | B | C |\n| - | - | - |\n";
            for (int row = 0; row < 100; ++row) source += L"| left | middle | right |\n";
            source += LR"(<!-- gogoga-table: {"w":[90,120,150],"merge":["B2:C3"],"density":"compact"} -->)";
            session.SetText(winrt::hstring(source));
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& model = frame.renderModel;
            expect(fatal(!model.blocks.empty()));
            const auto& table = model.blocks.front().special();
            expect(fatal(table.table_cell_spans.size() == 303_u));
            const auto leading = folia::TextPosition{table.table_cell_spans[4].container_id, 0};
            const auto covered = folia::TextPosition{table.table_cell_spans[5].container_id, 0};
            const auto last = folia::TextPosition{table.table_cell_spans.back().container_id, 0};
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            const auto anchor = renderer.CaretBounds(leading);
            expect(fatal(anchor.has_value()));
            const auto coveredCaret = renderer.CaretBounds(covered);
            expect(fatal(coveredCaret.has_value()));
            expect(coveredCaret->top > anchor->top);
            const auto hit = renderer.HitTest(anchor->left + 170.0f, (anchor->top + anchor->bottom) / 2);
            expect(fatal(hit.has_value()));
            expect(hit->container_id == leading.container_id);
            expect(!renderer.CaretBounds(last).has_value());
            expect(renderer.ScrollToPosition(last));
            renderer.Render(session.RenderFrame());
            expect(renderer.CaretBounds(last).has_value());
        };

        "native table rendering selects and edits all text inside a merged cell"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| A | B | C |\n| - | - | - |\n| alpha | beta | stay |\n| gamma | delta | keep |\n"
                LR"(<!-- gogoga-table: {"merge":["A2:B3"]} -->)");
            const auto original = session.TextUtf8();
            const auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 9_u));
            const auto first = spans[3].container_id;
            const auto last = spans[7].container_id;
            const auto other = spans[5].container_id;
            const auto start = folia::TextPosition{first, 2};
            const auto end = folia::TextPosition{last, 3};
            session.SetSelection(folia::TextSelection::cells({first, 0}, {first, 0}));
            const auto edit = session.TableEnterTarget();
            expect(fatal(edit && edit->container_id == last && edit->source_offset == 5_u));
            expect(session.TableAddress(*edit)->column == 0_u);
            session.SetSelection(folia::TextSelection::caret(*edit));
            expect(session.TextUtf8() == original);
            expect(!session.ExecuteCommand({.kind = folia::CommandKind::CommitTableCell}));

            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            const auto a = renderer.CaretBounds(start);
            const auto b = renderer.CaretBounds(end);
            const auto unrelated = renderer.CaretBounds({other, 0});
            expect(fatal(a && b && unrelated));
            const auto hit = renderer.HitTestTableText(b->left + .1f, std::midpoint(b->top, b->bottom), first);
            expect(fatal(hit && hit->container_id == last));
            expect(hit->source_offset == 3_u);

            EditorSelectionDrag drag;
            EditorScrollController scroll;
            EditorTextInputController input;
            EditorTablePointerController pointer;
            pointer.Attach(session, renderer, scroll, input, panel, nullptr, {}, {}, {},
                [&] { renderer.Render(session.RenderFrame()); }, {});
            session.SetSelection(folia::TextSelection::cells({first, 0}, {first, 0}));
            renderer.Render(session.RenderFrame());
            expect(pointer.DoubleTap({b->left + .1f, std::midpoint(b->top, b->bottom)}));
            expect(session.Selection().is_caret() && session.Selection().active.container_id == last);
            drag.Attach(session, renderer, scroll, input);
            drag.Begin(folia::TextSelection::caret(start), a->left, std::midpoint(a->top, a->bottom));
            expect(drag.Update(b->left + .1f, std::midpoint(b->top, b->bottom)));
            expect(session.Selection().active.container_id == last);
            expect(session.SelectedTextUtf8() == "pha\nbeta\ngamma\ndel");
            renderer.Render(session.RenderFrame());
            expect(!renderer.SelectionBackgroundAt(unrelated->left + 3.f, std::midpoint(unrelated->top, unrelated->bottom)));
            drag.Cancel();

            expect(session.ExecuteCommand({.kind = folia::CommandKind::SelectAll}));
            expect(session.SelectedTextUtf8() == "alpha\nbeta\ngamma\ndelta");
            session.SetSelection(folia::TextSelection::caret({spans[4].container_id, 4}));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::MoveRight}));
            expect(session.Selection().active.container_id == spans[6].container_id);

            const folia::TextSelection selection{start, end};
            session.SetSelection(selection);
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand({.kind = folia::CommandKind::InsertText, .text = U"X"}));
            const auto work = folia::read_core_operation_counters();
            expect(work.full_document_parses == 0_u && work.full_document_serializations == 0_u);
            expect(session.EditableSource(first) == U"alXta");
            expect(session.EditableSource(last) == U"");
            expect(session.EditableSource(other) == U"stay");
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.TextUtf8() == original && session.Selection() == selection);
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Redo}));
            expect(session.EditableSource(first) == U"alXta");
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::MoveSelection,
                .move_destination = folia::TextPosition{last, 5}}));
            expect(session.EditableSource(first) == U"altapha<br>beta<br>gamma<br>del");
            expect(session.EditableSource(other) == U"stay");
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.TextUtf8() == original && session.Selection() == selection);
        };

        "native table rendering keeps the first column frozen while scrolling"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| A | B | C |\n| - | - | - |\n| left | center | right |\n"
                LR"(<!-- gogoga-table: {"w":[120,280,280],"wide":1,"headerCol":true,"freeze":1} -->)");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& block = frame.renderModel.blocks.front();
            const auto id = block.id;
            const auto& spans = block.special().table_cell_spans;
            expect(fatal(spans.size() == 6_u));
            const auto frozen = folia::TextPosition{spans[3].container_id, 0};
            const auto last = folia::TextPosition{spans[5].container_id, 0};
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 495, 440);
            renderer.Render(session.RenderFrame());
            const auto before = renderer.CaretBounds(frozen);
            expect(fatal(before.has_value()));
            const auto scroll = renderer.ScrollTableAt(before->left, (before->top + before->bottom) / 2, 350.0f);
            expect(fatal(scroll && scroll->first == id && scroll->second > 0.0f));
            session.SetTableScrollOffset(id, scroll->second);
            renderer.Render(session.RenderFrame());
            const auto after = renderer.CaretBounds(frozen);
            expect(fatal(after.has_value()));
            expect(before->left == after->left);
            const auto hit = renderer.HitTest(before->left + 1.0f, (before->top + before->bottom) / 2);
            expect(fatal(hit.has_value()));
            expect(hit->container_id == frozen.container_id);
            expect(renderer.CaretBounds(last).has_value());
        };

        "native table rendering reuses cell layouts for range selection and a single edit"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            std::wstring source = L"| A | B | C |\n| - | - | - |\n";
            for (int row = 0; row < 120; ++row) source += L"| left | center | right |\n";
            session.SetText(winrt::hstring(source));
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 363_u));
            const auto anchor = folia::TextPosition{spans[4].container_id, 0};
            const auto active = folia::TextPosition{spans[10].container_id, 0};
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.ConfigureCaret(true, 0.0);
            renderer.Render(session.RenderFrame());
            const auto baseline = renderer.TableTextLayoutsBuilt();
            expect(baseline >= spans.size());
            session.SetSelection(folia::TextSelection::cells(anchor, anchor));
            renderer.Render(session.RenderFrame());
            expect(!renderer.SelectionAnimating());
            session.SetSelection(folia::TextSelection::cells(anchor, active));
            renderer.Render(session.RenderFrame());
            expect(renderer.SelectionAnimating());
            expect(renderer.TableTextLayoutsBuilt() == baseline);
            session.SetSelection(folia::TextSelection::caret(active));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::InsertText, .text = U"x"}));
            renderer.Render(session.RenderFrame());
            expect(renderer.TableTextLayoutsBuilt() - baseline == 1_u);
            expect(renderer.CaretBounds(active).has_value());
        };

        "native table rendering previews column sizing without changing source"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| A | B |\n| - | - |\n| left | right |\n"
                LR"(<!-- gogoga-table: {"w":[140,140]} -->)");
            const auto source = session.TextUtf8();
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& table = frame.renderModel.blocks.front();
            const auto first = folia::TextPosition{table.special().table_cell_spans[0].container_id, 0};
            const auto second = folia::TextPosition{table.special().table_cell_spans[1].container_id, 0};
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            const auto before = renderer.CaretBounds(second);
            expect(fatal(before.has_value()));
            const auto builds = renderer.TableTextLayoutsBuilt();
            folia::platform::editor::EditorTableAction action{
                .kind = folia::platform::editor::EditorTableActionKind::ResizeColumn,
                .sourcePosition = first, .width = 190.0f, .tableId = table.id};
            renderer.SetTableDrag(action, {});
            renderer.Render(session.RenderFrame());
            const auto after = renderer.CaretBounds(second);
            expect(fatal(after.has_value()));
            expect(after->left == before->left + 50.0f);
            expect(renderer.TableTextLayoutsBuilt() == builds);
            expect(session.TextUtf8() == source);
            auto format = renderer.TableResizeFormat(action, 190.0f);
            expect(fatal(format.has_value()));
            expect(format->widths[1] == 140.0_f);
            renderer.SetTableDrag({}, {});
            session.SetSelection(folia::TextSelection::cells(first, first));
            expect(session.ExecuteCommand({.kind = folia::CommandKind::SetTableAppearance, .table_format = format}));
            renderer.Render(session.RenderFrame());
            const auto committed = renderer.CaretBounds(second);
            expect(fatal(committed.has_value()));
            expect(committed->left == after->left);
        };

        "native table rendering keyboard switches between grid and cell editing"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| A | B |\n| - | - |\n| alpha | bravo |\n| charlie | delta |");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 6_u));
            const auto a = folia::TextPosition{spans[2].container_id, 0};
            const auto b = folia::TextPosition{spans[3].container_id, 0};
            const auto d = folia::TextPosition{spans[5].container_id, 0};
            session.SetSelection(folia::TextSelection::cells(a, a));
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            EditorTextInputController input;
            EditorKeyboardController keyboard;
            keyboard.Attach(session, renderer, input,
                [&](const folia::Command& command) { return session.ExecuteCommand(command); },
                {}, {}, {}, {}, {}, [&] { renderer.Render(session.RenderFrame()); });
            using Key = folia::platform::editor::EditorKey;
            expect(keyboard.Key(folia::platform::editor::EditorKeyGesture{.key = Key::Enter}));
            expect(session.Selection().is_caret());
            expect(session.Selection().active.container_id == a.container_id);
            expect(session.Selection().active.source_offset == 5_u);
            expect(keyboard.Key(folia::platform::editor::EditorKeyGesture{.key = Key::Right}));
            expect(session.Selection() == folia::TextSelection::cells(b, b));
            expect(keyboard.Key(folia::platform::editor::EditorKeyGesture{.key = Key::F2}));
            expect(session.Selection().is_caret());
            expect(keyboard.Key(folia::platform::editor::EditorKeyGesture{.key = Key::Enter}));
            expect(session.Selection() == folia::TextSelection::cells(d, d));
        };

        "native table rendering context actions preserve a rectangular selection"_test = [] {
            namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
            namespace Automation = winrt::Microsoft::UI::Xaml::Automation;
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"| A | B | C |\n| - | - | - |\n| a | b | c |\n| d | e | f |");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, 1);
            const auto& spans = frame.renderModel.blocks.front().special().table_cell_spans;
            expect(fatal(spans.size() == 9_u));
            const folia::TextPosition start{spans[3].container_id, 0};
            const folia::TextPosition inside{spans[4].container_id, 0};
            const folia::TextPosition outside{spans[5].container_id, 0};
            const folia::TextPosition end{spans[7].container_id, 0};
            const auto range = folia::TextSelection::cells(start, end);
            session.SetSelection(range);
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.Render(session.RenderFrame());
            auto pointAt = [&](folia::TextPosition position) {
                const auto bounds = renderer.CaretBounds(position);
                expect(fatal(bounds.has_value()));
                return winrt::Windows::Foundation::Point{bounds->left + 1, (bounds->top + bounds->bottom) / 2};
            };
            const auto insidePoint = pointAt(inside);
            const auto outsidePoint = pointAt(outside);
            EditorScrollController scroll;
            EditorTextInputController input;
            EditorPointerController pointer;
            auto renders = 0U, queuedFrames = 0U, commands = 0U, copies = 0U;
            pointer.Attach(session, renderer, scroll, input, panel, nullptr, {{}, [&] { ++copies; }, {}},
                [&](const folia::Command& command) { ++commands; return session.ExecuteCommand(command); },
                [&] { ++renders; renderer.Render(session.RenderFrame()); }, [&] { ++queuedFrames; }, {}, {}, {});
            auto itemWith = [](auto const& items, auto label) -> Controls::MenuFlyoutItemBase {
                const auto name = Localize(label);
                for (auto const& item : items) {
                    if (auto command = item.template try_as<Controls::MenuFlyoutItem>()) {
                        if (command.Text() == name) return item;
                    } else if (auto group = item.template try_as<Controls::MenuFlyoutSubItem>()) {
                        if (group.Text() == name) return item;
                    }
                }
                return nullptr;
            };
            auto invoke = [](Controls::MenuFlyoutItemBase const& entry) {
                expect(fatal(bool(entry)));
                Automation::Peers::MenuFlyoutItemAutomationPeer peer(entry.as<Controls::MenuFlyoutItem>());
                peer.GetPattern(Automation::Peers::PatternInterface::Invoke)
                    .as<Automation::Provider::IInvokeProvider>().Invoke();
            };
            const auto menu = pointer.ContextMenuAt(insidePoint);
            expect(fatal(bool(menu)));
            expect(session.Selection() == range);
            expect(renders == 0_u);
            invoke(itemWith(menu.Items(), L"Copy"));
            document_tests::Pump([&] { return copies == 1; });
            const auto colors = itemWith(menu.Items(), L"TableBackground");
            expect(fatal(bool(colors)));
            invoke(itemWith(colors.as<Controls::MenuFlyoutSubItem>().Items(), L"TableRed"));
            document_tests::Pump([&] { return commands == 1; });
            const auto colored = session.TextUtf8();
            for (auto cell : {"A2", "B2", "A3", "B3"})
                expect(colored.contains(std::string{"\""} + cell + "\":\"red\""));
            expect(session.Selection() == range);

            expect(bool(pointer.ContextMenuAt(outsidePoint)));
            expect(session.Selection() == folia::TextSelection::cells(outside, outside));
            expect(renders == 1_u);
            expect(queuedFrames == 0_u);

            session.SetSelection(range);
            const auto mergeMenu = pointer.ContextMenuAt(insidePoint);
            expect(fatal(bool(mergeMenu)));
            invoke(itemWith(mergeMenu.Items(), L"TableMerge"));
            document_tests::Pump([&] { return commands == 2; });
            expect(session.TextUtf8().contains("\"merge\":[\"A2:B3\"]"));
            pointer.Detach();
        };

        "workspace rendering transfers formula geometry and releases inactive SVG demand"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession first, second;
            first.SetText(L"First $x+1$ and $x+2$.");
            second.SetText(L"Second $y+3$.");
            const auto source = first.TextUtf8();
            const auto selection = first.Selection();
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            const auto context = renderer.CaptureMathPreparationContext();
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(first.RenderFrame(), context, {}, {}));
            renderer.Render(first.RenderFrame());
            auto firstCache = renderer.TakeMathCache();
            firstCache.WaitForIdle();
            auto original = firstCache.Snapshot();
            expect(original.entries == 2_u);
            expect(original.residentSvgBytes == 0_u);
            expect(original.queuedReads == 0_u);
            expect(original.previewSvgBytes == 0_u);
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(second.RenderFrame(), context, {}, {}));
            renderer.Render(second.RenderFrame());
            auto secondCache = renderer.TakeMathCache();
            secondCache.WaitForIdle();
            expect(secondCache.Snapshot().backingPath != original.backingPath);
            folia::reset_core_operation_counters();
            renderer.RestoreMathCache(std::move(firstCache));
            renderer.Render(first.RenderFrame());
            firstCache = renderer.TakeMathCache();
            firstCache.WaitForIdle();
            auto restored = firstCache.Snapshot();
            expect(restored.backingPath == original.backingPath);
            expect(restored.nativeRenders == original.nativeRenders);
            expect(restored.fileWrites == original.fileWrites);
            expect(restored.residentSvgBytes == 0_u);
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first.Selection() == selection);
            expect(first.TextUtf8() == source);
        };
    };
}
