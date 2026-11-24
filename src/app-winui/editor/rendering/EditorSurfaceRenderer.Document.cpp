#include "pch.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"
#include "localization/Localization.h"

import folia.core.render_model;

#include "editor/rendering/layout/EditorContentPreparation.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/layout/EditorDocumentBlockPreparer.h"
#include "editor/rendering/painting/EditorDocumentPainter.h"
#include "editor/rendering/layout/EditorDocumentPreparationPass.h"
#include "editor/rendering/painting/EditorDocumentRenderPass.h"
#include "editor/rendering/media/EditorInlineImageRenderer.h"
#include "editor/rendering/painting/EditorSvgPainter.h"
#include "editor/rendering/EditorTableBlockRenderer.h"
#include "editor/rendering/layout/EditorTextLayoutEngine.h"

namespace winrt::Folia
{
    void EditorSurfaceRenderer::DrawDocument(
        detail::EditorRenderFrame const& sourceFrame,
        std::optional<float> sourceTopOverride)
    {
        auto frame = sourceFrame;
        if (printMode) frame.completion.reset();
        if (!mathModelRevision || *mathModelRevision != frame.renderModel.revision)
        {
            std::vector<std::uint64_t> owners;
            owners.reserve(frame.renderModel.blocks.size());
            for (auto const& block : frame.renderModel.blocks) owners.push_back(block.id.v);
            mathJax.RetainBlocks(owners);
            mathModelRevision = frame.renderModel.revision;
        }
        interactionMap.Clear();
        nonInteractiveRegions.clear();
        inlineMathPreviewBounds.reset();
        auto metrics = EditorDocumentLayoutMetrics(frame.renderModel, resources.surfaceWidthDip,
            styleSheet.horizontalPadding, styleSheet.code.size);
        auto sourceGutterWidth = metrics.sourceGutterWidth;
        auto sourceGutterLeft = metrics.sourceGutterLeft;
        auto documentLeft = metrics.left;
        auto documentRight = metrics.right;
        auto documentWidth = metrics.width;
        // Printed pages use document coordinates directly.  In particular,
        // the final page may start below the editor viewport's maximum scroll
        // offset when less than one page remains, so it must not be clamped as
        // an interactive viewport position.
        auto scrollOffset = sourceTopOverride.value_or(scrollState.Offset());
        auto y = styleSheet.verticalPadding - scrollOffset;
        auto selection = printMode ? folia::TextSelection{} : frame.selection;
        auto caret = selection.active;

        EditorSvgPainter svgPainter(resources, renderCache);
        auto svgSupported = svgPainter.Supported();
        auto mathSvgSupported = svgSupported;
        EditorTextLayoutEngine textLayoutEngine(resources, styleSheet);
        EditorInlineImageRenderer inlineImages(
            resources,
            renderCache,
            styleSheet,
            svgNormalizer,
            svgPainter,
            frame.baseDirectory,
            !printMode,
            mediaSettings.imageMaxWidthPercent,
            printMode ? std::optional{resources.surfaceHeightDip} : std::nullopt);
        std::vector<std::uint64_t> visibleMath;
        auto drawMath = [&](folia::platform::MathJaxSvgFragment const& fragment, D2D1_POINT_2F origin, D2D1_COLOR_F, float scale) {
            if (origin.x + fragment.width * scale < 0.0f || origin.y + fragment.height * scale < 0.0f
                || origin.x > resources.surfaceWidthDip || origin.y > resources.surfaceHeightDip)
                return true;
            visibleMath.push_back(fragment.renderId);
            auto payload = fragment.svg ? fragment.svg : mathJax.GetSvg(fragment.renderId);
            if (!payload && printMode)
            {
                // PDF export runs off the UI thread and must record complete pages.
                mathJax.SetVisibleSvgIds(std::span{&fragment.renderId, std::size_t{1}});
                mathJax.WaitForIdle(mathPreparationStop);
                payload = mathJax.GetSvg(fragment.renderId);
            }
            if (!payload) return true; // Geometry is final; the background read will request a repaint.
            return svgPainter.Draw(
                fragment.renderId,
                *payload,
                fragment.width * scale,
                fragment.height * scale,
                origin,
                fragment.width,
                fragment.height);
        };
        auto drawMathFallback = [&](folia::TextSpan, D2D1_POINT_2F origin) {
            auto label = Localize(L"Formula");
            resources.d2dContext->DrawTextW(
                label.c_str(),
                static_cast<std::uint32_t>(label.size()),
                resources.codeFormat.Get(),
                D2D1::RectF(origin.x, origin.y, documentRight, origin.y + styleSheet.code.lineHeight),
                resources.textBrush.Get());
        };
        auto drawMermaid = [&](folia::platform::MermaidSvg const& diagram, float width, float height, D2D1_POINT_2F origin) {
            return svgPainter.Draw(
                diagram.renderId,
                diagram.svg,
                width,
                height,
                origin,
                diagram.width,
                diagram.height);
        };
        EditorDocumentPainter documentPainter(
            resources,
            styleSheet,
            interactionMap,
            treeSitter,
            pointerPosition,
            printMode,
            documentRight,
            selection,
            printMode ? std::span<const detail::EditorSearchHighlight>{} : frame.searchHighlights,
            frame.renderModel.editable_index);

        if (frame.renderModel.blocks.empty() && !printMode)
        {
            auto message = Localize(L"EmptyDocumentHint");
            resources.d2dContext->DrawTextW(
                message.c_str(),
                static_cast<std::uint32_t>(message.size()),
                resources.textFormat.Get(),
                D2D1::RectF(documentLeft, y, documentRight, y + 80.0f),
                resources.mutedBrush.Get());
        }

        auto remoteImageGeneration = renderCache.images.ResourceGeneration();
        EditorDocumentBlockPreparer blockPreparer(
            frame,
            resources,
            renderCache,
            styleSheet,
            textLayoutEngine,
            inlineImages,
            documentPainter,
            mathJax,
            mermaid,
            svgNormalizer,
            treeSitter,
            caret,
            documentWidth,
            mathSvgSupported,
            svgSupported,
            themeProfile.variant != folia::Theme::Light,
            embeddedGeneration,
            remoteImageGeneration);
        EditorDocumentPreparationPass preparationPass(
            frame,
            resources,
            styleSheet,
            inlineImages,
            blockPreparer,
            scrollState,
            preparedDocument,
            selection,
            documentWidth,
            themeRevision,
            embeddedGeneration,
            remoteImageGeneration,
            scrollOffset,
            printMode);
        auto preparation = preparationPass.Prepare();
        scrollOffset = preparation.scrollOffset;
        if (preparation.invalidateRequested) Invalidate();
        EditorDocumentRenderPass renderPass(
            resources,
            styleSheet,
            interactionMap,
            caretAnimation,
            nonInteractiveRegions,
            inlineImages,
            documentPainter,
            drawMath,
            drawMathFallback,
            drawMermaid);
        inlineMathPreviewBounds = renderPass.Paint(
            frame,
            *preparedDocument,
            selection,
            caret,
            documentLeft,
            documentRight,
            documentWidth,
            sourceGutterLeft,
            sourceGutterWidth,
            scrollOffset,
            preparation.visible.begin,
            preparation.visible.end,
            printMode,
            pointerPosition,
            draggedTableAction,
            tableDropIndex);
        std::vector<std::uint64_t> visibleMathBlocks;
        visibleMathBlocks.reserve(preparation.visible.end - preparation.visible.begin);
        for (auto index = preparation.visible.begin; index < preparation.visible.end; ++index)
            visibleMathBlocks.push_back(frame.renderModel.blocks[index].id.v);
        mathJax.CancelOffscreenRenders(visibleMathBlocks);
        mathJax.SetVisibleSvgIds(visibleMath);
        totalDocumentHeight = preparedDocument->totalHeight;
        if (!sourceTopOverride) scrollState.Clamp(MaximumScrollOffset());
    }

}
