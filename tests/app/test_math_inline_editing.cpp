#include "pch.h"
#include "support/folia_test.hpp"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include "editor/interaction/EditorSelectionDrag.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorMathPreview.h"

import folia.markdown.instrumentation;

import folia.core.command;
import folia.markdown.utf;

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    struct MathDragFixture
    {
        winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager xaml{
            winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread()};
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
        EditorSession session;
        EditorSurfaceRenderer renderer;
        EditorScrollController scroll;
        EditorTextInputController textInput;
        EditorSelectionDrag drag;
        bool formulasReady = true;
        std::size_t completions = 0;
        std::chrono::steady_clock::time_point lastCompletion = std::chrono::steady_clock::now();

        explicit MathDragFixture(winrt::hstring const& source)
        {
            session.SetText(source);
            renderer.Initialize(panel);
            renderer.Resize(panel, 540, 720);
            auto context = renderer.CaptureMathPreparationContext();
            auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            auto sheet = CreateEditorStyleSheet(context.theme);
            auto width = EditorDocumentContentWidth(context.surfaceWidth, sheet.horizontalPadding);
            auto checkFormula = [&](std::u32string_view tex, bool display, float available)
            {
                auto result = prepared->math.Lookup(folia::cps_to_utf8(tex), display,
                    sheet.body.size, available, SvgColor(sheet.textColor));
                auto ready = result && static_cast<bool>(*result);
                expect(ready) << "math-edit-initial-svg-ready";
                formulasReady = formulasReady && ready;
                if (!ready)
                    std::cout << "math-edit initial SVG failure: "
                        << (result ? result->error : "prepared formula was not found") << '\n';
            };
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            for (auto const& block : frame.renderModel.blocks)
            {
                auto available = EditorBlockContentWidth(block, width);
                if (block.kind == folia::RenderBlockKind::Math)
                    checkFormula(block.special().tex, true, available);
                for (auto const& item : block.inline_items)
                    if (item.kind == folia::InlineRenderItem::Kind::Math)
                        checkFormula(item.text, item.special().display == folia::MathDisplayMode::Block, available);
            }
            renderer.AdoptMathPreparation(std::move(prepared));
            renderer.SetInvalidateCallback([this]
            {
                ++completions;
                lastCompletion = std::chrono::steady_clock::now();
                renderer.Render(session.RenderFrame());
            });
            renderer.Render(session.RenderFrame());
            Settle();
            drag.Attach(session, renderer, scroll, textInput, panel);
        }

        ~MathDragFixture()
        {
            drag.Detach();
            renderer.SetInvalidateCallback({});
        }

        bool Settle(std::optional<std::size_t> previousCompletions = std::nullopt)
        {
            // Completion is dispatched through the production invalidation
            // queue. Require the preview callback when requested, then a quiet
            // interval, rather than assuming one synchronous Render is enough.
            auto start = std::chrono::steady_clock::now();
            auto deadline = start + std::chrono::seconds(4);
            while (std::chrono::steady_clock::now() < deadline)
            {
                MSG message{};
                if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
                auto now = std::chrono::steady_clock::now();
                if ((!previousCompletions || completions > *previousCompletions)
                    && now - (std::max)(start, lastCompletion) > std::chrono::milliseconds(100))
                    return true;
                ::MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
            return false;
        }
    };

    struct MathClick
    {
        folia::TextPosition position;
        float x = 0;
        float y = 0;
    };

    std::optional<folia::TextSpan> FindFormula(EditorSession& session, bool blockMath)
    {
        auto frame = session.RenderFrame();
        if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
        for (auto const& block : frame.renderModel.blocks)
        {
            if (blockMath && block.kind == folia::RenderBlockKind::Math)
                return block.content_span;
            for (auto const& item : block.inline_items)
                if (!blockMath && item.kind == folia::InlineRenderItem::Kind::Math)
                    return item.source_span;
        }
        return std::nullopt;
    }

    std::optional<MathClick> FindMathClick(EditorSurfaceRenderer& renderer, folia::TextSpan span)
    {
        // Use the real formula hit map, including multiline fragments. Select
        // an interior source position rather than a delimiter or line edge.
        auto midpoint = span.source_range.start + span.source_range.length() / 2;
        std::optional<MathClick> result;
        auto bestDistance = (std::numeric_limits<std::size_t>::max)();
        for (float y = 16; y < 680; y += 4)
            for (float x = 24; x < 516; x += 4)
            {
                auto hit = renderer.HitTest(x, y);
                if (!hit || hit->container_id != span.container_id
                    || hit->source_offset <= span.source_range.start + 2
                    || hit->source_offset + 2 >= span.source_range.end) continue;
                auto distance = hit->source_offset > midpoint
                    ? hit->source_offset - midpoint : midpoint - hit->source_offset;
                if (distance < bestDistance)
                {
                    result = MathClick{*hit, x, y};
                    bestDistance = distance;
                }
            }
        return result;
    }

    void CheckMathGesture(winrt::hstring const& source, bool blockMath)
    {
        MathDragFixture fixture(source);
        // A completed native job can contain an infrastructure error. In
        // particular, the test deployment must place folia_svg_normalizer.dll
        // beside this executable, as production resolves it by module path.
        // Source fallback is not a valid starting state for this regression.
        if (!fixture.formulasReady) return;
        auto formula = FindFormula(fixture.session, blockMath);
        expect(formula.has_value()) << "math-edit-01 formula-found";
        if (!formula) return;
        auto click = FindMathClick(fixture.renderer, *formula);
        expect(click.has_value()) << "math-edit-02 click-found";
        if (!click) return;
        auto originalDocument = fixture.session.TextUtf8();
        auto originalSource = fixture.session.EditableSource(click->position.container_id);
        expect(originalSource.has_value()) << "math-edit-03 source-found";
        if (!originalSource) return;
        std::cout << "math-edit input block=" << blockMath
            << " click=(" << click->x << ',' << click->y << ") source="
            << click->position.source_offset << " affinity="
            << static_cast<int>(click->position.affinity) << " span=["
            << formula->source_range.start << ',' << formula->source_range.end
            << "] completions=" << fixture.completions << '\n';

        auto beforePreview = fixture.completions;
        fixture.drag.Begin(click->position, click->x, click->y);
        auto pressedSelection = fixture.session.Selection();
        fixture.renderer.Render(fixture.session.RenderFrame());
        expect(fixture.session.Selection() == pressedSelection) << "math-edit-04 press-render-selection";
        expect(!fixture.drag.Update(click->x, click->y)) << "math-edit-05 stationary-update";
        expect(fixture.Settle(beforePreview)) << "math-edit-06 preview-completed";
        auto expandedHit = fixture.renderer.HitTest(click->x, click->y);
        std::cout << "math-edit expanded hit="
            << (expandedHit ? std::to_string(expandedHit->source_offset) : "none")
            << " affinity=" << (expandedHit ? static_cast<int>(expandedHit->affinity) : -1)
            << " completions=" << fixture.completions << " previous=" << beforePreview << '\n';
        // Ensure this scenario actually exercises a changed hit map; without
        // reflow, even the former release-time re-hit could pass accidentally.
        expect(!expandedHit || *expandedHit != click->position) << "math-edit-07 hit-map-changed";
        expect(fixture.session.Selection() == pressedSelection) << "math-edit-08 preview-selection";
        if (blockMath) expect(!fixture.renderer.InlineMathPreviewBounds()) << "display math keeps its existing preview placement";
        expect(!fixture.drag.Finish(click->x, click->y)) << "math-edit-09 stationary-release";
        expect(!fixture.drag.Active()) << "math-edit-10 gesture-finished";
        expect(!fixture.session.HasSelection()) << "math-edit-11 click-collapsed";
        expect(fixture.session.Selection() == pressedSelection) << "math-edit-12 release-selection";

        // The source is already expanded here. Small movement must still be
        // measured from the physical press, never from the relocated caret.
        fixture.drag.Begin(click->position, click->x, click->y);
        expect(!fixture.drag.Update(click->x + 3, click->y - 3)) << "math-edit-13 jitter-update";
        fixture.renderer.Render(fixture.session.RenderFrame());
        expect(!fixture.drag.Finish(click->x - 3, click->y + 3)) << "math-edit-14 jitter-release";
        expect(fixture.session.Selection() == pressedSelection) << "math-edit-15 jitter-selection";
        expect(fixture.session.TextUtf8() == originalDocument) << "math-edit-16 jitter-source";

        // Find a real editable hit in the expanded source far enough away to
        // cross the gesture threshold. This proves dragging was not disabled
        // as a workaround for the click/reflow problem.
        std::optional<MathClick> destination;
        for (float y = 24; y < 680 && !destination; y += 8)
            for (float x = 24; x < 516; x += 8)
            {
                if (std::abs(x - click->x) < 8 && std::abs(y - click->y) < 8) continue;
                auto hit = fixture.renderer.HitTest(x, y);
                if (hit && hit->container_id == click->position.container_id
                    && hit->source_offset != click->position.source_offset)
                {
                    destination = MathClick{*hit, x, y};
                    break;
                }
            }
        expect(destination.has_value()) << "math-edit-17 destination-found";
        if (!destination) return;
        fixture.drag.Begin(click->position, click->x, click->y);
        expect(fixture.drag.Update(destination->x, destination->y)) << "math-edit-18 real-drag-update";
        expect(fixture.session.HasSelection()) << "math-edit-19 real-drag-selected";
        expect(fixture.session.Selection().anchor == click->position) << "math-edit-20 real-drag-anchor";
        expect(fixture.session.Selection().active == destination->position) << "math-edit-21 real-drag-endpoint";
        fixture.drag.Finish(destination->x, destination->y);
        expect(fixture.session.HasSelection()) << "math-edit-22 real-drag-release-selected";

        fixture.session.SetSelection(pressedSelection);
        auto acp = fixture.session.TextInputAcpOffset(click->position);
        auto roundtrip = fixture.session.TextInputPositionFromAcp(click->position.container_id, acp);
        std::cout << "math-edit acp=" << acp << " roundtrip=" << roundtrip.source_offset
            << " affinity=" << static_cast<int>(roundtrip.affinity) << '\n';
        expect(fixture.session.TextInputPositionFromAcp(click->position.container_id, acp)
            == click->position) << "math-edit-23 acp-roundtrip";
        expect(fixture.session.ExecuteCommand(folia::Command::InsertText(U"z"))) << "math-edit-24 insert-command";
        auto expectedSource = *originalSource;
        expectedSource.insert(click->position.source_offset, U"z");
        expect(fixture.session.EditableSource(click->position.container_id)
            == std::optional<std::u32string>{expectedSource}) << "math-edit-25 insert-source";
        expect(fixture.session.Selection().active.source_offset == click->position.source_offset + 1) << "math-edit-26 insert-caret";
        folia::Command undo;
        undo.kind = folia::CommandKind::Undo;
        expect(fixture.session.ExecuteCommand(undo)) << "math-edit-27 undo-command";
        expect(fixture.session.EditableSource(click->position.container_id) == originalSource) << "math-edit-28 undo-source";
        expect(fixture.session.TextUtf8() == originalDocument) << "math-edit-29 undo-document";
        expect(fixture.session.Selection() == pressedSelection) << "math-edit-30 undo-selection";
    }

    suite math_inline_editing_tests = []
    {
        "math floating preview placement stays in viewport and clear of its editing line"_test = [] {
            for (auto viewport : {D2D1::SizeF(180, 90), D2D1::SizeF(320, 160), D2D1::SizeF(540, 720)})
            for (float width : {20.f, 200.f, 2000.f}) for (float height : {18.f, 100.f, 2000.f})
            for (float fraction : {.1f, .5f, .9f}) {
                auto caret = D2D1::RectF(viewport.width * fraction, viewport.height * fraction,
                    viewport.width * fraction + 1, viewport.height * fraction + 20);
                auto placement = PlaceInlineMathPreview(caret, caret.left, {width, height}, viewport);
                if (!placement) continue;
                auto rect = placement->bounds;
                expect(rect.left >= 11.99f && rect.top >= 11.99f);
                expect(rect.right <= viewport.width - 11.99f && rect.bottom <= viewport.height - 11.99f);
                expect(rect.bottom <= caret.top - 7.99f || rect.top >= caret.bottom + 7.99f);
                expect(placement->origin.x == rect.left + 12.f);
                expect(placement->origin.y == rect.top + 10.f);
            }
            expect(!PlaceInlineMathPreview({0, -40, 1, -20}, 0, {20, 20}, {540, 720}));
            expect(!PlaceInlineMathPreview({0, 750, 1, 770}, 0, {20, 20}, {540, 720}));
            expect(!PlaceInlineMathPreview({10, 20, 11, 40}, 10, {20, 20}, {20, 40}));
        };

        "math floating inline preview does not move following content when asynchronous rendering completes"_test = [] {
            for (auto source : {L"Introduction.\n\nBefore $\\frac{x+1}{y+2}+\\sqrt{a+b}$ after.\n\nFollowing text.\n",
                    L"Introduction.\n\n> Before $\\frac{x+1}{y+2}$ after.\n\nFollowing text.\n",
                    L"Introduction.\n\n- Before $\\frac{x+1}{y+2}$ after.\n\nFollowing text.\n"}) {
                MathDragFixture f(source);
                auto formula = FindFormula(f.session, false);
                expect(formula.has_value());
                if (!formula) continue;
                auto position = folia::TextPosition{formula->container_id, formula->source_range.start + 2, folia::TextAffinity::Downstream};
                f.session.SetSelection(position, position);
                auto text = f.session.TextUtf8();
                auto frame = f.session.RenderFrame();
                auto following = folia::TextPosition{frame.renderModel.blocks.back().id, 0, folia::TextAffinity::Downstream};
                folia::reset_core_operation_counters();
                auto beforePreview = f.completions;
                f.renderer.Render(frame);
                auto before = f.renderer.CaretBounds(following);
                expect(before.has_value());
                expect(f.Settle(beforePreview));
                auto bounds = f.renderer.InlineMathPreviewBounds();
                auto after = f.renderer.CaretBounds(following);
                expect(bounds.has_value());
                expect(after.has_value());
                if (before && after) expect(std::abs(before->top - after->top) < .01f);
                auto caret = f.renderer.CaretBounds(position);
                if (bounds && caret) {
                    expect(bounds->bottom <= caret->top || bounds->top >= caret->bottom);
                    expect(!f.renderer.HitTest((bounds->left + bounds->right) / 2, (bounds->top + bounds->bottom) / 2));
                }
                auto counters = folia::read_core_operation_counters();
                expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
                expect(f.session.Selection().active == position);
                expect(f.session.TextUtf8() == text);
                f.session.SetSelection(following, following);
                f.renderer.Render(f.session.RenderFrame());
                expect(!f.renderer.InlineMathPreviewBounds());
                expect(f.session.EnterSourceMode());
                f.renderer.Render(f.session.RenderFrame());
                expect(!f.renderer.InlineMathPreviewBounds());
            }
        };

        "math floating table preview leaves the next row in place"_test = [] {
            MathDragFixture f(L"Introduction.\n\n| Before $\\frac{x+1}{y+2}$ after | B |\n| --- | --- |\n| next row | data |\n\nFollowing text.\n");
            auto frame = f.session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            std::optional<folia::TextSpan> formula;
            folia::TextPosition following{};
            for (auto const& block : frame.renderModel.blocks) if (block.kind == folia::RenderBlockKind::Table) {
                for (auto const& cell : block.special().table_cells) for (auto const& item : cell)
                    if (item.kind == folia::InlineRenderItem::Kind::Math) formula = item.source_span;
                auto span = block.special().table_cell_spans[block.special().column_count];
                following = {span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
            }
            expect(formula.has_value());
            if (!formula) return;
            auto position = folia::TextPosition{formula->container_id, formula->source_range.start + 2, folia::TextAffinity::Downstream};
            f.session.SetSelection(position, position);
            auto beforePreview = f.completions;
            f.renderer.Render(f.session.RenderFrame());
            auto before = f.renderer.CaretBounds(following);
            expect(before.has_value());
            expect(f.Settle(beforePreview));
            auto after = f.renderer.CaretBounds(following);
            expect(after.has_value());
            expect(f.renderer.InlineMathPreviewBounds().has_value());
            if (before && after) expect(std::abs(before->top - after->top) < .01f);
        };

        "math inline click remains collapsed after source expansion and preview"_test = []
        {
            CheckMathGesture(L"Introduction.\n\n**bold** \U0001f642 $\\frac{x+1}{y+2}+\\sqrt{a+b}$ tail.\n", false);
        };

        "math long inline click and real drag survive source reflow"_test = []
        {
            CheckMathGesture(L"Introduction.\n\n\U0001f642 **before** $\\frac{a_1+b_1}{c_1+d_1}+\\frac{a_2+b_2}{c_2+d_2}+\\frac{a_3+b_3}{c_3+d_3}+\\frac{a_4+b_4}{c_4+d_4}+\\frac{a_5+b_5}{c_5+d_5}+\\sqrt{x^2+y^2}$ after.\n", false);
        };

        "math long block click and real drag preserve source and undo"_test = []
        {
            CheckMathGesture(L"Introduction \U0001f642.\n\n$$\n\\frac{a_1+b_1}{c_1+d_1}+\\frac{a_2+b_2}{c_2+d_2}+\\frac{a_3+b_3}{c_3+d_3}+\\frac{a_4+b_4}{c_4+d_4}+\\frac{a_5+b_5}{c_5+d_5}+\\sqrt{x^2+y^2}\n$$\n", true);
        };
    };
}
