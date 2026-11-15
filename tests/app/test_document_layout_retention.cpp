#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorPreparedDocumentCache.h"
#include "editor/rendering/EditorDocumentPreparationPass.h"
#include <psapi.h>

import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_fragments;
import folia.platform.editor_display_mapping;

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    // Real DirectWrite, production display projection and line breaking. The
    // measurement path deliberately has no XAML window or D3D device.
    struct MeasurementFixture {
        EditorStyleSheet sheet = CreateEditorStyleSheet(folia::default_theme_profile());
        EditorRenderResources resources;
        EditorRenderCache resourcesCache;
        MathJaxRenderer math;
        MermaidRenderer mermaid;
        SvgNormalizer normalizer;
        TreeSitterHighlighter syntax;
        EditorInteractionMap interaction;
        EditorScrollState scroll;
        std::unique_ptr<EditorPreparedDocument> document;

        explicit MeasurementFixture(float width = 600) {
            resources.InitializeTextMeasurement(sheet, width, 400);
        }

        template <class Callback>
        void WithPreparer(winrt::Folia::detail::EditorRenderFrame const& frame, Callback callback) {
            auto metrics = EditorDocumentLayoutMetrics(frame.renderModel, resources.surfaceWidthDip,
                sheet.horizontalPadding, sheet.code.size);
            EditorTextLayoutEngine engine(resources, sheet);
            EditorSvgPainter svg(resources, resourcesCache);
            EditorInlineImageRenderer images(resources, resourcesCache, sheet, normalizer,
                svg, frame.baseDirectory, false);
            EditorDocumentPainter painter(resources, sheet, interaction, syntax, {}, false,
                metrics.right, frame.selection, {}, frame.renderModel.editable_index);
            EditorDocumentBlockPreparer preparer(frame, resources, resourcesCache, sheet,
                engine, images, painter, math, mermaid, normalizer, syntax,
                frame.selection.active, metrics.width, true, true, false, 0, 0);
            callback(preparer, images, metrics.width);
        }
    };

    std::string RepeatedList(std::size_t count, std::string_view marker, bool loose = false) {
        std::string source;
        for (std::size_t index = 0; index < count; ++index) {
            source += marker;
            source += "item ";
            source += std::to_string(index);
            source += " with **bold** and a wrapping phrase, 中文, e\xCC\x81 and 😀.\n";
            if (loose) source += '\n';
        }
        return source;
    }

    float SourceDocumentTop(MeasurementFixture const& fixture, folia::TextPosition position) {
        auto index = fixture.document->ownerBlockIndex.at(position.container_id.v);
        auto const& block = fixture.document->blocks[index];
        expect(fatal(block.layout != nullptr));
        auto display = folia::platform::editor::DisplayPositionForSource(
            block.display.displayToSource, position);
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS hit{};
        expect(fatal(SUCCEEDED(block.layout->HitTestTextPosition(
            static_cast<UINT32>(display), FALSE, &x, &y, &hit))));
        return fixture.document->geometry.At(index).top + block.textTop + y;
    }

    void PrepareFrame(MeasurementFixture& fixture, winrt::Folia::detail::EditorRenderFrame const& frame) {
        fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
            for (std::size_t retry = 0; retry < 32; ++retry) {
                EditorDocumentPreparationPass pass(frame, fixture.resources, fixture.sheet,
                    images, preparer, fixture.scroll, fixture.document, frame.selection,
                    width, 1, 0, 0, fixture.scroll.Offset(), false);
                if (!pass.Prepare().invalidateRequested) return;
            }
            expect(false) << "viewport preparation did not settle";
        });
    }

    void ScrollToSource(MeasurementFixture& fixture, winrt::Folia::detail::EditorRenderFrame const& frame,
                        folia::TextPosition position) {
        fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
            EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
            cache.Reconcile(frame.selection, width, 1);
            cache.EnsureGeometry();
            EditorViewportMaterializer materializer(frame, fixture.resources, preparer,
                fixture.scroll, fixture.document);
            materializer.EnsurePrepared(fixture.document->ownerBlockIndex.at(position.container_id.v),
                true, true);
            fixture.scroll.Set(SourceDocumentTop(fixture, position) + 3.0f,
                fixture.document->totalHeight);
        });
        PrepareFrame(fixture, frame);
    }

    suite document_layout_retention_tests = [] {
        "document layout fragments preserve real DirectWrite flow height"_test = [] {
            for (auto const& source : std::vector<std::string>{
                RepeatedList(193, "- "),
                RepeatedList(193, "98. "),
                RepeatedList(193, "- [x] "),
                RepeatedList(193, "- ", true),
                std::string{"- parent\n"} + RepeatedList(193, "  - ") + "- tail\n",
                std::string{"> [!NOTE]\n"} + RepeatedList(193, "> - "),
                RepeatedList(193, "> > - "),
            }) {
                auto parsed = folia::parse_text(1, source);
                expect(fatal(parsed.document.root.children.size() == 1_u));
                auto theme = folia::default_theme_profile();
                auto model = folia::build_render_model(parsed.document, parsed.outline, parsed.symbols, theme);
                folia::Builder reference(theme);
                folia::configure_render_dependencies(reference, parsed.symbols);
                auto whole = reference.build_block(parsed.document.root.children.front());
                std::wstring directory;
                winrt::Folia::detail::EditorRenderFrame frame{model, {}, {}, directory};
                MeasurementFixture fixture(460);
                fixture.WithPreparer(frame, [&](auto& preparer, auto&, auto) {
                    auto expected = preparer.Prepare(whole, false, false);
                    double actualHeight = 0;
                    for (auto const& block : model.blocks) {
                        auto measured = preparer.Prepare(block, false, false);
                        expect(measured.layout != nullptr);
                        actualHeight += block.block_style.margin_top + measured.height
                            + block.block_style.margin_bottom;
                    }
                    auto expectedHeight = whole.block_style.margin_top + expected.height
                        + whole.block_style.margin_bottom;
                    expect(std::abs(actualHeight - expectedHeight) < 0.25)
                        << "fragment boundaries changed native line height "
                        << actualHeight << " != " << expectedHeight;
                });
            }
        };

        "document layout measurement and scrolling retain bounded visual resources"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(RepeatedList(8193, "- ")));
            auto frame = session.RenderFrame();
            expect(fatal(frame.renderModel.virtualized));
            expect(fatal(frame.renderModel.blocks.size() > 100_u));
            MeasurementFixture fixture;
            fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
                EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
                cache.Reconcile(frame.selection, width, 1);
                cache.EnsureGeometry();
                expect(fixture.document->layoutBlocks.empty());
                expect(frame.renderModel.materialized_block_indices.empty());
                for (auto const& block : fixture.document->blocks) {
                    expect(block.geometryMeasured);
                    expect(block.height > 0);
                    expect(block.layout == nullptr);
                    expect(block.display.text.empty());
                    expect(!block.paintPlan);
                }
                auto exactHeight = fixture.document->geometry.PreciseTotalHeight();
                for (float fraction : {0.0f, 0.5f, 0.95f, 0.2f, 0.0f}) {
                    auto offset = static_cast<float>(exactHeight) * fraction;
                    fixture.scroll.Set(offset, static_cast<float>(exactHeight));
                    for (std::size_t retry = 0; retry < 32; ++retry) {
                        EditorDocumentPreparationPass pass(frame, fixture.resources, fixture.sheet,
                            images, preparer, fixture.scroll, fixture.document, frame.selection,
                            width, 1, 0, 0, offset, false);
                        auto result = pass.Prepare();
                        offset = result.scrollOffset;
                        if (!result.invalidateRequested) break;
                    }
                    expect(fixture.document->geometry.PreciseTotalHeight() == exactHeight);
                    expect(fixture.document->layoutBlocks.size() < 16_u);
                    expect(frame.renderModel.materialized_block_indices.size() < 16_u);
                }
            });
        };

        "document layout background preparation publishes geometry without graphics objects"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(RepeatedList(257, "- ")));
            EditorMathPreparationContext context{folia::default_theme_profile(), 1, 600, 400, true};
            auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            expect(fatal(prepared->document != nullptr));
            expect(prepared->document->geometry.Initialized());
            expect(prepared->document->totalHeight > 400);
            expect(prepared->document->layoutBlocks.empty());
            for (auto const& block : prepared->document->blocks) {
                expect(block.geometryMeasured);
                expect(block.layout == nullptr);
                expect(!block.paintPlan);
            }
            std::stop_source stop;
            bool canceled = false;
            auto fragments = session.RenderModel().blocks.size();
            try {
                (void)EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, stop.get_token(),
                    [&](std::size_t completed, std::size_t) {
                        if (completed == fragments + 1) stop.request_stop();
                    });
            } catch (winrt::hresult_canceled const&) { canceled = true; }
            expect(canceled);
        };

        "document layout anchors the visible source when earlier text wraps inside its fragment"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(RepeatedList(193, "- ")));
            auto frame = session.RenderFrame();
            auto first = frame.renderModel.editable_order.front();
            auto owner = frame.renderModel.editable_order[50];
            folia::TextPosition anchor{owner, 0, folia::TextAffinity::Downstream};
            MeasurementFixture fixture(460);
            ScrollToSource(fixture, frame, anchor);
            expect(fixture.document->ownerBlockIndex.at(owner.v) == 0_u);
            auto beforeOffset = fixture.scroll.Offset();
            auto screenY = SourceDocumentTop(fixture, anchor) - beforeOffset;
            expect(std::abs(screenY + 3.0f) < 0.25f);

            session.SetSelection(folia::TextSelection::caret({first, 0, folia::TextAffinity::Downstream}));
            std::u32string prefix;
            for (std::size_t index = 0; index < 40; ++index) prefix += U"extra wrapped text ";
            expect(fatal(session.ExecuteCommand(folia::Command::InsertText(std::move(prefix)))));
            PrepareFrame(fixture, session.RenderFrame());
            expect(fixture.scroll.Offset() > beforeOffset + fixture.sheet.body.lineHeight);
            expect(std::abs(SourceDocumentTop(fixture, anchor) - fixture.scroll.Offset() - screenY) < 0.25f)
                << "same-fragment reflow displaced the visible source line";
        };

        "document layout retains a source anchor through width and list boundary changes"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(RepeatedList(193, "> - ")));
            auto frame = session.RenderFrame();
            auto first = frame.renderModel.editable_order.front();
            auto owner = frame.renderModel.editable_order[63];
            folia::TextPosition anchor{owner, 0, folia::TextAffinity::Downstream};
            MeasurementFixture fixture(700);
            ScrollToSource(fixture, frame, anchor);
            auto screenY = SourceDocumentTop(fixture, anchor) - fixture.scroll.Offset();
            auto originalOffset = fixture.scroll.Offset();

            fixture.resources.surfaceWidthDip = 300;
            PrepareFrame(fixture, session.RenderFrame());
            expect(fixture.scroll.Offset() > originalOffset + fixture.sheet.body.lineHeight);
            expect(std::abs(SourceDocumentTop(fixture, anchor) - fixture.scroll.Offset() - screenY) < 0.25f)
                << "width reflow displaced the visible source line";

            auto beforeInsert = fixture.scroll.Offset();
            auto firstSource = session.EditableSource(first);
            expect(fatal(firstSource.has_value()));
            session.SetSelection(folia::TextSelection::caret(
                {first, firstSource->size(), folia::TextAffinity::Downstream}));
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::InsertNewline})));
            PrepareFrame(fixture, session.RenderFrame());
            expect(fixture.scroll.Offset() > beforeInsert);
            expect(std::abs(SourceDocumentTop(fixture, anchor) - fixture.scroll.Offset() - screenY) < 0.25f)
                << "fragment regrouping displaced the visible source line";
        };

        "document layout 10MiB synthetic list records native measurement memory"_test = [] {
            constexpr std::string_view line =
                "- Sample markdown list item for parser and upload testing.\n";
            std::string source;
            source.reserve(10 * 1024 * 1024 + line.size());
            while (source.size() < 10 * 1024 * 1024) source += line;
            auto started = std::chrono::steady_clock::now();
            auto report = [&](std::string_view stage) {
                PROCESS_MEMORY_COUNTERS_EX memory{};
                memory.cb = sizeof(memory);
                GetProcessMemoryInfo(GetCurrentProcess(),
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
                std::cerr << "document-scalability-native stage=" << stage
                    << " ms=" << std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count()
                    << " working_set_bytes=" << memory.WorkingSetSize
                    << " peak_working_set_bytes=" << memory.PeakWorkingSetSize
                    << " private_bytes=" << memory.PrivateUsage << '\n';
            };
            EditorSession session;
            session.SetText(winrt::to_hstring(source));
            report("parsed");
            expect(fatal(session.RenderModel().virtualized));
            auto fragments = session.RenderModel().blocks.size();
            expect(fragments > 2000_u);
            EditorMathPreparationContext context{folia::default_theme_profile(), 1, 1200, 800, true};
            auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            report("measured-and-released");
            expect(fatal(prepared->document != nullptr));
            expect(prepared->document->geometry.Size() == fragments);
            expect(prepared->document->layoutBlocks.empty());
            expect(session.RenderModel().materialized_block_indices.empty());
            for (auto const& block : prepared->document->blocks) {
                expect(block.geometryMeasured);
                expect(block.layout == nullptr);
                expect(block.display.text.empty());
                expect(!block.paintPlan);
            }
            std::cerr << "document-scalability-native fragments=" << fragments
                << " retained_layouts=" << prepared->document->layoutBlocks.size()
                << " exact_height=" << prepared->document->geometry.PreciseTotalHeight() << '\n';
        };
    };
}
