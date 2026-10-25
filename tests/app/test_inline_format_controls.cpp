#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include "editor/interaction/EditorSelectionDrag.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <dwrite_1.h>

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    suite inline_format_controls_tests = [] {
        "marker layout reserves space outside italic ink at multiple font sizes"_test = [] {
            EditorRenderResources resources;
            winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(resources.dwriteFactory.GetAddressOf())));
            for (auto family : {L"Segoe UI", L"Times New Roman", L"Consolas"})
            for (auto size : {12.0f, 16.0f, 32.0f})
            for (auto body : {L"斜体内容", L"italic", L"ffff"}) {
                EditorStyleSheet sheet;
                sheet.body.family = family;
                sheet.body.size = size;
                sheet.body.lineHeight = size * 1.5f;
                sheet.code = sheet.body;
                ::Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
                winrt::check_hresult(resources.dwriteFactory->CreateTextFormat(family, nullptr,
                    DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    size, L"en-us", format.GetAddressOf()));
                EditorTextLayoutEngine engine(resources, sheet);
                auto text = std::wstring{L"*"} + body + L"*";
                auto length = static_cast<UINT32>(std::wcslen(body));
                auto italic = folia::InlineStyle::plain();
                italic.italic = true;
                std::vector<InlineStyleRange> ranges{
                    {0, 1, {}, true}, {1, length, italic, false}, {length + 1, 1, {}, true}};
                ranges.front().icon = ranges.back().icon = SyntaxIcon::Star;
                auto layout = engine.Create(text, format.Get(), 1000);
                engine.ApplyStyles(layout.Get(), ranges);
                auto opening = SyntaxIconRectangles(layout.Get(), ranges.front(), {0, 0});
                auto closing = SyntaxIconRectangles(layout.Get(), ranges.back(), {0, 0});
                expect(opening.size() == 1_u);
                expect(closing.size() == 1_u);
                if (opening.empty() || closing.empty()) continue;
                auto bodyLayout = engine.Create(body, format.Get(), 1000);
                bodyLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, {0, length});
                bodyLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                DWRITE_TEXT_METRICS metrics{};
                winrt::check_hresult(bodyLayout->GetMetrics(&metrics));
                bodyLayout->SetMaxWidth(metrics.widthIncludingTrailingWhitespace);
                DWRITE_OVERHANG_METRICS ink{};
                winrt::check_hresult(bodyLayout->GetOverhangMetrics(&ink));
                FLOAT bodyX = 0, bodyY = 0;
                DWRITE_HIT_TEST_METRICS hit{};
                winrt::check_hresult(layout->HitTestTextPosition(1, false, &bodyX, &bodyY, &hit));
                auto inkLeft = bodyX - (std::max)(0.0f, ink.left);
                auto inkRight = bodyX + metrics.widthIncludingTrailingWhitespace + (std::max)(0.0f, ink.right);
                expect(opening.front().right < inkLeft) << "opening marker intersects italic ink" << size;
                expect(closing.front().left > inkRight) << "closing marker intersects italic ink" << size;
                expect(closing.front().right > closing.front().left);
                // Space belongs to the actual layout, so each source position
                // still round-trips through DirectWrite hit testing.
                for (UINT32 position = 0; position < text.size(); ++position) {
                    FLOAT x = 0, y = 0;
                    winrt::check_hresult(layout->HitTestTextPosition(position, false, &x, &y, &hit));
                    BOOL trailing = false, inside = false;
                    DWRITE_HIT_TEST_METRICS back{};
                    winrt::check_hresult(layout->HitTestPoint(x + 0.1f, y + hit.height * 0.5f,
                        &trailing, &inside, &back));
                    expect(back.textPosition == position);
                }
            }
        };

        "inline format markers reveal their exact source only while editing the construct"_test = [] {
            struct Example { wchar_t const* source; std::u32string_view visible; };
            for (auto const& example : {
                Example{L"**abc** tail\n", U"abc tail"},
                Example{L"__abc__ tail\n", U"abc tail"},
                Example{L"_abc_ tail\n", U"abc tail"},
                Example{L"*abc* tail\n", U"abc tail"},
                Example{L"~~abc~~ tail\n", U"abc tail"},
                Example{L"`abc` tail\n", U"abc tail"},
                Example{L"**a\U0001F600b** tail\n", U"a\U0001F600b tail"},
            }) {
                EditorSession session;
                session.SetText(example.source);
                auto original = session.Text();
                auto owner = session.Selection().active.container_id;
                auto source = session.EditableSource(owner);
                expect(source.has_value());
                if (!source) continue;
                MathJaxRenderer math;
                SvgNormalizer normalizer;
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto const& block = frame.renderModel.blocks.front();
                auto reading = BuildDisplayInlineText(block.inline_items, {},
                    {owner, source->size(), folia::TextAffinity::Upstream}, math, normalizer,
                    D2D1::ColorF(D2D1::ColorF::Black), 16, 500, false, false, false);
                expect(std::u32string_view{reading.text} == example.visible);
                expect(std::none_of(reading.ranges.begin(), reading.ranges.end(), [](auto const& range) { return range.marker; }));
                auto constructEnd = source->find(U" tail");
                for (std::size_t offset = 0; offset <= source->size(); ++offset) {
                    auto caret = folia::TextPosition{owner, offset, folia::TextAffinity::Downstream};
                    auto display = BuildDisplayInlineText(block.inline_items, caret,
                        {owner, source->size(), folia::TextAffinity::Upstream}, math, normalizer,
                        D2D1::ColorF(D2D1::ColorF::Black), 16, 500, false, false, false);
                    auto active = offset <= constructEnd;
                    expect(std::u32string_view{display.text} == (active ? std::u32string_view{*source} : example.visible)) << offset;
                    auto markers = std::count_if(display.ranges.begin(), display.ranges.end(), [](auto const& range) { return range.marker; });
                    expect(markers == (active ? 2 : 0));
                    auto wide = ToWide(display.text);
                    for (auto const& range : display.ranges) {
                        if (!range.marker) continue;
                        auto marker = wide.substr(range.start, range.length);
                        expect(marker.find_first_not_of(L"*_~`") == std::wstring::npos)
                            << "only delimiters may receive marker decoration";
                    }
                    expect(DisplayPositionForSource(display.displayToSource, caret) < display.displayToSource.size());
                    if (active) {
                        auto position = DisplayPositionForSource(display.displayToSource, caret);
                        expect(display.displayToSource[position].source_offset == offset);
                    }
                }
                expect(session.Text() == original);
                expect(math.Snapshot().entries == 0_u);
            }
        };

        "inline markers preserve nested spelling and incomplete syntax"_test = [] {
            for (auto text : {L"a***b***c\n", L"**abc\n", L"~~abc\n", L"\\*abc\\*\n", L"[title](<url>)\n"}) {
                EditorSession session;
                session.SetText(text);
                auto owner = session.Selection().active.container_id;
                auto source = session.EditableSource(owner);
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                MathJaxRenderer math;
                SvgNormalizer normalizer;
                auto display = BuildDisplayInlineText(frame.renderModel.blocks.front().inline_items,
                    {owner, 3, folia::TextAffinity::Downstream},
                    {owner, source->size(), folia::TextAffinity::Upstream}, math, normalizer,
                    D2D1::ColorF(D2D1::ColorF::Black), 16, 500, false, false, false);
                // Escapes reveal only at their own source span, not anywhere
                // in the paragraph. Other constructs encompass this caret.
                expect(static_cast<bool>(display.text == (*source == U"\\*abc\\*" ? U"*abc*" : *source)));
                for (auto const& range : display.ranges) {
                    expect(range.start + range.length <= display.displayToSource.size() - 1);
                }
                expect(static_cast<bool>(session.Text() == winrt::hstring{text}));
            }
        };

        "marker geometry excludes the styled body across wrapped DirectWrite lines"_test = [] {
            EditorRenderResources resources;
            winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(resources.dwriteFactory.GetAddressOf())));
            ::Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
            winrt::check_hresult(resources.dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr,
                DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                16.0f, L"en-us", format.GetAddressOf()));
            EditorStyleSheet sheet;
            EditorTextLayoutEngine engine(resources, sheet);
            auto layout = engine.Create(L"**alpha beta gamma delta emoji \U0001F600 epsilon**", format.Get(), 100.0f);
            expect(static_cast<bool>(layout));
            auto text = std::wstring{L"**alpha beta gamma delta emoji \U0001F600 epsilon**"};
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, {2, 5});
            auto opening = InlineRangeRectangles(layout.Get(), 0, 2, {0, 0});
            auto closing = InlineRangeRectangles(layout.Get(), static_cast<UINT32>(text.size() - 2), 2, {0, 0});
            expect(opening.size() == 1_u);
            expect(closing.size() == 1_u);
            if (opening.empty() || closing.empty()) return;
            expect(closing.front().top > opening.front().bottom);
            expect(opening.front().right - opening.front().left < 20.0f);
            expect(closing.front().right - closing.front().left < 20.0f);
            FLOAT x = 0, y = 0;
            DWRITE_HIT_TEST_METRICS hit{};
            winrt::check_hresult(layout->HitTestTextPosition(2, false, &x, &y, &hit));
            expect(opening.front().right <= x + 0.5f) << "opening decoration must stop before body";
        };

        "math and link marker decorations exclude their editable content"_test = [] {
            struct Example { wchar_t const* source; std::wstring_view markers; std::wstring_view body; };
            for (auto example : {
                Example{L"$a+b$ tail\n", L"$$", L"a+b tail"},
                Example{L"\\(a+b\\) tail\n", L"\\(\\)", L"a+b tail"},
                Example{L"[title](<url>) tail\n", L"[](<url>)", L"title tail"},
                Example{L"[a&amp;b](url) tail\n", L"[](url)", L"a&amp;b tail"},
                Example{L"![alt](url) tail\n", L"![](url)", L"alt tail"},
                Example{L"**bold** tail\n", L"****", L"bold tail"},
            }) {
                EditorSession session;
                session.SetText(example.source);
                auto owner = session.Selection().active.container_id;
                auto source = session.EditableSource(owner);
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                MathJaxRenderer math;
                SvgNormalizer normalizer;
                auto display = BuildDisplayInlineText(frame.renderModel.blocks.front().inline_items,
                    {owner, 3, folia::TextAffinity::Downstream},
                    {owner, source->size(), folia::TextAffinity::Upstream}, math, normalizer,
                    D2D1::ColorF(D2D1::ColorF::Black), 16, 500, false, false, false);
                auto wide = ToWide(display.text);
                std::wstring markers, body;
                for (auto const& range : display.ranges)
                    (range.marker ? markers : body).append(wide, range.start, range.length);
                expect(static_cast<bool>(markers == example.markers));
                expect(static_cast<bool>(body == example.body));
                expect(static_cast<bool>(display.text == *source));
            }
        };

        "format controls reflect selection and use the existing reversible source edit"_test = [] {
            EditorSession session;
            session.SetText(L"__a\U0001F600b__ tail\n");
            auto original = session.Text();
            auto owner = session.Selection().active.container_id;
            auto caret = folia::TextPosition{owner, 3, folia::TextAffinity::Downstream};
            session.SetSelection(caret, caret);
            expect(session.ActiveInlineStyle().bold);
            expect(!session.ActiveInlineStyle().italic);
            folia::Command toggle;
            toggle.kind = folia::CommandKind::ToggleStrong;
            expect(session.ExecuteCommand(toggle));
            expect(!session.ActiveInlineStyle().bold);
            expect(session.EditableSource(owner) == std::optional<std::u32string>{U"a\U0001F600b tail"});
            folia::Command undo;
            undo.kind = folia::CommandKind::Undo;
            expect(session.ExecuteCommand(undo));
            expect(session.Text() == original);
            expect(session.Selection() == folia::TextSelection::caret(caret));
            expect(session.ActiveInlineStyle().bold);
            expect(session.EnterSourceMode());
            expect(!session.ActiveInlineStyle().bold);
            expect(session.Text() == original);
            expect(session.ExitSourceMode());
            expect(session.Text() == original);
        };

        "syntax icon hit testing keeps each delimiter indivisible in the real renderer"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"**a\U0001F600b** tail\n");
            auto original = session.Text();
            auto owner = session.Selection().active.container_id;
            session.SetSelection(folia::TextSelection::caret({owner, 3, folia::TextAffinity::Downstream}));
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 500, 350);
            renderer.Render(session.RenderFrame());
            for (std::size_t offset : {0u, 2u, 3u, 5u, 7u}) {
                auto position = folia::TextPosition{owner, offset, folia::TextAffinity::Downstream};
                auto bounds = renderer.CaretBounds(position);
                expect(bounds.has_value());
                if (!bounds) continue;
                auto hit = renderer.HitTest(bounds->left + .1f, (bounds->top + bounds->bottom) * .5f);
                expect(hit.has_value());
                if (hit) {
                    expect(hit->container_id == owner);
                    expect(hit->source_offset == offset);
                }
            }
            for (auto start : {0u, 5u}) {
                auto left = renderer.CaretBounds({owner, start, folia::TextAffinity::Downstream});
                auto right = renderer.CaretBounds({owner, start + 2, folia::TextAffinity::Downstream});
                expect(left.has_value() && right.has_value());
                if (!left || !right) continue;
                for (float x = left->left; x <= right->left; x += .5f) {
                    auto hit = renderer.HitTest(x, (left->top + left->bottom) * .5f);
                    expect(hit.has_value());
                    if (hit) expect(hit->source_offset == start || hit->source_offset == start + 2);
                }
            }
            auto contentStart = folia::TextSelection::caret({owner, 2, folia::TextAffinity::Downstream});
            session.SetSelection(contentStart);
            expect(session.ExecuteCommand(folia::Command::InsertText(U"x")));
            expect(session.EditableSource(owner) == std::optional<std::u32string>{U"**xa\U0001F600b** tail"});
            folia::Command undo;
            undo.kind = folia::CommandKind::Undo;
            expect(session.ExecuteCommand(undo));
            expect(session.Text() == original);
            expect(session.Selection() == contentStart);
        };

        "revealing long inline format markers cannot turn a stationary click into a selection"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            session.SetText(L"**alpha beta gamma delta epsilon zeta eta theta** tail\n");
            auto original = session.Text();
            auto owner = session.Selection().active.container_id;
            session.SetSelection(folia::TextSelection::caret({owner, 53, folia::TextAffinity::Upstream}));
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 380, 600);
            renderer.Render(session.RenderFrame());
            auto bounds = renderer.CaretBounds({owner, 8, folia::TextAffinity::Downstream});
            expect(bounds.has_value());
            if (!bounds) return;
            float x = bounds->left;
            float y = (bounds->top + bounds->bottom) * 0.5f;
            auto hit = renderer.HitTest(x, y);
            expect(hit.has_value());
            if (!hit) return;
            EditorScrollController scroll;
            EditorTextInputController input;
            EditorSelectionDrag drag;
            drag.Attach(session, renderer, scroll, input, panel);
            drag.Begin(*hit, x, y);
            auto pressed = session.Selection();
            renderer.Render(session.RenderFrame());
            auto expanded = renderer.HitTest(x, y);
            expect(!expanded || *expanded != *hit) << "test must exercise actual marker reflow";
            expect(!drag.Update(x, y));
            expect(!drag.Finish(x, y));
            expect(session.Selection() == pressed);
            expect(!session.HasSelection());
            expect(session.Text() == original);
            drag.Detach();
        };
    };
}
