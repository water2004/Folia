#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorSvgPainter.h"
#include "editor/rendering/EditorDocumentPreparationPass.h"
#include "editor/session/EditorSession.h"
import folia.markdown.instrumentation;

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
suite completion_layout_tests = [] {
    "completion layout cache invalidates only generated content at the current owner"_test = [] {
        auto sheet = CreateEditorStyleSheet(folia::default_theme_profile());
        EditorRenderResources resources; resources.InitializeOffscreen(sheet, 600, 400); resources.EnsureFrameResources(sheet);
        EditorRenderCache cache; MathJaxRenderer math; MermaidRenderer mermaid;
        SvgNormalizer normalizer; TreeSitterHighlighter syntax; EditorInteractionMap interaction;
        EditorTextLayoutEngine engine(resources, sheet);
        EditorSession session; session.SetText(L"abc\n\nuntouched");
        auto frame = session.RenderFrame();
        if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
        EditorSvgPainter svg(resources, cache);
        EditorInlineImageRenderer images(resources, cache, sheet, normalizer, svg, frame.baseDirectory);
        EditorDocumentPainter painter(resources, sheet, interaction, syntax, {}, false, 180,
            frame.selection, {}, frame.renderModel.editable_index);
        folia::platform::editor::EditorScrollState scroll;
        std::unique_ptr<EditorPreparedDocument> document;
        auto prepare = [&] {
            EditorDocumentBlockPreparer preparer(frame, resources, cache, sheet, engine, images, painter,
                math, mermaid, normalizer, syntax, frame.selection.active, 180, true, true, false, 0, 0);
            EditorDocumentPreparationPass pass(frame, resources, sheet, images, preparer, scroll, document,
                frame.selection, 180, 1, 0, 0, 0, false);
            (void)pass.Prepare();
        };
        prepare(); auto baseline = document->blocks.front().height;
        auto untouched = document->blocks.back().layout.Get();
        frame.completion = winrt::Folia::detail::EditorCompletionPreview{frame.selection.active,
            U" generated text that must wrap to several lines at this narrow width", 1};
        prepare(); expect(document->blocks.front().height > baseline);
        expect(document->blocks.back().layout.Get() == untouched);
        frame.completion = winrt::Folia::detail::EditorCompletionPreview{frame.selection.active, U" hi", 2};
        prepare(); expect(document->blocks.front().display.text.find(U"generated") == std::u32string::npos);
        frame.completion.reset(); prepare();
        expect(document->blocks.front().height == baseline);
        expect(std::ranges::none_of(document->blocks.front().display.ranges, [](auto const& range) { return range.completion; }));
        expect(document->blocks.back().layout.Get() == untouched);
    };
    "completion layout shares native flow and theme brush across editable containers"_test = [] {
        for (auto theme : {folia::Theme::Light, folia::Theme::Dark}) {
            auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(theme));
            EditorRenderResources resources; resources.InitializeOffscreen(sheet, 600, 400); resources.EnsureFrameResources(sheet);
            EditorRenderCache cache; MathJaxRenderer math; MermaidRenderer mermaid;
            SvgNormalizer normalizer; TreeSitterHighlighter syntax; EditorInteractionMap interaction;
            EditorTextLayoutEngine engine(resources, sheet);
            for (auto source : {L"abc", L"# abc", L"> abc", L"- abc", L"- [ ] abc", L"| abc |\n| --- |\n| def |", L"```cpp\nabc\n```", L""}) {
                EditorSession session; session.SetText(source);
                auto position = session.Selection().active;
                if (auto text = session.EditableSource(position.container_id)) {
                    auto at = text->find(U"abc");
                    if (at != std::u32string::npos) position.source_offset = at + 3;
                }
                session.SetSelection(folia::TextSelection::caret(position));
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto caret = session.Selection().active;
                std::u32string ghost = U" generated plain **text** that wraps onto another line";
                frame.completion = winrt::Folia::detail::EditorCompletionPreview{caret, ghost, 1};
                auto original = session.TextUtf8(); auto selection = session.Selection();
                folia::reset_core_operation_counters();
                EditorSvgPainter svg(resources, cache);
                EditorInlineImageRenderer images(resources, cache, sheet, normalizer, svg, frame.baseDirectory);
                EditorDocumentPainter painter(resources, sheet, interaction, syntax, {}, false, 180,
                    selection, {}, frame.renderModel.editable_index);
                EditorDocumentBlockPreparer preparer(frame, resources, cache, sheet, engine, images, painter,
                    math, mermaid, normalizer, syntax, caret, 180, true, true, theme == folia::Theme::Dark, 0, 0);
                unsigned found = 0;
                auto inspect = [&](DisplayInlineText const& display, IDWriteTextLayout* layout) {
                    for (auto const& range : display.ranges) if (range.completion) {
                        ++found; expect(layout != nullptr); expect(!range.style.bold && !range.marker);
                        expect(display.text.find(U"**text**") != std::u32string::npos);
                        if (!layout) continue;
                        ::Microsoft::WRL::ComPtr<IUnknown> effect;
                        winrt::check_hresult(layout->GetDrawingEffect(range.start, effect.GetAddressOf()));
                        expect(effect.Get() == resources.mutedBrush.Get());
                        DWRITE_TEXT_METRICS metrics{}; winrt::check_hresult(layout->GetMetrics(&metrics));
                        expect(metrics.lineCount > 1_u);
                        auto displayOffset = folia::platform::editor::DisplayPositionForSource(display.displayToSource, caret);
                        expect(displayOffset == range.start);
                        for (auto offset = range.start; offset < range.start + range.length; ++offset)
                            expect(display.displayToSource[offset].kind != EditorDisplayPositionKind::Source)
                                << "preview and its generated wrap indentation must not own source characters";
                    }
                };
                for (auto const& block : frame.renderModel.blocks) {
                    auto prepared = preparer.Prepare(block, false, false);
                    if (prepared.table) {
                        for (std::size_t index = 0; index < prepared.table->displays.size(); ++index) {
                            auto display = prepared.table->displays[index];
                            auto const& cell = prepared.table->visual.cells[index];
                            display.text = cell.text; display.displayToSource = cell.displayToSource;
                            inspect(display, cell.layout.Get());
                        }
                    } else inspect(prepared.display, prepared.layout.Get());
                }
                expect(found == 1_u) << winrt::to_string(source);
                auto counts = folia::read_core_operation_counters();
                expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
                expect(session.Selection() == selection); expect(session.TextUtf8() == original);
            }
        }
    };
};
}
