#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include "media/NativeMathSession.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

import folia.markdown.utf;

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    std::vector<SyntaxIcon> EditingIcons(EditorSession& session)
    {
        auto caret = session.Selection().active;
        auto source = session.EditableSource(caret.container_id);
        auto frame = session.RenderFrame();
        if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
        MathJaxRenderer math;
        SvgNormalizer normalizer;
        std::vector<SyntaxIcon> icons;
        auto collect = [&](DisplayInlineText const& display) {
            for (auto const& range : display.ranges)
                if (range.icon != SyntaxIcon::None && range.start < display.displayToSource.size()
                    && display.displayToSource[range.start].container_id == caret.container_id)
                    icons.push_back(range.icon);
        };
        for (auto const& block : frame.renderModel.blocks) {
            if (block.kind == folia::RenderBlockKind::Math && block.id == caret.container_id) {
                auto display = BuildMathBlockText(block, caret, math, normalizer, {}, 18, 500, false, false, false);
                expect(static_cast<bool>(display.text == *source));
                collect(display);
            } else {
                collect(BuildDisplayInlineText(block.inline_items, caret,
                    {caret.container_id, source->size(), folia::TextAffinity::Upstream},
                    math, normalizer, {}, 18, 500, false, false, false));
                for (std::size_t i = 0; i < block.special().table_cells.size(); ++i)
                    collect(BuildDisplayInlineText(block.special().table_cells[i], caret,
                        {caret.container_id, source->size(), folia::TextAffinity::Upstream},
                        math, normalizer, {}, 18, 500, false, false, false));
            }
        }
        expect(math.Snapshot().entries == 0_u);
        return icons;
    }

    suite syntax_marker_editing_tests = [] {
        "typing an incomplete marker stays source text and undo restores each semantic transition"_test = [] {
            for (auto initial : {L"target\n", L"# target\n", L"- target\n", L"> target\n", L"- [ ] target\n",
                L"| target |\n| --- |\n| body |\n", L"> [!NOTE]\n> target\n", L"[^a]: target\n"})
            for (auto spelling : {U"*中\U0001f600*", U"_中\U0001f600_", U"`中\U0001f600`", U"$x$"}) {
                EditorSession session;
                session.SetText(initial);
                auto owner = session.Selection().active.container_id;
                auto originalSource = session.EditableSource(owner);
                expect(originalSource.has_value());
                if (!originalSource) continue;
                session.SetSelection({owner, 0, folia::TextAffinity::Downstream},
                    {owner, originalSource->size(), folia::TextAffinity::Upstream});
                folia::Command erase;
                erase.kind = folia::CommandKind::DeleteSelection;
                expect(session.ExecuteCommand(erase)) << "clear initial selection";
                std::u32string typed;
                auto input = std::u32string_view{spelling};
                for (char32_t c : input) {
                    auto before = session.Text();
                    auto selectionBefore = session.Selection();
                    typed += c;
                    expect(session.ExecuteCommand(folia::Command::InsertText(std::u32string(1, c))));
                    auto after = session.Text();
                    auto selectionAfter = session.Selection();
                    expect(session.EditableSource(owner) == std::optional<std::u32string>{typed}) << "typed source";
                    auto icons = EditingIcons(session);
                    expect(icons.size() == (typed.size() == input.size() ? 2u : 0u)) << "semantic icons after input";
                    auto acp = session.TextInputAcpOffset(selectionAfter.active);
                    expect(session.TextInputPositionFromAcp(owner, acp).source_offset == typed.size());
                    folia::Command command;
                    command.kind = folia::CommandKind::Undo;
                    expect(session.ExecuteCommand(command));
                    expect(session.Text() == before) << "undo text";
                    expect(session.Selection() == selectionBefore) << "undo selection";
                    command.kind = folia::CommandKind::Redo;
                    expect(session.ExecuteCommand(command));
                    expect(session.Text() == after) << "redo text";
                    expect(session.Selection() == selectionAfter) << "redo selection";
                }
                auto complete = session.Text();
                session.SetSelection({owner, typed.size() - 1, folia::TextAffinity::Downstream},
                    {owner, typed.size(), folia::TextAffinity::Upstream});
                expect(session.ExecuteCommand(erase)) << "delete closing delimiter";
                expect(EditingIcons(session).empty());
                folia::Command undo;
                undo.kind = folia::CommandKind::Undo;
                expect(session.ExecuteCommand(undo));
                expect(session.Text() == complete);
                expect(EditingIcons(session).size() == 2_u);
                session.SetSelection({owner, 0, folia::TextAffinity::Downstream},
                    {owner, typed.size(), folia::TextAffinity::Upstream});
                expect(static_cast<bool>(session.SelectedSource() == typed)) << "copy retains source delimiters";
                auto selected = session.Selection();
                folia::Command paste;
                paste.kind = folia::CommandKind::Paste;
                paste.text = U"*pasted*";
                expect(session.ExecuteCommand(paste));
                expect(session.EditableSource(owner) == std::optional<std::u32string>{U"*pasted*"});
                expect(EditingIcons(session).size() == 2_u);
                expect(session.ExecuteCommand(undo));
                expect(session.Text() == complete);
                expect(session.Selection() == selected);
                expect(session.EnterSourceMode());
                expect(session.ExitSourceMode());
                expect(session.Text() == complete);
                expect(EditingIcons(session).size() == 2_u);
            }
        };

        "display math delimiter icons stay indivisible and deletion unwraps their TeX"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            for (auto source : {L"$$\nx+1\n$$\n", L"\\[\nx+1\n\\]\n"}) {
                winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
                EditorSession session;
                session.SetText(source);
                auto owner = session.Selection().active.container_id;
                auto raw = session.EditableSource(owner);
                expect(raw.has_value());
                if (!raw) continue;
                auto original = session.Text();
                EditorSurfaceRenderer renderer;
                renderer.Initialize(panel);
                renderer.Resize(panel, 500, 400);
                session.SetSelection(folia::TextSelection::caret({owner, 3, folia::TextAffinity::Downstream}));
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto markers = frame.renderModel.blocks.front().special().source_markers;
                expect(markers.size() == 2_u);
                renderer.Render(session.RenderFrame());
                for (std::size_t offset = 0; offset < raw->size(); ++offset) {
                    if ((*raw)[offset] == U'\n') continue; // line-end affinity has two valid visual edges
                    if (std::ranges::any_of(markers, [&](auto const& marker) {
                        return offset > marker.range.start && offset < marker.range.end;
                    })) continue; // a semantic marker has no internal caret
                    auto bounds = renderer.CaretBounds({owner, offset, folia::TextAffinity::Downstream});
                    expect(bounds.has_value()) << "math source caret" << offset;
                    if (!bounds) continue;
                    auto hit = renderer.HitTest(bounds->left + .1f, (bounds->top + bounds->bottom) * .5f);
                    expect(hit.has_value());
                    if (hit) {
                        expect(hit->container_id == owner);
                        expect(hit->source_offset == offset) << "math source hit";
                    }
                }
                auto end = raw->size();
                session.SetSelection({owner, end - 2, folia::TextAffinity::Downstream},
                    {owner, end, folia::TextAffinity::Upstream});
                auto selection = session.Selection();
                folia::Command erase;
                erase.kind = folia::CommandKind::DeleteSelection;
                expect(session.ExecuteCommand(erase));
                expect(EditingIcons(session).empty());
                expect(session.EditableSource(owner) == std::optional<std::u32string>{U"x+1"});
                folia::Command undo;
                undo.kind = folia::CommandKind::Undo;
                expect(session.ExecuteCommand(undo));
                expect(session.Text() == original);
                expect(session.Selection() == selection);
                expect(EditingIcons(session).size() == 2_u);
            }
        };

        "display math exposes parsed delimiters only while editing with lossless offsets"_test = [] {
            for (auto source : {L"$$\nx+1\n$$\n", L"\\[\nx+1\n\\]\n", L"$$\nx+1", L"\\[\nx+1"}) {
                EditorSession session;
                session.SetText(source);
                auto owner = session.Selection().active.container_id;
                auto raw = session.EditableSource(owner);
                expect(raw.has_value());
                if (!raw) continue;
                auto original = session.Text();
                for (std::size_t offset = 0; offset <= raw->size(); ++offset) {
                    session.SetSelection(folia::TextSelection::caret({owner, offset, folia::TextAffinity::Downstream}));
                    auto icons = EditingIcons(session);
                    expect(icons.size() == (std::wstring_view{source}.ends_with(L"\n") ? 2u : 0u));
                    auto acp = session.TextInputAcpOffset(session.Selection().active);
                    expect(session.TextInputPositionFromAcp(owner, acp).source_offset == offset);
                }
                expect(session.Text() == original);
            }
        };
        "block delimiter tiles align with TeX and scale with the source line height"_test = [] {
            EditorRenderResources resources;
            winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(resources.dwriteFactory.GetAddressOf())));
            MathJaxRenderer math;
            SvgNormalizer normalizer;
            for (auto source : {L"$$\nx+1\n$$\n", L"\\[\nx+1\n\\]\n"})
            for (float size : {12.f, 18.f, 32.f})
            for (float width : {80.f, 600.f}) {
                EditorSession session;
                session.SetText(source);
                auto original = session.Text();
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto const& block = frame.renderModel.blocks.front();
                EditorStyleSheet sheet;
                sheet.body = {L"Segoe UI", size, size * 1.6f};
                sheet.code = sheet.body;
                ::Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
                winrt::check_hresult(resources.dwriteFactory->CreateTextFormat(sheet.body.family.c_str(), nullptr,
                    DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    size, L"en-us", format.GetAddressOf()));
                auto display = BuildMathBlockText(block, {block.id, 3, folia::TextAffinity::Downstream},
                    math, normalizer, {}, size, width, false, false, false);
                EditorTextLayoutEngine engine(resources, sheet);
                auto layout = engine.CreateFlow(display, format.Get(), width, {});
                FLOAT bodyX = 0, bodyY = 0;
                DWRITE_HIT_TEST_METRICS body{};
                winrt::check_hresult(layout->HitTestTextPosition(
                    static_cast<UINT32>(block.special().content_to_source.front()), FALSE, &bodyX, &bodyY, &body));
                std::size_t count = 0;
                for (auto const& range : display.ranges) {
                    if (range.icon == SyntaxIcon::None) continue;
                    ++count;
                    expect(range.iconPlacement == SyntaxIconPlacement::Block);
                    auto rectangles = SyntaxIconRectangles(layout.Get(), range, {});
                    expect(rectangles.size() == 1_u);
                    if (rectangles.size() != 1) continue;
                    auto rect = rectangles.front();
                    expect(std::abs(rect.left - bodyX) < .01f) << "no inline clearance on block marker rows";
                    expect(std::abs(rect.right - rect.left - sheet.body.lineHeight * sheet.marker.sizeEm) < .01f);
                    expect(rect.bottom - rect.top >= sheet.body.lineHeight);
                    ::Microsoft::WRL::ComPtr<IDWriteInlineObject> object;
                    winrt::check_hresult(layout->GetInlineObject(range.start, object.GetAddressOf()));
                    DWRITE_INLINE_OBJECT_METRICS metrics{};
                    winrt::check_hresult(object->GetMetrics(&metrics));
                    expect(std::abs(metrics.width - metrics.height) < .01f);
                }
                expect(count == 2_u);
                expect(static_cast<bool>(display.text == block.special().raw_source));
                expect(session.Text() == original);
            }
        };

        "unmatched tilde keeps the list formula on the native math rendering path"_test = [] {
            NativeMathSession native;
            for (auto source : {L"1. 微观粒子质量~原子质量单位 $1u=1.6605655\\timesz10^{-27}kg$\n",
                L"1. 微观粒子质量原子质量单位 $1u=1.6605655\\timesz10^{-27}kg$\n"}) {
                EditorSession session;
                session.SetText(source);
                auto original = session.Text();
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                std::size_t count = 0;
                for (auto const& block : frame.renderModel.blocks)
                for (auto const& item : block.inline_items) {
                    if (item.kind != folia::InlineRenderItem::Kind::Math) continue;
                    ++count;
                    expect(static_cast<bool>(item.text == U"1u=1.6605655\\timesz10^{-27}kg"));
                    auto svg = native.Render(folia::cps_to_utf8(item.text), false, 18, 500);
                    expect(static_cast<bool>(svg)) << svg.error;
                    expect(!svg.fragments.empty());
                }
                expect(count == 1_u);
                expect(session.Text() == original);
            }
        };
    };
}
