#pragma once

import folia.core.render_model;
import folia.markdown.text_edit;

#include "editor/rendering/painting/EditorDocumentPainter.h"
#include "editor/rendering/media/EditorInlineImageRenderer.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"
#include "editor/rendering/EditorRenderCache.h"
#include "editor/rendering/EditorRenderResources.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/layout/EditorTextLayoutEngine.h"
#include "editor/session/EditorRenderFrame.h"
#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/mermaid/MermaidRenderer.h"
#include "windows/media/svg/SvgNormalizer.h"

namespace winrt::Folia
{
    // Builds one prepared block at a time. A background pass can visit the
    // entire document, so its syntax cache retains only the latest context.
    class EditorDocumentBlockPreparer final
    {
    public:
        EditorDocumentBlockPreparer(
            detail::EditorRenderFrame const& frame,
            EditorRenderResources& resources,
            EditorRenderCache& renderCache,
            EditorStyleSheet const& styleSheet,
            EditorTextLayoutEngine& textLayoutEngine,
            EditorInlineImageRenderer& inlineImages,
            EditorDocumentPainter& documentPainter,
            folia::platform::MathJaxRenderer& mathJax,
            folia::platform::MermaidRenderer& mermaid,
            folia::platform::SvgNormalizer& svgNormalizer,
            TreeSitterHighlighter& treeSitter,
            folia::TextPosition caret,
            float documentWidth,
            bool mathSvgSupported,
            bool mermaidSvgSupported,
            bool darkTheme,
            std::uint64_t embeddedGeneration,
            std::uint64_t remoteImageGeneration);

        void InitializeMetadata(
            EditorPreparedDocument::Block& prepared,
            folia::RenderBlock const& block) const;
        void InitializeContentMetadata(
            EditorPreparedDocument::Block& prepared,
            folia::RenderBlock const& block) const;
        EditorPreparedDocument::Block Prepare(
            folia::RenderBlock const& block,
            bool requestEmbedded,
            bool highPriority,
            std::optional<EditorFlowLayoutContext> flowContext = {},
            EditorTableBlockRenderer::PreparedTable* previousTable = nullptr);

        static std::vector<std::string> ImageSources(
            EditorPreparedDocument::Block const& prepared);

    private:
        DisplayInlineText BuildDisplay(
            folia::RenderBlock const& block,
            float width,
            bool requestEmbedded,
            bool highPriority);
        void ApplyMermaidPresentation(
            DisplayInlineText& display,
            folia::RenderBlock const& block,
            bool requestEmbedded);

        detail::EditorRenderFrame const& frame;
        EditorRenderResources& resources;
        EditorRenderCache& renderCache;
        EditorStyleSheet const& styleSheet;
        EditorTextLayoutEngine& textLayoutEngine;
        EditorInlineImageRenderer& inlineImages;
        EditorDocumentPainter& documentPainter;
        folia::platform::MathJaxRenderer& mathJax;
        folia::platform::MermaidRenderer& mermaid;
        folia::platform::SvgNormalizer& svgNormalizer;
        TreeSitterHighlighter& treeSitter;
        folia::TextPosition caret;
        float documentWidth = 0.0f;
        bool mathSvgSupported = false;
        bool mermaidSvgSupported = false;
        bool darkTheme = false;
        std::uint64_t embeddedGeneration = 0;
        std::uint64_t remoteImageGeneration = 0;
        std::shared_ptr<const std::u32string> sourceCodeContext;
        std::string sourceCodeLanguage;
        std::vector<SyntaxHighlightRange> sourceCodeHighlights;
    };
}
