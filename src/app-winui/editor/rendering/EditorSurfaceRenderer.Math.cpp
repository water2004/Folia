#include "pch.h"

import std;
import folia.markdown.utf;
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorPreparedDocumentCache.h"

namespace winrt::Folia
{
    EditorMathPreparationContext EditorSurfaceRenderer::CaptureMathPreparationContext() const
    {
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext5> svgContext;
        if (resources.d2dContext) resources.d2dContext.As(&svgContext);
        return {themeProfile, themeRevision, resources.surfaceWidthDip, resources.surfaceHeightDip,
            resources.Ready(), static_cast<bool>(svgContext), mediaSettings};
    }

    bool EditorSurfaceRenderer::MatchesMathPreparationContext(EditorMathPreparationContext const& context) const
    {
        return context.ready && resources.Ready() && context.themeRevision == themeRevision
            && context.surfaceWidth == resources.surfaceWidthDip && context.media == mediaSettings;
    }

    std::unique_ptr<EditorMathPreparation> EditorSurfaceRenderer::PrepareMath(
        detail::EditorRenderFrame const& frame, EditorMathPreparationContext const& context,
        std::stop_token stop, std::function<void(std::size_t, std::size_t)> progress)
    {
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        auto result = std::make_unique<EditorMathPreparation>();
        result->context = context;
        auto sheet = CreateEditorStyleSheet(context.theme);
        auto metrics = EditorDocumentLayoutMetrics(frame.renderModel, context.surfaceWidth,
            sheet.horizontalPadding, sheet.code.size);
        auto width = metrics.width;
        auto batch = result->math.BeginBatch();
        auto formula = [&](std::u32string_view tex, bool display, float available, D2D1_COLOR_F foreground)
        {
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            batch.Render(folia::cps_to_utf8(tex), display, sheet.body.size,
                available, SvgColor(foreground), stop);
            ++result->formulaCount;
            // Count newly staged renders, rather than repeated occurrences.
            // Source-model chunks do not delimit a native/font request.
            if (batch.Size() == 32)
            {
                batch.Submit();
                result->math.WaitForIdle(stop);
            }
        };
        auto inlines = [&](auto const& self, std::vector<folia::InlineRenderItem> const& items, float available) -> void
        {
            for (auto const& item : items)
            {
                if (item.kind == folia::InlineRenderItem::Kind::Math)
                    formula(item.text, item.special().display == folia::MathDisplayMode::Block, available,
                        InlineForeground(item.style, sheet.textColor));
                else if (item.kind == folia::InlineRenderItem::Kind::Link)
                    self(self, item.special().semantic().children, available);
            }
        };
        auto block = [&](auto const& self, folia::RenderBlock const& source, float available) -> void
        {
            if (source.source_mode || source.source_code) return;
            auto contentWidth = EditorBlockContentWidth(source, available);
            if (source.kind == folia::RenderBlockKind::Math)
                formula(source.special().tex, true, contentWidth, sheet.textColor);
            else if (source.kind == folia::RenderBlockKind::Table)
            {
                auto const& special = source.special();
                auto cellWidth = EditorTableCellContentWidth(available, special.column_count);
                for (auto const& cell : special.table_cells) inlines(inlines, cell, cellWidth);
            }
            else if (!source.inline_items.empty())
                inlines(inlines, source.inline_items, contentWidth);
            else
                for (auto const& child : source.child_blocks) self(self, child, contentWidth);
        };
        auto count = frame.renderModel.blocks.size();
        constexpr std::size_t blockBatchSize = 16;
        if (progress) progress(0, count * 2);
        for (std::size_t begin = 0; begin < count; begin += blockBatchSize)
        {
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            auto end = (std::min)(count, begin + blockBatchSize);
            if (frame.materializeBlocks) frame.materializeBlocks(begin, end);
            for (auto index = begin; index < end; ++index)
            {
                auto const& source = frame.renderModel.blocks[index];
                result->math.BeginBlock(source.id.v);
                block(block, source, width);
                result->math.EndBlock();
                if (progress) progress(index, count * 2);
            }
            if (frame.releaseBlocksOutside) frame.releaseBlocksOutside(end, end);
            // Model materialization remains bounded independently of the
            // at-most-32 staged native requests, even for sparse formulas.
        }
        batch.Submit();
        result->math.WaitForIdle(stop);
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        // Prepare compact, exact text geometry on the loader's background
        // thread. The UI's first frame never has to shape the entire document,
        // and no long-lived glyph/paint/display objects accompany publication.
        struct MeasurementApartment
        {
            HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            MeasurementApartment()
            {
                if (FAILED(status) && status != RPC_E_CHANGED_MODE) winrt::check_hresult(status);
            }
            ~MeasurementApartment() { if (SUCCEEDED(status)) CoUninitialize(); }
        } apartment;
        EditorRenderResources measurementResources;
        measurementResources.InitializeTextMeasurement(sheet, context.surfaceWidth, context.surfaceHeight);
        EditorRenderCache measurementCache;
        EditorTextLayoutEngine engine(measurementResources, sheet);
        EditorSvgPainter svg(measurementResources, measurementCache);
        SvgNormalizer normalizer;
        TreeSitterHighlighter syntax;
        MermaidRenderer diagrams;
        diagrams.SetTheme(context.media.mermaidTheme);
        EditorInteractionMap interaction;
        EditorInlineImageRenderer images(measurementResources, measurementCache, sheet,
            normalizer, svg, frame.baseDirectory, false,
            context.media.imageMaxWidthPercent, context.imageMaxHeight);
        EditorDocumentPainter painter(measurementResources, sheet, interaction, syntax, {},
            false, metrics.right, frame.selection, {}, frame.renderModel.editable_index);
        EditorDocumentBlockPreparer preparer(frame, measurementResources, measurementCache,
            sheet, engine, images, painter, result->math, diagrams, normalizer, syntax,
            frame.selection.active, width, context.svgSupported, context.svgSupported,
            context.theme.variant != folia::Theme::Light, 0, 0);
        EditorPreparedDocumentCache cache(frame, sheet, images, preparer, result->document);
        cache.Reconcile(frame.selection, width, context.themeRevision);
        cache.EnsureGeometry(stop, [&](std::size_t completed, std::size_t)
        {
            if (progress) progress(count + completed, count * 2);
        });
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        if (progress) progress(count * 2, count * 2);
        return result;
    }

    void EditorSurfaceRenderer::AdoptMathPreparation(std::unique_ptr<EditorMathPreparation> preparation)
    {
        if (!preparation || !MatchesMathPreparationContext(preparation->context))
            throw std::logic_error("Math preparation context changed before publication");
        ResetDocumentCaches();
        mathJax = std::move(preparation->math);
        preparedDocument = std::move(preparation->document);
        if (preparedDocument) totalDocumentHeight = preparedDocument->totalHeight;
        // The viewport path only creates visible visual resources from the
        // already measured geometry; reflow after edits remains fragment-local.
        AttachMathCompletion();
    }

    void EditorSurfaceRenderer::SetMathPreparationStopToken(std::stop_token stop)
    {
        mathPreparationStop = stop;
        mathJax.SetPreparationStopToken(stop);
    }
}
