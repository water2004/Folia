#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/layout/EditorPreparedDocumentCache.h"
#include "editor/rendering/layout/EditorDocumentPreparationPass.h"
#include <psapi.h>

import folia.markdown.parser;
import folia.markdown.utf;
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
        folia::platform::MathJaxRenderer math;
        folia::platform::MermaidRenderer mermaid;
        folia::platform::SvgNormalizer normalizer;
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
                metrics.right, frame.selection, frame.tableTextOwner, {}, frame.renderModel.editable_index);
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
            if (loose && index + 1 < count) source += '\n';
        }
        return source;
    }

    std::string ShortList(std::size_t count, std::optional<std::pair<std::size_t, std::string>> special = {}) {
        std::string source;
        for (std::size_t index = 0; index < count; ++index) {
            source += "- ";
            source += special && special->first == index
                ? special->second : "item " + std::to_string(index);
            source += '\n';
        }
        return source;
    }

    // Compare the complete document extent with the original single-flow
    // DirectWrite path, including outer padding and its one trailing block gap.
    // The reference source is small and synthesized by these tests.
    EditorFlowLayoutContext ExpectMonolithicHeight(
        MeasurementFixture& fixture, winrt::Folia::detail::EditorRenderFrame const& frame,
        std::string const& source) {
        auto parsed = folia::parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1_u));
        auto theme = folia::default_theme_profile();
        folia::Builder reference(theme);
        folia::configure_render_dependencies(reference, parsed.symbols);
        auto whole = reference.build_block(parsed.document.root.children.front());
        EditorFlowLayoutContext expectedContext;
        fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
            auto expected = preparer.Prepare(whole, false, false);
            expect(fatal(expected.Visuals().layout != nullptr));
            expectedContext = expected.intrinsicFlow;
            auto expectedHeight = 2.0 * fixture.sheet.verticalPadding
                + whole.block_style.margin_top + expected.height
                + whole.block_style.margin_bottom + fixture.sheet.blockGap;
            EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
            cache.Reconcile(frame.selection, width, 1);
            cache.EnsureGeometry();
            expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - expectedHeight) < 0.25)
                << "shared flow typography changed the monolithic height "
                << fixture.document->geometry.PreciseTotalHeight() << " != " << expectedHeight;
            for (auto const& block : fixture.document->blocks) {
                expect(block.hasFlowContext);
                expect(block.appliedFlow.format == expectedContext.format);
                expect(block.appliedFlow.naturalLineSpacing == expectedContext.naturalLineSpacing);
            }

            // Rematerialization must use the same shared policy that produced
            // compact geometry, including fragments with no local image/style.
            EditorViewportMaterializer materializer(frame, fixture.resources, preparer,
                images, fixture.scroll, fixture.document);
            for (std::size_t index = 0; index < fixture.document->blocks.size(); ++index) {
                materializer.EnsurePrepared(index, false, false);
                auto const& block = fixture.document->blocks[index];
                expect(fatal(block.Visuals().layout != nullptr));
                DWRITE_LINE_SPACING_METHOD method{};
                FLOAT spacing = 0.0f, baseline = 0.0f;
                expect(SUCCEEDED(block.Visuals().layout->GetLineSpacing(&method, &spacing, &baseline)));
                expect((method == DWRITE_LINE_SPACING_METHOD_DEFAULT)
                    == expectedContext.naturalLineSpacing);
            }
            expect(!fixture.document->flowContextsChanged)
                << "the applied root policy contaminated an intrinsic fragment summary";
            expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - expectedHeight) < 0.25)
                << "viewport rematerialization changed exact shared-flow geometry";
        });
        return expectedContext;
    }

    float SourceDocumentTop(MeasurementFixture const& fixture, folia::TextPosition position) {
        auto index = fixture.document->FindOwnerBlock(position.container_id).value();
        auto const& block = fixture.document->blocks[index];
        expect(fatal(block.Visuals().layout != nullptr));
        auto display = folia::platform::editor::DisplayPositionForSource(
            block.Visuals().display.displayToSource, position);
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS hit{};
        expect(fatal(SUCCEEDED(block.Visuals().layout->HitTestTextPosition(
            static_cast<UINT32>(display), FALSE, &x, &y, &hit))));
        return fixture.document->geometry.At(index).top + block.Visuals().textTop + y;
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
                images, fixture.scroll, fixture.document);
            materializer.EnsurePrepared(fixture.document->FindOwnerBlock(position.container_id).value(),
                true, true);
            fixture.scroll.Set(SourceDocumentTop(fixture, position) + 3.0f,
                fixture.document->totalHeight);
        });
        PrepareFrame(fixture, frame);
    }

    suite document_layout_retention_tests = [] {
        "markdown image uses the available width and configured limit at its intrinsic ratio"_test = [] {
            MeasurementFixture fixture;
            std::wstring baseDirectory;
            EditorSvgPainter svg(fixture.resources, fixture.resourcesCache);
            EditorTextLayoutEngine engine(fixture.resources, fixture.sheet);
            // A 1200 x 900 PNG keeps this layout test independent of disk and network resources.
            std::vector<DisplayInlineText::ImageOverlay> images{{.source =
                "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAABLAAAAOEAQAAAACZFvVXAAAAm0"
                "lEQVR42u3BAQ0AAADCoPdPbQ43oAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA4NsAEvoAAfYq/V"
                "EAAAAASUVORK5CYII=", .block = true}};
            auto measure = [&](float width, int limit) {
                EditorInlineImageRenderer renderer(fixture.resources, fixture.resourcesCache, fixture.sheet,
                    fixture.normalizer, svg, baseDirectory, false, limit);
                auto layout = engine.Create(L"\uFFFC", fixture.resources.textFormat.Get(), width);
                auto draws = renderer.Resolve(layout.Get(), images, width, false);
                expect(fatal(draws.size() == 1_u));
                return std::pair(draws.front().width, draws.front().imageHeight);
            };
            expect(measure(480, 100) == std::pair(480.0f, 360.0f));
            expect(measure(960, 100) == std::pair(960.0f, 720.0f));
            expect(measure(960, 50) == std::pair(480.0f, 360.0f));
            expect(measure(1800, 100) == std::pair(1200.0f, 900.0f));
        };

        "document layout keeps small block geometry independent of visual storage"_test = [] {
            // A long list has few physical fragments; a document of small
            // independent blocks exposes the cost of each retained entry.
            std::string source;
            for (std::size_t index = 0; index < 2049; ++index) {
                source += "# Section " + std::to_string(index) + "\n\n";
                source += "A **formatted** paragraph with 中文, e\xCC\x81 and 😀.\n\n";
            }
            source += "| first | second |\n| --- | --- |\n| one | two |\n";
            EditorSession session;
            session.SetText(winrt::to_hstring(source));
            auto frame = session.RenderFrame();
            expect(fatal(frame.renderModel.virtualized));
            expect(fatal(frame.renderModel.blocks.size() > 4096_u));
            expect(sizeof(EditorPreparedDocument::Block) <= std::size_t{128})
                << "offscreen entries must not embed visual containers";
            std::cerr << "document-memory prepared_block_bytes=" << sizeof(EditorPreparedDocument::Block)
                << " visual_content_bytes=" << sizeof(EditorPreparedDocument::VisualContent) << '\n';

            MeasurementFixture fixture;
            fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
                EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
                cache.Reconcile(frame.selection, width, 1);
                cache.EnsureGeometry();
                auto const exactHeight = fixture.document->geometry.PreciseTotalHeight();
                for (auto const& block : fixture.document->blocks) {
                    expect(block.geometryMeasured);
                    expect(!block.HasVisualContent());
                    expect(block.Visuals().display.text.empty());
                    expect(!block.HasVisualContent()) << "a read materialized an evicted block";
                }
                EditorViewportMaterializer materializer(frame, fixture.resources, preparer,
                    images, fixture.scroll, fixture.document);
                for (auto index : {std::size_t{0}, fixture.document->blocks.size() / 2,
                                   fixture.document->blocks.size() - 1}) {
                    auto& block = fixture.document->blocks[index];
                    auto const height = block.height;
                    auto const structuralOwners = block.structuralOwners;
                    auto const sourceId = block.sourceId;
                    auto const context = block.appliedFlow;
                    materializer.EnsurePrepared(index, false, false);
                    expect(fatal(block.HasVisualContent()));
                    auto const* resident = &block.Visuals();
                    materializer.EnsurePrepared(index, false, false);
                    expect(&block.Visuals() == resident) << "a warm fragment lost its visual cache";
                    expect(block.height == height);
                    block.ReleaseVisualContent();
                    fixture.document->layoutBlocks.erase(index);
                    expect(!block.HasVisualContent());
                    expect(block.geometryMeasured);
                    expect(block.height == height);
                    expect(block.structuralOwners == structuralOwners);
                    expect(block.sourceId == sourceId);
                    expect(block.appliedFlow == context);
                    materializer.EnsurePrepared(index, false, false);
                    expect(block.HasVisualContent());
                    expect(block.height == height);
                    block.ReleaseVisualContent();
                    fixture.document->layoutBlocks.erase(index);
                }
                expect(fixture.document->geometry.PreciseTotalHeight() == exactHeight);
            });
        };

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
                        expect(measured.Visuals().layout != nullptr);
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

        "document layout shares natural line spacing across list fragments"_test = [] {
            for (auto const& special : std::vector<std::string>{
                "item with <span style=\"font-size:48px\">large text</span> after",
                "item with <img src=\"missing-badge.png\" width=\"48\" height=\"36\"> after",
            }) {
                auto source = ShortList(193, std::pair{std::size_t{80}, special});
                auto parsed = folia::parse_text(1, source);
                auto model = folia::build_render_model(parsed.document, parsed.outline,
                    parsed.symbols, folia::default_theme_profile());
                expect(fatal(model.fragments.size() == 4_u));
                std::wstring directory;
                winrt::Folia::detail::EditorRenderFrame frame{model, {}, {}, directory};
                MeasurementFixture fixture;
                auto expected = ExpectMonolithicHeight(fixture, frame, source);
                expect(expected.naturalLineSpacing);
                expect(fatal(fixture.document->flowGroups.size() == 1_u));
                auto const& summary = fixture.document->flowGroups.begin()->second.summary;
                expect(summary.naturalLineSpacingCount == 1_u)
                    << "a shared DEFAULT policy became an intrinsic requirement in every fragment";
                expect(!fixture.document->blocks.back().intrinsicFlow.naturalLineSpacing);
            }
        };

        "document layout shares the root text format across mixed quote headings"_test = [] {
            std::string source;
            for (std::size_t index = 0; index < 192; ++index)
                source += "> # heading " + std::to_string(index) + "\n>\n";
            source += "> final body paragraph\n";
            auto parsed = folia::parse_text(1, source);
            auto model = folia::build_render_model(parsed.document, parsed.outline,
                parsed.symbols, folia::default_theme_profile());
            expect(fatal(model.fragments.size() == 4_u));
            std::wstring directory;
            winrt::Folia::detail::EditorRenderFrame frame{model, {}, {}, directory};
            MeasurementFixture fixture(460);
            auto expected = ExpectMonolithicHeight(fixture, frame, source);
            expect(expected.format == EditorFlowTextFormat::Body);
        };

        "document layout restores shared spacing when its last natural requirement is edited"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(ShortList(193)));
            auto target = session.RenderModel().editable_order[80];
            auto plain = session.EditableSource(target);
            expect(fatal(plain.has_value()));
            MeasurementFixture fixture;
            auto verify = [&](bool natural) {
                auto frame = session.RenderFrame();
                // Compare identical unfocused presentations even though the
                // real command path places the caret inside the edited span.
                frame.selection = {};
                auto expected = ExpectMonolithicHeight(fixture, frame, session.TextUtf8());
                expect(expected.naturalLineSpacing == natural);
                expect(fatal(fixture.document->flowGroups.size() == 1_u));
                expect(fixture.document->flowGroups.begin()->second.summary.naturalLineSpacingCount
                    == static_cast<std::size_t>(natural));
            };
            auto replace = [&](std::u32string value) {
                auto current = session.EditableSource(target);
                expect(fatal(current.has_value()));
                session.SetSelection(
                    {target, 0, folia::TextAffinity::Downstream},
                    {target, current->size(), folia::TextAffinity::Upstream});
                expect(fatal(session.ExecuteCommand(folia::Command::InsertText(std::move(value)))));
            };
            verify(false);
            auto plainHeight = fixture.document->geometry.PreciseTotalHeight();
            replace(U"item with <span style=\"font-size:48px\">large text</span> after");
            verify(true);
            replace(*plain);
            verify(false);
            expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - plainHeight) < 0.25);
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
            verify(true);
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Redo})));
            verify(false);
            expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - plainHeight) < 0.25);
        };

        "document layout source highlighting retains only its latest lexical context"_test = [] {
            EditorSession session;
            session.SetText(L"```cpp\n/* start\ncontinued */ int value = 7;\n```\n");
            expect(fatal(session.EnterSourceMode()));
            auto frame = session.RenderFrame();
            std::vector<folia::RenderBlock> lines;
            for (auto const& block : frame.renderModel.blocks)
                if (block.source_code_context) lines.push_back(block);
            expect(fatal(lines.size() == 2_u));
            // Detached, immutable contexts let their weak references detect
            // retention independently of the source model's own lifetime.
            auto first = lines[0];
            auto second = lines[1];
            first.payload = std::make_shared<folia::RenderBlockSpecial>(first.special());
            std::weak_ptr<const std::u32string> retained;
            MeasurementFixture fixture;
            fixture.WithPreparer(frame, [&](auto& preparer, auto&, float) {
                auto commentAtStart = [](auto const& prepared) {
                    return std::ranges::any_of(prepared.Visuals().display.ranges, [](auto const& range) {
                        return range.start == 0 && range.length > 0
                            && range.syntax == SyntaxHighlightKind::Comment;
                    });
                };
                for (std::size_t index = 0; index < 24; ++index) {
                    auto previous = retained;
                    auto context = std::make_shared<const std::u32string>(
                        *lines[0].source_code_context);
                    first.source_code_context = context;
                    second.source_code_context = context;
                    auto opened = preparer.Prepare(first, false, false);
                    expect(previous.expired()) << "a previous code context survived eviction";
                    auto continued = preparer.Prepare(second, false, false);
                    expect(commentAtStart(opened));
                    expect(commentAtStart(continued))
                        << "a multiline comment lost its shared lexical context";
                    if (index == 0) {
                        first.ensure_special().language = "text";
                        expect(!commentAtStart(preparer.Prepare(first, false, false)))
                            << "language changes reused stale syntax ranges";
                        first.ensure_special().language = "cpp";
                        expect(commentAtStart(preparer.Prepare(first, false, false)));
                    }
                    retained = context;
                    first.source_code_context.reset();
                    second.source_code_context.reset();
                    context.reset();
                    expect(!retained.expired()) << "the cached context key was not owned";
                }
            });
            expect(retained.expired()) << "the pass retained syntax after its lifetime ended";
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
                    expect(!block.HasVisualContent());
                    expect(block.Visuals().layout == nullptr);
                    expect(block.Visuals().display.text.empty());
                    expect(!block.Visuals().paintPlan);
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
                    expect(std::ranges::count_if(fixture.document->blocks,
                        [](auto const& block) { return block.HasVisualContent(); }) < 16);
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
                expect(!block.HasVisualContent());
                expect(block.Visuals().layout == nullptr);
                expect(!block.Visuals().paintPlan);
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
            expect(fixture.document->FindOwnerBlock(owner).value() == 0_u);
            auto beforeOffset = fixture.scroll.Offset();
            auto screenY = SourceDocumentTop(fixture, anchor) - beforeOffset;
            expect(std::abs(screenY + 3.0f) < 0.25f);
            for (int retry = 0; retry < 3; ++retry)
                PrepareFrame(fixture, frame);
            expect(fixture.scroll.Offset() == beforeOffset)
                << "an unchanged layout shifted the viewport";

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

        "document layout tail splits reuse the prepared owner index"_test = [] {
            EditorSession session;
            session.SetText(winrt::to_hstring(RepeatedList(8192, "- ")));
            auto first = session.RenderModel().editable_order.front();
            auto tail = session.RenderModel().editable_order.back();
            auto tailSource = session.EditableSource(tail);
            expect(fatal(tailSource.has_value()));
            session.SetSelection(folia::TextSelection::caret(
                {tail, tailSource->size(), folia::TextAffinity::Downstream}));
            MeasurementFixture fixture;
            ScrollToSource(fixture, session.RenderFrame(),
                {first, 0, folia::TextAffinity::Downstream});
            auto originalFragments = fixture.document->blocks.size();
            auto initialOwner = fixture.document->FindOwnerBlock(first);
            auto verifyLocalRepair = [&](std::size_t before) {
                expect(fixture.document->ownerIndexEntriesVisited - before < 2048_u)
                    << "a tail edit revisited the full document owner index";
                expect(fixture.document->FindOwnerBlock(first) == initialOwner)
                    << "a retained fragment lost its owner association";
                for (auto owner : session.RenderModel().editable_order)
                    expect(fixture.document->FindOwnerBlock(owner).value()
                        == session.RenderModel().fragment_for_owner(owner).value());
            };
            auto visits = fixture.document->ownerIndexEntriesVisited;
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::InsertNewline})));
            auto inserted = session.Selection().active.container_id;
            PrepareFrame(fixture, session.RenderFrame());
            expect(fixture.document->blocks.size() > originalFragments);
            verifyLocalRepair(visits);

            visits = fixture.document->ownerIndexEntriesVisited;
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
            PrepareFrame(fixture, session.RenderFrame());
            verifyLocalRepair(visits);
            expect(!fixture.document->FindOwnerBlock(inserted).has_value());

            visits = fixture.document->ownerIndexEntriesVisited;
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Redo})));
            PrepareFrame(fixture, session.RenderFrame());
            verifyLocalRepair(visits);
        };

        "document layout owner indexing covers thematic breaks tables and source rows"_test = [] {
            EditorSession session;
            session.SetText(L"# heading\n\n---\n\n| A | B |\n| --- | --- |\n| C | |\n\n> - body\n");
            MeasurementFixture fixture;
            for (bool sourceMode : {false, true, false}) {
                if (sourceMode != session.IsSourceMode()) expect(fatal(session.ToggleSourceMode()));
                auto frame = session.RenderFrame();
                fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
                    EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
                    cache.Reconcile(frame.selection, width, 1);
                    cache.EnsureGeometry();
                });
                for (auto owner : frame.renderModel.editable_order) {
                    auto found = fixture.document->FindOwnerBlock(owner);
                    expect(fatal(found.has_value()));
                    expect(*found < fixture.document->blocks.size());
                    auto expected = sourceMode ? frame.renderModel.editable_index.at(owner.v)
                        : frame.renderModel.fragment_for_owner(owner).value();
                    expect(*found == expected);
                }
                if (!sourceMode) {
                    auto thematic = std::ranges::find(frame.renderModel.blocks,
                        folia::RenderBlockKind::ThematicBreak, &folia::RenderBlock::kind);
                    expect(fatal(thematic != frame.renderModel.blocks.end()));
                    expect(fixture.document->FindOwnerBlock(thematic->id).has_value());
                }
            }
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
                expect(!block.HasVisualContent());
                expect(block.Visuals().layout == nullptr);
                expect(block.Visuals().display.text.empty());
                expect(!block.Visuals().paintPlan);
            }
            std::cerr << "document-scalability-native fragments=" << fragments
                << " retained_layouts=" << prepared->document->layoutBlocks.size()
                << " exact_height=" << prepared->document->geometry.PreciseTotalHeight() << '\n';
        };
        "document layout raw previews remain after the semantic flow with a remote source fragment"_test = [] {
            for (bool diagram : {false, true}) {
                std::string source = diagram
                    ? "- before\n\n  ```mermaid\n  flowchart TD\n  A[Start] --> B[Done]\n  ```\n\n"
                    : "- before\n\n  $$\n  x+y\n  $$\n\n";
                source += RepeatedList(257, "- ");
                EditorSession session;
                session.SetText(winrt::to_hstring(source));
                auto initial = session.RenderFrame();
                expect(fatal(static_cast<bool>(initial.materializeBlocks)));
                initial.materializeBlocks(0, 1);
                auto findRaw = [&](auto& self, folia::RenderBlock const& block)
                    -> folia::RenderBlock const* {
                    if (block.kind == (diagram ? folia::RenderBlockKind::Code
                                              : folia::RenderBlockKind::Math)) return &block;
                    for (auto const& child : block.child_blocks)
                        if (auto result = self(self, child)) return result;
                    return nullptr;
                };
                auto raw = findRaw(findRaw, initial.renderModel.blocks.front());
                expect(fatal(raw != nullptr));
                auto diagramSource = diagram ? folia::cps_to_utf8(raw->special().code_text) : std::string{};
                session.SetSelection(folia::TextSelection::caret(
                    {raw->id, 0, folia::TextAffinity::Downstream}));
                auto frame = session.RenderFrame();
                MeasurementFixture fixture;

                // Use the original single semantic flow as the native height
                // reference. Its parser-local ids need their own caret owner.
                auto parsed = folia::parse_text(1, source);
                expect(fatal(parsed.document.root.children.size() == 1_u));
                auto theme = folia::default_theme_profile();
                folia::Builder builder(theme);
                folia::configure_render_dependencies(builder, parsed.symbols);
                auto whole = builder.build_block(parsed.document.root.children.front());
                auto referenceRaw = findRaw(findRaw, whole);
                expect(fatal(referenceRaw != nullptr));
                auto referenceFrame = frame;
                referenceFrame.selection = folia::TextSelection::caret(
                    {referenceRaw->id, 0, folia::TextAffinity::Downstream});
                fixture.WithPreparer(referenceFrame, [&](auto& preparer, auto&, float) {
                    (void)preparer.Prepare(whole, true, true);
                });
                fixture.math.WaitForIdle();
                if (diagram) {
                    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                    std::optional<folia::platform::MermaidSvg> rendered;
                    do {
                        rendered = fixture.mermaid.GetOrQueue(diagramSource, false);
                        if (rendered) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    } while (std::chrono::steady_clock::now() < deadline);
                    expect(fatal(rendered.has_value()));
                    expect(fatal(static_cast<bool>(*rendered)));
                }
                float referenceHeight = 0, referenceTextEnd = 0, referencePreviewHeight = 0;
                fixture.WithPreparer(referenceFrame, [&](auto& preparer, auto&, float) {
                    auto expected = preparer.Prepare(whole, true, true);
                    referenceHeight = 2 * fixture.sheet.verticalPadding + whole.block_style.margin_top
                        + expected.height + whole.block_style.margin_bottom + fixture.sheet.blockGap;
                    referenceTextEnd = fixture.sheet.verticalPadding + whole.block_style.margin_top
                        + expected.Visuals().textTop + expected.Visuals().textHeight;
                    referencePreviewHeight = expected.previewHeight;
                    expect(fatal(referencePreviewHeight > 0));
                });

                fixture.WithPreparer(frame, [&](auto& preparer, auto& images, float width) {
                    EditorPreparedDocumentCache cache(frame, fixture.sheet, images, preparer, fixture.document);
                    cache.Reconcile(frame.selection, width, 1);
                    cache.EnsureGeometry();
                    expect(fatal(fixture.document->trailingPreview.has_value()));
                    auto sourceIndex = fixture.document->trailingPreview->sourceIndex;
                    auto targetIndex = fixture.document->trailingPreview->targetIndex;
                    expect(sourceIndex < targetIndex);
                    expect(targetIndex + 1 == fixture.document->blocks.size());
                    expect(!fixture.document->trailingPreview->resident);
                    expect(fixture.document->trailingPreview->mathPreviews.empty());
                    expect(fixture.document->trailingPreview->mermaidPreviews.empty());
                    expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - referenceHeight) < 0.25);

                    EditorViewportMaterializer materializer(frame, fixture.resources, preparer,
                        images, fixture.scroll, fixture.document);
                    materializer.EnsurePrepared(targetIndex, false, false);
                    auto targetTop = fixture.document->geometry.At(targetIndex).top;
                    auto const& tail = fixture.document->blocks[targetIndex];
                    expect(std::abs(targetTop + tail.Visuals().textTop + tail.Visuals().textHeight - referenceTextEnd) < 0.25)
                        << "the preview moved into an artificial fragment boundary";
                    (void)materializer.MaterializeTrailingPreview(targetTop, false, 1);
                    cache.ReconcileTrailingPreview();
                    auto const& preview = *fixture.document->trailingPreview;
                    expect(preview.resident);
                    expect(!preview.pending);
                    expect(std::abs(preview.height - referencePreviewHeight) < 0.25);
                    expect(diagram ? !preview.mermaidPreviews.empty() : !preview.mathPreviews.empty());
                    expect(fixture.document->blocks[sourceIndex].Visuals().layout == nullptr)
                        << "a remote preview retained its source fragment layout";
                    expect(!frame.renderModel.blocks[sourceIndex].materialized);
                    expect(std::abs(fixture.document->geometry.PreciseTotalHeight() - referenceHeight) < 0.25);
                    expect(!materializer.MaterializeTrailingPreview(targetTop, false, 1));

                    // A pending request waits for an embedded completion epoch,
                    // rather than scheduling another render on every frame.
                    fixture.document->trailingPreview->pending = true;
                    fixture.document->trailingPreview->embeddedGeneration = 1;
                    expect(!materializer.MaterializeTrailingPreview(targetTop, false, 1));
                    fixture.document->trailingPreview->pending = false;
                    (void)materializer.MaterializeTrailingPreview(0, false, 1);
                    expect(!fixture.document->trailingPreview->resident);
                    expect(fixture.document->trailingPreview->mathPreviews.empty());
                    expect(fixture.document->trailingPreview->mermaidPreviews.empty());
                    expect(fixture.document->trailingPreview->height > 0);
                });
            }
        };
    };
}
