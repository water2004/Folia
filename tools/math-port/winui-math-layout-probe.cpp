#include "pch.h"

import std;
import folia.core.command;
import folia.core.render_model;

#include <Psapi.h>

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorSvgPainter.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "media/SvgNormalizer.h"
#include "media/NativeMathSession.h"

#pragma comment(lib, "Psapi.lib")

namespace
{
    using namespace winrt::Folia;

    void Require(bool condition, std::string_view message)
    {
        if (!condition) throw std::runtime_error(std::string(message));
    }

    void WaitForIdleBounded(MathJaxRenderer& renderer,
        std::chrono::seconds timeout = std::chrono::seconds(50))
    {
        std::stop_source waitStop;
        std::mutex mutex;
        std::condition_variable_any changed;
        std::jthread watchdog([&](std::stop_token stop) {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, stop, timeout, [] { return false; }))
                waitStop.request_stop();
        });
        try { renderer.WaitForIdle(waitStop.get_token()); }
        catch (winrt::hresult_canceled const&) {
            watchdog.request_stop();
            throw std::runtime_error("timed out waiting for native math preparation");
        }
        watchdog.request_stop();
    }

    struct BlockStamp
    {
        std::uint64_t id = 0;
        std::uint64_t sourceKey = 0;
        std::uint64_t presentationKey = 0;
        folia::RenderBlockKind kind{};

        friend bool operator==(BlockStamp const&, BlockStamp const&) = default;
    };

    std::vector<BlockStamp> Stamp(detail::EditorRenderFrame const& frame)
    {
        std::vector<BlockStamp> result;
        result.reserve(frame.renderModel.blocks.size());
        for (auto const& block : frame.renderModel.blocks)
            result.push_back({ block.id.v, block.source_key, block.presentation_key, block.kind });
        return result;
    }

    struct ProcessMemory
    {
        SIZE_T privateBytes = 0;
        SIZE_T peakWorkingSet = 0;
    };

    ProcessMemory ReadProcessMemory()
    {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                "GetProcessMemoryInfo");
        return { counters.PrivateUsage, counters.PeakWorkingSetSize };
    }

    void ReportStage(std::string_view stage, std::chrono::steady_clock::duration elapsed)
    {
        const auto memory = ReadProcessMemory();
        std::cout << "stage=" << stage
                  << " elapsed-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                  << " private-bytes=" << memory.privateBytes
                  << " peak-working-bytes=" << memory.peakWorkingSet << '\n';
    }

    float DocumentWidth(EditorMathPreparationContext const& context)
    {
        return EditorDocumentContentWidth(context.surfaceWidth,
            context.theme.layout.document_horizontal_padding);
    }

    std::string TextColor(folia::ThemeProfile const& theme)
    {
        auto const& color = theme.colors.fg;
        char buffer[8]{};
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
            static_cast<unsigned>(color.r), static_cast<unsigned>(color.g), static_cast<unsigned>(color.b));
        return buffer;
    }

    std::size_t MathOnlyBlockIndex(detail::EditorRenderFrame const& frame)
    {
        for (std::size_t blockIndex = 0; blockIndex < frame.renderModel.blocks.size(); ++blockIndex)
        {
            auto const& block = frame.renderModel.blocks[blockIndex];
            for (auto const& item : block.inline_items)
            {
                if (item.text.find(U"MATH_WRAP_SENTINEL") != std::u32string::npos)
                    return blockIndex;
            }
        }
        throw std::runtime_error("math-only wrapping paragraph was not projected as an inline block");
    }

    bool SameMetrics(MathJaxSvg const& left, MathJaxSvg const& right)
    {
        if (left.width != right.width || left.height != right.height
            || left.verticalAlign != right.verticalAlign
            || left.fragments.size() != right.fragments.size()) return false;
        for (std::size_t index = 0; index < left.fragments.size(); ++index)
        {
            auto const& a = left.fragments[index];
            auto const& b = right.fragments[index];
            if (a.renderId != b.renderId || a.width != b.width || a.height != b.height
                || a.verticalAlign != b.verticalAlign || a.breakSpace != b.breakSpace
                || a.breakBefore != b.breakBefore) return false;
        }
        return true;
    }

    void CheckReadingLayout(detail::EditorRenderFrame const& frame,
        EditorMathPreparation& preparation, std::size_t index)
    {
        auto const& context = preparation.context;
        auto sheet = CreateEditorStyleSheet(context.theme);
        EditorRenderResources resources;
        resources.InitializeOffscreen(sheet, context.surfaceWidth, context.surfaceHeight);
        EditorRenderCache cache;
        SvgNormalizer normalizer;
        MermaidRenderer mermaid;
        TreeSitterHighlighter highlighter;
        EditorSvgPainter svgPainter(resources, cache);
        EditorInlineImageRenderer images(resources, cache, sheet, normalizer, svgPainter, frame.baseDirectory, false);
        EditorTextLayoutEngine text(resources, sheet);
        EditorInteractionMap interaction;
        auto width = DocumentWidth(context);
        EditorDocumentPainter painter(resources, sheet, interaction, highlighter, {}, false,
            width, frame.selection, {}, frame.renderModel.editable_index);
        EditorDocumentBlockPreparer preparer(frame, resources, cache, sheet, text, images,
            painter, preparation.math, mermaid, normalizer, highlighter, frame.selection.active,
            width, svgPainter.Supported(), svgPainter.Supported(),
            context.theme.variant != folia::Theme::Light, 0, 0);
        auto makeLines = [](IDWriteTextLayout* layout) {
            UINT32 count = 0;
            layout->GetLineMetrics(nullptr, 0, &count);
            std::vector<DWRITE_LINE_METRICS> lines(count);
            winrt::check_hresult(layout->GetLineMetrics(lines.data(), count, &count));
            return lines;
        };
        auto first = preparer.Prepare(frame.renderModel.blocks[index], true, true);
        Require(first.layout && !first.display.mathOverlays.empty(), "reading uses math inline objects");
        auto firstLines = makeLines(first.layout.Get());
        Require(firstLines.size() > 1, "long inline formula wraps in DirectWrite");
        auto firstPositions = EditorDocumentPainter::PositionMath(first.layout.Get(), first.display.mathOverlays, width);
        for (int pass = 0; pass < 6; ++pass)
        {
            // Exercise the same offscreen preparation used when a block returns to the viewport.
            auto reread = preparer.Prepare(frame.renderModel.blocks[index], pass % 2 != 0, false);
            auto lines = makeLines(reread.layout.Get());
            Require(lines.size() == firstLines.size(), "scroll rematerialization preserves line count");
            for (std::size_t line = 0; line < lines.size(); ++line)
                Require(lines[line].length == firstLines[line].length
                    && lines[line].height == firstLines[line].height
                    && lines[line].baseline == firstLines[line].baseline,
                    "scroll rematerialization preserves line breaks and baselines");
            auto positions = EditorDocumentPainter::PositionMath(reread.layout.Get(), reread.display.mathOverlays, width);
            Require(positions.size() == firstPositions.size(), "scroll rematerialization preserves math fragments");
            for (std::size_t part = 0; part < positions.size(); ++part)
                Require(positions[part].localX == firstPositions[part].localX
                    && positions[part].localTop == firstPositions[part].localTop,
                    "scroll rematerialization preserves formula positions");
        }

        // Publication checks stay with the test-only reader-resource path.
        EditorSurfaceRenderer publicationRenderer;
        publicationRenderer.InitializeForPdf();
        const auto publicationContext = publicationRenderer.CaptureMathPreparationContext();
        auto matching = EditorSurfaceRenderer::PrepareMath(frame, publicationContext, {}, {});
        WaitForIdleBounded(matching->math);
        publicationRenderer.AdoptMathPreparation(std::move(matching));
        Require(publicationRenderer.MatchesMathPreparationContext(publicationContext),
            "renderer adopts math results with the current publication context");
        auto changedContext = publicationContext;
        changedContext.surfaceWidth += 1.0f;
        auto mismatched = EditorSurfaceRenderer::PrepareMath(frame, changedContext, {}, {});
        WaitForIdleBounded(mismatched->math);
        bool rejected = false;
        try { publicationRenderer.AdoptMathPreparation(std::move(mismatched)); }
        catch (std::logic_error const&) { rejected = true; }
        Require(rejected, "renderer rejects math results prepared at another width");
        std::cout << "reading lines=" << firstLines.size() << " repeated-placements=stable\n";
    }

    void CheckPlainDocumentCanGainMath()
    {
        EditorSession session;
        session.SetText(L"type formula: ");
        auto initialFrame = session.RenderFrame();
        Require(initialFrame.renderModel.blocks.size() == 1, "plain edit fixture has one paragraph");
        auto const owner = initialFrame.renderModel.blocks.front().id;

        EditorSurfaceRenderer publicationRenderer;
        publicationRenderer.InitializeForPdf();
        auto context = publicationRenderer.CaptureMathPreparationContext();
        auto initial = EditorSurfaceRenderer::PrepareMath(initialFrame, context, {}, {});
        auto initialStats = initial->math.Snapshot();
        Require(initial->formulaCount == 0 && initialStats.nativeRenders == 0
                && initialStats.pending == 0 && initialStats.backingPath.empty(),
            "plain document math preparation has no formulas, worker jobs, or cache file");
        publicationRenderer.AdoptMathPreparation(std::move(initial));

        auto const insertion = std::u32string(U"$x+1$");
        auto const sourceStart = session.EditableSource(owner)->size();
        const folia::TextPosition insertionPoint{owner, sourceStart, folia::TextAffinity::Downstream};
        session.SetSelection(insertionPoint, insertionPoint);
        Require(session.ExecuteCommand(folia::Command::InsertText(insertion)),
            "normal InsertText command adds inline math to the existing paragraph");
        Require(session.TextUtf8() == "type formula: $x+1$",
            "source editing preserves the exact newly inserted math spelling");
        session.SetSelection(
            {owner, sourceStart + 2, folia::TextAffinity::Downstream},
            {owner, sourceStart + 2, folia::TextAffinity::Downstream});

        auto editedFrame = session.RenderFrame();
        auto const& editedBlock = editedFrame.renderModel.blocks.front();
        auto const caret = editedFrame.selection.active;
        Require(caret.container_id == owner, "caret remains in the original edited container");
        auto editedMath = EditorSurfaceRenderer::PrepareMath(editedFrame, context, {}, {});
        Require(editedMath->formulaCount == 1,
            "unified math scan discovers a formula added after plain-document preparation");
        WaitForIdleBounded(editedMath->math);
        auto const editedFormulaCount = editedMath->formulaCount;
        publicationRenderer.AdoptMathPreparation(std::move(editedMath));

        auto const sheet = CreateEditorStyleSheet(context.theme);
        const auto contentWidth = DocumentWidth(context);
        SvgNormalizer normalizer;
        MathJaxRenderer previewRenderer;
        auto buildInlineDisplay = [&](folia::TextPosition position) {
            previewRenderer.BeginBlock(owner.v);
            auto display = BuildDisplayInlineText(editedBlock.inline_items, position,
                {owner, editedBlock.content_span.source_range.end, folia::TextAffinity::Downstream},
                previewRenderer, normalizer, sheet.textColor, sheet.body.size,
                contentWidth, true, false, true);
            previewRenderer.EndBlock();
            return display;
        };

        auto pending = buildInlineDisplay(caret);
        Require(pending.pendingMath && pending.mathPreviews.empty(),
            "editing display returns immediately while its RAM preview is pending");
        auto previewStats = previewRenderer.Snapshot();
        Require(previewStats.fileReads == 0 && previewStats.fileWrites == 0
                && previewStats.backingPath.empty(),
            "cold editing preview performs no file reads or writes");
        WaitForIdleBounded(previewRenderer);
        auto ready = buildInlineDisplay(caret);
        Require(!ready.pendingMath && ready.mathPreviews.size() == 1,
            "worker completion supplies the active inline math preview to display construction");
        previewStats = previewRenderer.Snapshot();
        Require(previewStats.fileReads == 0 && previewStats.fileWrites == 0
                && previewStats.previewSvgBytes > 0 && previewStats.backingPath.empty(),
            "ready editing preview remains resident in memory without creating a cache file");

        auto const malformed = std::u32string(U"$\\frac{x}{");
        session.SetSelection({owner, sourceStart, folia::TextAffinity::Downstream},
            {owner, sourceStart + insertion.size(), folia::TextAffinity::Upstream});
        Require(session.ExecuteCommand(folia::Command::InsertText(malformed)),
            "normal source edit can leave a temporarily incomplete formula");
        Require(session.TextUtf8() == "type formula: $\\frac{x}{",
            "incomplete formula source is preserved exactly while editing");
        auto malformedFrame = session.RenderFrame();
        auto const& malformedBlock = malformedFrame.renderModel.blocks.front();
        previewRenderer.BeginBlock(owner.v);
        auto malformedDisplay = BuildDisplayInlineText(malformedBlock.inline_items,
            malformedFrame.selection.active,
            {owner, malformedBlock.content_span.source_range.end, folia::TextAffinity::Downstream},
            previewRenderer, normalizer, sheet.textColor, sheet.body.size,
            contentWidth, true, false, true);
        previewRenderer.EndBlock();
        Require(malformedDisplay.mathPreviews.empty(),
            "temporarily incomplete formula is not published as a stale preview");

        auto const valid = std::u32string(U"$\\frac{x}{2}$");
        const auto malformedEnd = sourceStart + malformed.size();
        session.SetSelection({owner, sourceStart, folia::TextAffinity::Downstream},
            {owner, malformedEnd, folia::TextAffinity::Upstream});
        Require(session.ExecuteCommand(folia::Command::InsertText(valid)),
            "normal source edit repairs the incomplete formula");
        Require(session.TextUtf8() == "type formula: $\\frac{x}{2}$",
            "repaired formula source remains lossless");
        session.SetSelection(
            {owner, sourceStart + 3, folia::TextAffinity::Downstream},
            {owner, sourceStart + 3, folia::TextAffinity::Downstream});
        auto repairedFrame = session.RenderFrame();
        auto const& repairedBlock = repairedFrame.renderModel.blocks.front();
        auto repairedCaret = repairedFrame.selection.active;
        previewRenderer.BeginBlock(owner.v);
        auto repairedDisplay = BuildDisplayInlineText(repairedBlock.inline_items, repairedCaret,
            {owner, repairedBlock.content_span.source_range.end, folia::TextAffinity::Downstream},
            previewRenderer, normalizer, sheet.textColor, sheet.body.size,
            contentWidth, true, false, true);
        previewRenderer.EndBlock();
        Require(repairedDisplay.pendingMath,
            "repaired edit schedules its newest preview asynchronously");
        WaitForIdleBounded(previewRenderer);
        previewRenderer.BeginBlock(owner.v);
        repairedDisplay = BuildDisplayInlineText(repairedBlock.inline_items, repairedCaret,
            {owner, repairedBlock.content_span.source_range.end, folia::TextAffinity::Downstream},
            previewRenderer, normalizer, sheet.textColor, sheet.body.size,
            contentWidth, true, false, true);
        previewRenderer.EndBlock();
        Require(!repairedDisplay.pendingMath && repairedDisplay.mathPreviews.size() == 1,
            "latest valid edit is the preview published to the display");

        // Once the caret leaves the formula, the same renderer independently
        // requests the ordinary file-backed document result.
        auto outsideCaret = folia::TextPosition{owner, 0, folia::TextAffinity::Upstream};
        previewRenderer.BeginBlock(owner.v);
        auto documentDisplay = BuildDisplayInlineText(repairedBlock.inline_items, outsideCaret,
            {owner, repairedBlock.content_span.source_range.end, folia::TextAffinity::Downstream},
            previewRenderer, normalizer, sheet.textColor, sheet.body.size,
            contentWidth, true, false, true);
        previewRenderer.EndBlock();
        Require(documentDisplay.pendingMath,
            "caret leaving the formula requests document rendering asynchronously");
        WaitForIdleBounded(previewRenderer);
        auto documentStats = previewRenderer.Snapshot();
        Require(documentStats.fileWrites > 0 && !documentStats.backingPath.empty(),
            "document SVG persistence starts independently after editing preview");
        Require(session.TextUtf8() == "type formula: $\\frac{x}{2}$",
            "math preview and document caching do not alter edited source");
        std::cout << "plain-to-math editing formula-count=" << editedFormulaCount
                  << " preview-svg-bytes=" << previewStats.previewSvgBytes
                  << " document-file-writes=" << documentStats.fileWrites << '\n';
    }

    void Benchmark(std::filesystem::path const& path)
    {
        using Clock = std::chrono::steady_clock;
        auto milliseconds = [](auto start) {
            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        };
        std::vector<std::string> tex;
        for (int i = 0; i < 512; ++i)
            tex.push_back("\\frac{x_{" + std::to_string(i) + "}^2+\\sqrt{a+b}}{1+\\alpha}");
        NativeMathSession native;
        // One configured parser stays alive; font bodies belong to each batch.
        for (std::size_t size : {1, 16, 32, 64})
        {
            std::vector<NativeMathInput> inputs;
            auto start = Clock::now();
            std::size_t bytes = 0;
            for (std::size_t begin = 0; begin < tex.size(); begin += size)
            {
                inputs.clear();
                for (auto i = begin; i < (std::min)(begin + size, tex.size()); ++i)
                    inputs.push_back({tex[i], false, 16, 700});
                for (auto const& result : native.RenderBatch(inputs))
                {
                    Require(static_cast<bool>(result), "native benchmark produces usable SVG");
                    for (auto const& fragment : result.fragments) bytes += fragment.svg->size();
                }
            }
            std::cout << "benchmark native formulas=" << tex.size() << " batch=" << size
                << " ms=" << milliseconds(start) << " svg-bytes=" << bytes << '\n';
        }
        std::string source;
        if (!path.empty())
        {
            Require(std::filesystem::file_size(path) <= 16 * 1024 * 1024, "benchmark input <=16MiB");
            std::ifstream file(path, std::ios::binary);
            Require(static_cast<bool>(file), "benchmark input is readable");
            source.assign(std::istreambuf_iterator<char>(file), {});
        }
        else for (auto const& formula : tex) source += "Formula $" + formula + "$\n\n";
        EditorMathPreparationContext context;
        context.theme = folia::default_theme_profile(folia::Theme::Light);
        context.surfaceWidth = 900;
        context.surfaceHeight = 600;
        context.ready = true;
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            auto start = Clock::now();
            EditorSession session;
            session.SetText(winrt::to_hstring(source));
            auto parsed = milliseconds(start);
            start = Clock::now();
            auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            auto elapsed = milliseconds(start);
            auto stats = prepared->math.Snapshot();
            Require(stats.pending == 0 && stats.residentSvgBytes == 0, "benchmark leaves no pending work or resident SVG");
            std::cout << "benchmark preparation repeat=" << repeat << " input-bytes=" << source.size()
                << " parse-ms=" << parsed << " math-ms=" << elapsed << " formulas=" << prepared->formulaCount
                << " renders=" << stats.nativeRenders << " batches=" << stats.nativeBatches
                << " writes=" << stats.fileWrites << " private-bytes=" << ReadProcessMemory().privateBytes << '\n';
        }
    }

    void RunProbe()
    {
        CheckPlainDocumentCanGainMath();
        const auto processStart = ReadProcessMemory();
        auto stageStart = std::chrono::steady_clock::now();
        std::string longFormula = "x";
        for (std::size_t index = 0; index < 80; ++index) longFormula += "+x";

        EditorSession session;
        std::wstring source =
            L"# Math preparation\n\n"
            L"A long paragraph exercises DirectWrite wrapping at more than one surface width. "
            L"It carries an inline formula $\\frac{1}{2}$ between words, then continues with enough "
            L"ordinary text to occupy several lines when the document is narrow. The source must stay "
            L"exactly as entered after every preparation pass.\n\n"
            L"MATH_WRAP_SENTINEL $";
        for (char character : longFormula) source.push_back(static_cast<wchar_t>(character));
        source +=
            L"$\n\n"
            L"- First list item with $x^2$\n"
            L"  - Nested list item\n\n"
            L"> Quoted paragraph with $\\mathbb{R}$ and wrapped explanatory text.\n\n"
            L"| Value | Formula |\n"
            L"| --- | --- |\n"
            L"| half | $\\frac{1}{2}$ |\n\n"
            L"$$\n\\int_0^1 x^2 \\, dx\n$$\n";
        session.SetText(winrt::hstring(source));
        const auto originalText = session.TextUtf8();
        auto frame = session.RenderFrame();
        const auto originalStamps = Stamp(frame);
        const auto mathOnlyBlock = MathOnlyBlockIndex(frame);
        Require(frame.renderModel.blocks.size() >= 6, "rich sample produced multiple render blocks");
        Require(std::ranges::any_of(frame.renderModel.blocks, [](auto const& block) {
            return block.kind == folia::RenderBlockKind::Quote;
        }), "sample includes a quote block");
        Require(std::ranges::any_of(frame.renderModel.blocks, [](auto const& block) {
            return block.kind == folia::RenderBlockKind::Table;
        }), "sample includes a table block");
        Require(std::ranges::any_of(frame.renderModel.blocks, [](auto const& block) {
            return block.kind == folia::RenderBlockKind::Math;
        }), "sample includes display math");
        auto hasIndentedFlow = [](auto const& self, auto const& block) -> bool {
            if (block.flow_local_indent_columns > 0) return true;
            return std::ranges::any_of(block.child_blocks, [&](auto const& child) { return self(self, child); });
        };
        Require(std::ranges::any_of(frame.renderModel.blocks, [&](auto const& block) {
            return hasIndentedFlow(hasIndentedFlow, block);
        }), "sample includes an indented list flow");
        ReportStage("rich-model", std::chrono::steady_clock::now() - stageStart);

        const EditorMathPreparationContext baseContext{
            folia::default_theme_profile(), 1, 1280.0f, 1000.0f, true };
        auto narrowContext = baseContext;
        narrowContext.surfaceWidth = 300.0f;
        narrowContext.surfaceHeight = 1000.0f;
        auto wideContext = narrowContext;
        wideContext.surfaceWidth = 1100.0f;

        stageStart = std::chrono::steady_clock::now();
        auto narrow = EditorSurfaceRenderer::PrepareMath(frame, narrowContext, {}, {});
        Require(narrow->formulaCount > 0, "narrow math scan finds formulas across rich block types");
        Require(narrow->math.Snapshot().nativeSessionActive
                && narrow->math.Snapshot().residentSvgBytes == 0,
            "opening retains the lightweight engine and geometry without offscreen SVG bytes");
        WaitForIdleBounded(narrow->math);
        ReportStage("rich-narrow-math", std::chrono::steady_clock::now() - stageStart);
        Require(session.TextUtf8() == originalText && Stamp(frame) == originalStamps,
            "narrow math preparation preserves exact source and render model identities");

        stageStart = std::chrono::steady_clock::now();
        auto wide = EditorSurfaceRenderer::PrepareMath(frame, wideContext, {}, {});
        WaitForIdleBounded(wide->math);
        ReportStage("rich-wide-math", std::chrono::steady_clock::now() - stageStart);
        Require(session.TextUtf8() == originalText && Stamp(frame) == originalStamps,
            "wide math preparation preserves exact source and render model identities");

        const auto em = baseContext.theme.typography.body.size;
        const auto width = DocumentWidth(narrowContext);
        const auto color = TextColor(narrowContext.theme);
        auto preparedMath = narrow->math.Render(longFormula, false, em, width, color);
        auto repeatedMath = narrow->math.Render(longFormula, false, em, width, color);
        auto wideMath = wide->math.Render(longFormula, false, em,
            DocumentWidth(wideContext), color);
        Require(preparedMath && repeatedMath && wideMath && static_cast<bool>(*preparedMath)
                && static_cast<bool>(*wideMath), "prepared math returns usable inline metrics");
        Require(preparedMath->fragments.size() > 1
                && std::ranges::any_of(preparedMath->fragments, [](auto const& fragment) {
                    return fragment.breakBefore;
                }) && wideMath->fragments.size() > 1,
            "MathJax supplies optional inline fragment breaks at both widths");
        Require(SameMetrics(*preparedMath, *repeatedMath),
            "repeated prepared math preserves metrics and SVG render ids");
        CheckReadingLayout(frame, *narrow, mathOnlyBlock);

        std::stop_source cancellation;
        cancellation.request_stop();
        bool cancelled = false;
        try
        {
            (void)EditorSurfaceRenderer::PrepareMath(frame, narrowContext,
                cancellation.get_token(), {});
        }
        catch (winrt::hresult_canceled const&) { cancelled = true; }
        Require(cancelled, "pre-cancelled math preparation throws and yields no result");

        EditorSession largePlainSession;
        std::wstring largePlainSource;
        for (int block = 0; block < 4100; ++block) largePlainSource += L"Plain paragraph with no formula.\n\n";
        largePlainSession.SetText(winrt::hstring(largePlainSource));
        auto largePlainFrame = largePlainSession.RenderFrame();
        Require(largePlainFrame.renderModel.virtualized, "large plain fixture uses model virtualization");
        stageStart = std::chrono::steady_clock::now();
        auto plain = EditorSurfaceRenderer::PrepareMath(largePlainFrame, narrowContext, {}, {});
        WaitForIdleBounded(plain->math);
        ReportStage("large-plain-math", std::chrono::steady_clock::now() - stageStart);
        auto plainStats = plain->math.Snapshot();
        Require(plain->formulaCount == 0 && plainStats.nativeRenders == 0 && plainStats.pending == 0,
            "large plain document math preparation performs no native renders");

        EditorSession largeSession;
        std::wstring largeSource;
        for (int block = 0; block < 4100; ++block) largeSource += L"Paragraph $1$\n\n";
        largeSession.SetText(winrt::hstring(largeSource));
        auto largeOriginalText = largeSession.TextUtf8();
        auto largeFrame = largeSession.RenderFrame();
        Require(largeFrame.renderModel.virtualized, "large math fixture uses model virtualization");
        stageStart = std::chrono::steady_clock::now();
        auto large = EditorSurfaceRenderer::PrepareMath(largeFrame, narrowContext, {}, {});
        WaitForIdleBounded(large->math);
        ReportStage("large-math-prewarm", std::chrono::steady_clock::now() - stageStart);
        Require(large->formulaCount == 4100,
            "math prewarm counts every formula occurrence across virtualized blocks");
        Require(largeSession.TextUtf8() == largeOriginalText,
            "virtualized math prewarm preserves exact source");
        auto largeStats = large->math.Snapshot();
        Require(largeStats.pending == 0 && largeStats.nativeRenders > 0,
            "large virtualized math prewarm finishes its asynchronous native work");

        EditorSession distinctSession;
        std::wstring distinctSource;
        for (int i = 0; i < 512; ++i)
            distinctSource += L"Formula $x+" + std::to_wstring(i) + L"$\n\n";
        distinctSession.SetText(winrt::hstring(distinctSource));
        auto distinctFrame = distinctSession.RenderFrame();
        auto distinct = EditorSurfaceRenderer::PrepareMath(distinctFrame, narrowContext, {}, {});
        auto distinctStats = distinct->math.Snapshot();
        Require(distinct->formulaCount == 512 && distinctStats.nativeRenders == 512
                && distinctStats.nativeBatches == 16 && distinctStats.pending == 0
                && distinctStats.residentSvgBytes == 0 && distinctStats.nativeSessionActive,
            "opening distinct formulas uses real bounded native batches and retains only the lightweight engine");

        EditorSession oneBlockSession;
        std::wstring oneBlockSource;
        for (int i = 0; i < 80; ++i) oneBlockSource += L"$y+" + std::to_wstring(i) + L"$ ";
        oneBlockSession.SetText(winrt::hstring(oneBlockSource));
        auto oneBlockFrame = oneBlockSession.RenderFrame();
        auto oneBlock = EditorSurfaceRenderer::PrepareMath(oneBlockFrame, narrowContext, {}, {});
        auto oneBlockStats = oneBlock->math.Snapshot();
        Require(oneBlock->formulaCount == 80 && oneBlockStats.entries == 80
                && oneBlockStats.nativeRenders == 80 && oneBlockStats.nativeBatches == 3
                && oneBlockStats.residentSvgBytes == 0,
            "batch boundaries inside one owner preserve all formula metadata without a large queue");
        std::cout << "opening-native-batch distinct=512 calls=" << distinctStats.nativeBatches
                  << " one-owner=80 calls=" << oneBlockStats.nativeBatches
                  << " document-resident-svg=0 engine=retained\n";

        Require(session.TextUtf8() == originalText && Stamp(frame) == originalStamps,
            "all math preparation paths preserve exact source and render model identities");

        auto finalMemory = ReadProcessMemory();
        std::cout << "memory-baseline-private-bytes=" << processStart.privateBytes
                  << " final-private-bytes=" << finalMemory.privateBytes
                  << " peak-working-bytes=" << finalMemory.peakWorkingSet << '\n';
        std::cout << "math-preparation formulas-narrow=" << narrow->formulaCount
                  << " formulas-wide=" << wide->formulaCount
                  << " math-only-virtualized-formulas=" << large->formulaCount
                  << " unique-native-renders=" << largeStats.nativeRenders
                  << " math-fragments=" << preparedMath->fragments.size()
                  << " wide-math-fragments=" << wideMath->fragments.size()
                  << " cancellation=success reading-layout=stable\n";
        std::cout << "native MathJax math preparation probe: PASS\n";
    }
}

int wmain(int argc, wchar_t** argv)
{
    std::cout << std::unitbuf;
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if (argc == 2 && std::wstring_view(argv[1]) == L"--edit-only")
        {
            CheckPlainDocumentCanGainMath();
            std::cout << "editing preview probe: PASS\n";
        }
        else if (argc >= 2 && std::wstring_view(argv[1]) == L"--benchmark")
            Benchmark(argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::path{});
        else RunProbe();
        winrt::uninit_apartment();
        return 0;
    }
    catch (winrt::hresult_error const& error)
    {
        std::cerr << "native MathJax math preparation probe: FAIL: HRESULT 0x"
                  << std::hex << static_cast<std::uint32_t>(error.code().value)
                  << ": " << winrt::to_string(error.message()) << '\n';
        return 1;
    }
    catch (std::exception const& error)
    {
        std::cerr << "native MathJax math preparation probe: FAIL: " << error.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "native MathJax math preparation probe: FAIL: unknown exception\n";
        return 1;
    }
}
