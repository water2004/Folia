#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorDocumentRenderPass.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;

    struct NativeInlineFrame {
        EditorStyleSheet sheet;
        EditorRenderResources resources;
        EditorRenderCache cache;
        MathJaxRenderer math;
        MermaidRenderer mermaid;
        SvgNormalizer normalizer;
        TreeSitterHighlighter syntax;
        EditorInteractionMap interaction;
        EditorCaretAnimation caretAnimation;
        std::vector<D2D1_RECT_F> mathRects;
        std::vector<D2D1_COLOR_F> mathColors;
        std::size_t mathSourceCount = 0;
        std::vector<DisplayInlineText::MathBracket> mathBrackets;
        std::vector<InlineStyleRange> mathSyntax;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
        float width = EditorDocumentContentWidth(600, sheet.horizontalPadding);

        explicit NativeInlineFrame(folia::Theme theme = folia::Theme::Light)
            : sheet(CreateEditorStyleSheet(folia::default_theme_profile(theme))) {
            resources.InitializeOffscreen(sheet, 600, 400);
            resources.EnsureFrameResources(sheet);
            auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            winrt::check_hresult(resources.d2dContext->CreateBitmap({600, 400}, nullptr, 0, properties, target.GetAddressOf()));
        }

        std::vector<std::uint32_t> Paint(EditorSession& session,
            std::span<const winrt::Folia::detail::EditorSearchHighlight> highlights = {}, bool paintSvg = true,
            bool showSelection = false, bool paintMatchingBrackets = true) {
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            frame.searchHighlights = highlights;
            auto selection = showSelection ? frame.selection : folia::TextSelection{};
            interaction = {}; mathRects.clear(); mathColors.clear();
            mathSourceCount = 0; mathBrackets.clear(); mathSyntax.clear();
            std::vector<D2D1_RECT_F> nonInteractive;
            EditorTextLayoutEngine engine(resources, sheet);
            EditorSvgPainter svg(resources, cache);
            EditorInlineImageRenderer images(resources, cache, sheet, normalizer, svg, frame.baseDirectory);
            EditorDocumentPainter painter(resources, sheet, interaction, syntax, {}, false, 20 + width,
                selection, highlights, frame.renderModel.editable_index);
            EditorDocumentBlockPreparer preparer(frame, resources, cache, sheet, engine, images,
                painter, math, mermaid, normalizer, syntax, selection.active, width, true, true, false, 0, 0);
            EditorPreparedDocument document;
            for (auto const& block : frame.renderModel.blocks) document.blocks.push_back(preparer.Prepare(block, true, true));
            auto collectSyntax = [&](auto const& display) {
                mathSourceCount += display.mathSources.size();
                mathBrackets.insert(mathBrackets.end(), display.mathBrackets.begin(), display.mathBrackets.end());
                if (!display.mathSources.empty()) for (auto const& range : display.ranges)
                    if (range.syntax != SyntaxHighlightKind::None) mathSyntax.push_back(range);
            };
            for (auto const& block : document.blocks) {
                collectSyntax(block.display);
                if (block.table) for (auto const& display : block.table->displays) collectSyntax(display);
            }
            math.WaitForIdle();
            std::vector<EditorBlockGeometryIndex::Entry> geometry;
            for (std::size_t i = 0; i < document.blocks.size(); ++i) {
                document.blocks[i] = preparer.Prepare(frame.renderModel.blocks[i], true, true);
                geometry.push_back({0, document.blocks[i].height, 8});
            }
            document.geometry.Reset(std::move(geometry), 20);
            if (!paintMatchingBrackets) for (auto& block : document.blocks) {
                block.display.mathBrackets.clear();
                if (block.table) for (auto& display : block.table->displays) display.mathBrackets.clear();
            }
            std::vector<std::uint64_t> visibleIds;
            for (auto const& block : document.blocks) {
                for (auto const& overlay : block.display.mathOverlays) visibleIds.push_back(overlay.fragment.renderId);
                if (block.table) for (auto const& display : block.table->displays)
                    for (auto const& overlay : display.mathOverlays) visibleIds.push_back(overlay.fragment.renderId);
            }
            math.SetVisibleSvgIds(visibleIds);
            math.WaitForIdle();
            EditorDocumentRenderPass pass(resources, sheet, interaction, caretAnimation, nonInteractive, images, painter,
                [&](auto const& fragment, auto origin, auto color, float scale) {
                    mathRects.push_back({origin.x, origin.y, origin.x + fragment.width * scale, origin.y + fragment.height * scale});
                    mathColors.push_back(color);
                    if (!paintSvg) return true;
                    auto payload = fragment.svg ? fragment.svg : math.GetSvg(fragment.renderId);
                    return payload && svg.Draw(fragment.renderId, *payload, fragment.width * scale, fragment.height * scale,
                        origin, fragment.width, fragment.height);
                }, [&](auto, auto) { expect(false) << "native formula unexpectedly fell back"; },
                [](auto const&, float, float, auto) { return true; });
            resources.d2dContext->SetTarget(target.Get());
            resources.d2dContext->BeginDraw();
            resources.d2dContext->Clear(sheet.canvasColor);
            pass.Paint(frame, document, selection, selection.active, 20, 20 + width, width, 0, 0, 0,
                0, document.blocks.size(), false, {}, {}, {});
            winrt::check_hresult(resources.d2dContext->EndDraw());
            resources.d2dContext->SetTarget(nullptr);
            auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> readback;
            winrt::check_hresult(resources.d2dContext->CreateBitmap({600, 400}, nullptr, 0, properties, readback.GetAddressOf()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
            D2D1_MAPPED_RECT pixels{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &pixels));
            std::vector<std::uint32_t> result(600 * 400);
            for (std::size_t y = 0; y < 400; ++y) std::memcpy(result.data() + y * 600, pixels.bits + y * pixels.pitch, 600 * 4);
            readback->Unmap();
            return result;
        }

        void Save(std::filesystem::path const& path, std::vector<std::uint32_t> const& pixels) {
            std::filesystem::create_directories(path.parent_path());
            ::Microsoft::WRL::ComPtr<IWICStream> stream;
            winrt::check_hresult(resources.wicFactory->CreateStream(&stream));
            winrt::check_hresult(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
            ::Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
            winrt::check_hresult(resources.wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
            winrt::check_hresult(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
            ::Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
            winrt::check_hresult(encoder->CreateNewFrame(&frame, nullptr));
            winrt::check_hresult(frame->Initialize(nullptr));
            winrt::check_hresult(frame->SetSize(600, 400));
            auto format = GUID_WICPixelFormat32bppBGRA;
            winrt::check_hresult(frame->SetPixelFormat(&format));
            winrt::check_hresult(frame->WritePixels(400, 600 * 4, 600 * 400 * 4,
                reinterpret_cast<BYTE*>(const_cast<std::uint32_t*>(pixels.data()))));
            winrt::check_hresult(frame->Commit());
            winrt::check_hresult(encoder->Commit());
        }
    };

    suite inline_presentation_paths = [] {
        "math editing highlights CST source and matching brackets across content containers"_test = [] {
            int caseIndex = 0;
            for (auto source : {L"😀 $\\frac{α}{中}$", L"# $\\frac{a}{b}$", L"> $\\frac{a}{b}$",
                    L"- $\\frac{a}{b}$", L"- [ ] $\\frac{a}{b}$", L"> [!NOTE]\n> $\\frac{a}{b}$",
                    L"| math |\n| --- |\n| $\\frac{a}{b}$ |", L"ref[^n]\n\n[^n]: $\\frac{a}{b}$",
                    L"$$\n\\frac{a}{b}\n$$", L"$\\frac{a$"}) {
                EditorSession session;
                session.SetText(source);
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                std::optional<folia::TextPosition> caret;
                auto inspect = [&](auto const& items) {
                    for (auto const& item : items) if (item.kind == folia::InlineRenderItem::Kind::Math) {
                        if (auto content = item.special().semantic().content_range)
                            caret = folia::TextPosition{item.source_span.container_id, content->start + 6, folia::TextAffinity::Downstream};
                    }
                };
                auto visit = [&](auto&& self, folia::RenderBlock const& block) -> void {
                    inspect(block.inline_items);
                    for (auto const& cell : block.special().table_cells) inspect(cell);
                    if (block.kind == folia::RenderBlockKind::Math) {
                        auto const& positions = block.special().content_to_source;
                        caret = folia::TextPosition{block.id, positions.empty() ? block.content_span.source_range.start + 6 : positions.at(6),
                            folia::TextAffinity::Downstream};
                    }
                    for (auto const& child : block.child_blocks) self(self, child);
                };
                for (auto const& block : frame.renderModel.blocks) visit(visit, block);
                expect(caret.has_value()) << "CST math source in context " << caseIndex;
                if (!caret) { ++caseIndex; continue; }
                session.SetSelection(folia::TextSelection::caret(*caret));
                auto before = session.TextUtf8();
                NativeInlineFrame native;
                auto pixels = native.Paint(session, {}, true, true);
                expect(native.mathSourceCount == 1_u) << "math source in context " << caseIndex;
                expect(std::ranges::any_of(native.mathSyntax, [](auto const& range) { return range.syntax == SyntaxHighlightKind::Function; }));
                expect(!native.mathBrackets.empty()) << "brackets in context " << caseIndex;
                bool incomplete = caseIndex == 9;
                expect(native.mathBrackets.size() == (incomplete ? std::size_t{1} : std::size_t{2}));
                for (auto const& bracket : native.mathBrackets) expect(bracket.matched == !incomplete);
                expect(session.TextUtf8() == before);
                if (caseIndex == 0) native.Save(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                    / "build/tests/math-editor-source.png", pixels);
                ++caseIndex;
            }
        };

        "math editing never highlights escaped dollar text as a formula"_test = [] {
            EditorSession session;
            session.SetText(L"literal \\$ and \\frac{a}{b}");
            NativeInlineFrame native;
            native.Paint(session);
            expect(native.mathSourceCount == 0_u);
            expect(native.mathBrackets.empty());
        };

        "math editing highlighting follows source transactions and exact undo redo"_test = [] {
            EditorSession session;
            session.SetText(L"before $\\frac{a}{b}$ after");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            auto const& items = frame.renderModel.blocks.front().inline_items;
            auto math = std::ranges::find(items, folia::InlineRenderItem::Kind::Math, &folia::InlineRenderItem::kind);
            expect(math != items.end());
            if (math == items.end()) return;
            auto content = math->special().semantic().content_range;
            expect(content.has_value());
            if (!content) return;
            session.SetSelection(folia::TextSelection::caret({math->source_span.container_id, content->start + 6, folia::TextAffinity::Downstream}));
            auto original = session.TextUtf8();
            auto selection = session.Selection();
            NativeInlineFrame native;
            auto verify = [&] {
                native.Paint(session, {}, true, true);
                auto counters = folia::read_core_operation_counters();
                expect(counters.full_document_parses == 0_u);
                expect(counters.full_document_serializations == 0_u);
                expect(native.mathSourceCount == 1_u);
                expect(!native.mathSyntax.empty());
            };
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand(folia::Command::InsertText(U"1+\\alpha")));
            verify();
            auto edited = session.TextUtf8();
            auto editedSelection = session.Selection();
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            verify();
            expect(session.TextUtf8() == original);
            expect(session.Selection() == selection);
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            verify();
            expect(session.TextUtf8() == edited);
            expect(session.Selection() == editedSelection);
        };

        "math bracket boxes stay subtle and use their own theme colors"_test = [] {
            for (auto theme : {folia::Theme::Light, folia::Theme::Dark}) {
                EditorSession session;
                session.SetText(L"$\\frac{a}{b}$\n");
                auto frame = session.RenderFrame();
                auto owner = frame.renderModel.blocks.front().id;
                session.SetSelection(folia::TextSelection::caret({owner, 6, folia::TextAffinity::Downstream}));
                NativeInlineFrame native(theme);
                auto plain = native.Paint(session, {}, true, true, false);
                auto marked = native.Paint(session, {}, true, true);
                expect(native.mathBrackets.size() == 2_u);
                std::size_t changed = 0;
                int maximumDelta = 0;
                for (std::size_t i = 0; i < plain.size(); ++i) {
                    changed += plain[i] != marked[i];
                    for (auto shift : {0, 8, 16}) maximumDelta = (std::max)(maximumDelta,
                        std::abs(static_cast<int>((plain[i] >> shift) & 255) - static_cast<int>((marked[i] >> shift) & 255)));
                }
                expect(changed > 5_u);
                expect(maximumDelta <= (theme == folia::Theme::Light ? 75 : 130))
                    << "matching brackets must not borrow the selection fill or bright accent border";
                auto directory = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "build/tests";
                native.Save(directory / (theme == folia::Theme::Light ? "math-brackets-light.png" : "math-brackets-dark.png"), marked);
            }
        };

        "code background contains terminal empty rows through enter undo and redo"_test = [] {
            int caseIndex = 0;
            for (auto source : {L"```cpp\n1111111\n111111\n```\n", L"```cpp\n\n```\n",
                L"> ```cpp\n> 1111111\n> 111111\n> ```\n", L"- item\n\n  ```cpp\n  1111111\n  111111\n  ```\n"}) {
                EditorSession session;
                session.SetText(source);
                NativeInlineFrame native;
                native.Paint(session);
                auto frame = session.RenderFrame();
                folia::RenderBlock const* code = nullptr;
                auto find = [&](auto&& self, folia::RenderBlock const& block) -> void {
                    if (block.kind == folia::RenderBlockKind::Code) code = &block;
                    for (auto const& child : block.child_blocks) self(self, child);
                };
                for (auto const& block : frame.renderModel.blocks) find(find, block);
                expect(code != nullptr);
                if (!code) continue;
                auto owner = code->id;
                session.SetSelection(folia::TextSelection::caret({owner, code->content_span.source_range.end, folia::TextAffinity::Downstream}));
                auto containsCaret = [&] {
                    auto pixels = native.Paint(session, {}, true, true);
                    auto caret = native.interaction.CaretBounds(session.Selection().active, native.sheet.body.lineHeight);
                    auto raw = native.interaction.RawBlockFor(owner);
                    expect(caret.has_value() && raw.has_value());
                    auto stillInCode = session.Selection().active.container_id == owner;
                    if (caret && raw) {
                        if (stillInCode) expect(caret->top >= raw->rect.top && caret->bottom <= raw->rect.bottom + .5f)
                            << "terminal editable row fell outside the code background";
                        else expect(caret->top >= raw->rect.bottom - .5f)
                            << "exiting an empty code row must place the paragraph after the code block";
                    }
                    if (caret && raw && stillInCode && caret->bottom > raw->rect.bottom + .5f) {
                        DWRITE_TEXT_METRICS metrics{};
                        if (!native.interaction.blocks.empty() && native.interaction.blocks.front().layout)
                            native.interaction.blocks.front().layout->GetMetrics(&metrics);
                        std::fprintf(stderr, "code row case=%d sourceOffset=%zu caret=%.1f..%.1f raw=%.1f..%.1f layoutHeight=%.1f\n",
                            caseIndex, session.Selection().active.source_offset, caret->top, caret->bottom,
                            raw->rect.top, raw->rect.bottom, metrics.height);
                    }
                    return pixels;
                };
                containsCaret();
                for (int row = 0; row < 3; ++row) {
                    auto before = session.Text();
                    auto beforeSelection = session.Selection();
                    folia::Command enter; enter.kind = folia::CommandKind::InsertNewline;
                    expect(session.ExecuteCommand(enter));
                    auto after = session.Text();
                    auto afterSelection = session.Selection();
                    auto pixels = containsCaret();
                    if (row == 0) native.Save(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                        / "build/tests/inline-presentation/code-newline.png", pixels);
                    folia::Command undo; undo.kind = folia::CommandKind::Undo;
                    expect(session.ExecuteCommand(undo));
                    expect(session.Text() == before);
                    expect(session.Selection().active.container_id == beforeSelection.active.container_id
                        && session.Selection().active.source_offset == beforeSelection.active.source_offset);
                    containsCaret();
                    folia::Command redo; redo.kind = folia::CommandKind::Redo;
                    expect(session.ExecuteCommand(redo));
                    expect(session.Text() == after);
                    expect(session.Selection().active.container_id == afterSelection.active.container_id
                        && session.Selection().active.source_offset == afterSelection.active.source_offset);
                    containsCaret();
                }
                ++caseIndex;
            }
        };

        "native code language header keeps the same font and pixels while editing"_test = [] {
            for (auto theme : {folia::Theme::Light, folia::Theme::Dark}) {
                EditorSession session;
                session.SetText(L"```text\nminecraft_session.has.joined.server\ninvite.read.any\ninvite.create.any\ninvite.delete.any\n```\n");
                NativeInlineFrame native(theme);
                auto reading = native.Paint(session);
                expect(!native.interaction.rawBlocks.empty());
                if (native.interaction.rawBlocks.empty()) continue;
                auto raw = native.interaction.rawBlocks.front();
                session.SetSelection(folia::TextSelection::caret(raw.contentStart));
                auto editing = native.Paint(session, {}, true, true);
                std::size_t changed = 0;
                for (int y = static_cast<int>(std::ceil(raw.headerRect.top)); y < raw.headerRect.bottom; ++y)
                for (int x = static_cast<int>(std::ceil(raw.headerRect.left)); x < raw.headerRect.right; ++x)
                    changed += reading[y * 600 + x] != editing[y * 600 + x];
                expect(changed == 0_u) << "focusing code replaced its native language label";
                auto directory = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "build/tests/inline-presentation";
                native.Save(directory / (theme == folia::Theme::Light ? "code-header-light.png" : "code-header-dark.png"), editing);
            }
        };

        "production flow and table paint search hits without changing the document"_test = [] {
            for (auto table : {false, true}) {
                EditorSession session;
                session.SetText(table ? L"| head |\n| --- |\n| needle |\n" : L"needle\n");
                auto original = session.Text();
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto owner = table ? frame.renderModel.blocks.front().special().table_cell_spans.back().container_id
                    : frame.renderModel.blocks.front().id;
                NativeInlineFrame native;
                auto plain = native.Paint(session);
                std::array<winrt::Folia::detail::EditorSearchHighlight, 1> hits{{{{owner, {0, 6}}, true}}};
                auto marked = native.Paint(session, hits);
                auto changed = std::inner_product(plain.begin(), plain.end(), marked.begin(), std::size_t{0},
                    std::plus<>{}, [](auto a, auto b) -> std::size_t { return a != b; });
                expect(changed > 10_u) << (table ? "table search highlight missing" : "flow search highlight missing");
                expect(session.Text() == original);
            }
        };

        "production flow and table math have one centered deletion line"_test = [] {
            for (auto table : {false, true}) {
              for (auto tex : {L"x", L"x+1", LR"(\frac{g_j}{1+x})"}) {
                EditorSession session;
                auto body = std::wstring{L"~~$"} + tex + L"$~~";
                session.SetText(winrt::hstring{table ? L"| head |\n| --- |\n| " + body + L" |\n" : body + L"\n"});
                NativeInlineFrame native;
                auto pixels = native.Paint(session, {}, false);
                expect(!native.mathRects.empty());
                if (native.mathRects.empty()) continue;
                auto rect = native.mathRects.front();
                for (auto const& next : native.mathRects) {
                    rect.left = (std::min)(rect.left, next.left); rect.right = (std::max)(rect.right, next.right);
                    rect.top = (std::min)(rect.top, next.top); rect.bottom = (std::max)(rect.bottom, next.bottom);
                }
                auto expectedY = rect.top + (rect.bottom - rect.top) * .52f;
                std::vector<int> rows;
                for (int y = static_cast<int>(rect.top); y < std::ceil(rect.bottom); ++y) {
                    std::size_t ink = 0;
                    for (int x = static_cast<int>(rect.left + 2); x < rect.right - 2; ++x) {
                        auto pixel = pixels[y * 600 + x];
                        if ((pixel & 0xff) < 150 && ((pixel >> 8) & 0xff) < 150) ++ink;
                    }
                    if (ink > 3) rows.push_back(y);
                }
                expect(!rows.empty());
                if (!rows.empty()) {
                    expect(rows.size() <= 3_u) << "more than one deletion band";
                    expect(std::abs((rows.front() + rows.back() + 1) * .5f - expectedY) < 1.f)
                        << (table ? "table" : "flow") << "deletion line does not follow the SVG bounds";
                }
              }
            }
        };

        "batch and visible math inherit the semantic inline foreground"_test = [] {
            for (auto table : {false, true}) {
                EditorSession session;
                session.SetText(table ? L"| head |\n| --- |\n| <span style=\"color:#cc3355\">text $x+1$</span> |\n"
                    : L"<span style=\"color:#cc3355\">text $x+1$</span>\n");
                auto frame = session.RenderFrame();
                auto theme = folia::default_theme_profile(folia::Theme::Light);
                EditorMathPreparationContext context{theme, 1, 600, 400, true};
                auto prepared = EditorSurfaceRenderer::PrepareMath(frame, context, {}, {});
                auto sheet = CreateEditorStyleSheet(theme);
                auto width = EditorDocumentContentWidth(600, sheet.horizontalPadding);
                if (table) width = EditorTableCellContentWidth(width, 1);
                auto colored = prepared->math.Lookup("x+1", false, sheet.body.size, width, "#CC3355");
                expect(static_cast<bool>(colored)) << "batch cache did not use the inherited foreground";
                NativeInlineFrame native;
                native.math = std::move(prepared->math);
                auto rendered = native.math.Snapshot().nativeRenders;
                native.Paint(session);
                expect(native.math.Snapshot().nativeRenders == rendered) << "visible math must reuse the batch request key";
                expect(!native.mathColors.empty());
                if (!native.mathColors.empty()) {
                    auto color = native.mathColors.front();
                    expect(std::abs(color.r - .8f) < .001f && std::abs(color.g - .2f) < .001f
                        && std::abs(color.b - 1.f / 3) < .001f) << "visible paint lost the inherited foreground";
                }
            }
        };
    };
}
