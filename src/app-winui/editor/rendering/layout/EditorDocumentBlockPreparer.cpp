#include "pch.h"
#include "editor/rendering/layout/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorContentGeometry.h"

import folia.markdown.utf;

namespace winrt::Folia
{
    namespace
    {
        bool InlineItemsContain(
            std::vector<folia::InlineRenderItem> const& items,
            folia::InlineRenderItem::Kind kind)
        {
            for (auto const& item : items)
            {
                if (item.kind == kind
                    || InlineItemsContain(item.special().semantic().children, kind)) return true;
            }
            return false;
        }

        bool RenderBlockContainsMath(folia::RenderBlock const& block)
        {
            if (block.kind == folia::RenderBlockKind::Math
                || InlineItemsContain(block.inline_items, folia::InlineRenderItem::Kind::Math)) return true;
            for (auto const& cell : block.special().table_cells)
                if (InlineItemsContain(cell, folia::InlineRenderItem::Kind::Math)) return true;
            for (auto const& child : block.child_blocks)
                if (RenderBlockContainsMath(child)) return true;
            return false;
        }

        bool RenderBlockContainsImage(folia::RenderBlock const& block)
        {
            if (block.kind == folia::RenderBlockKind::Image
                || InlineItemsContain(block.inline_items, folia::InlineRenderItem::Kind::Image)) return true;
            for (auto const& cell : block.special().table_cells)
                if (InlineItemsContain(cell, folia::InlineRenderItem::Kind::Image)) return true;
            for (auto const& child : block.child_blocks)
                if (RenderBlockContainsImage(child)) return true;
            return false;
        }

        bool RenderBlockContainsMermaid(folia::RenderBlock const& block)
        {
            if (!block.source_mode
                && block.kind == folia::RenderBlockKind::Code
                && IsMermaidLanguage(block.special().language))
                return true;
            for (auto const& child : block.child_blocks)
                if (RenderBlockContainsMermaid(child)) return true;
            return false;
        }

        std::size_t CodepointIndexAtUtf16(
            std::u32string_view text,
            std::uint32_t utf16Offset)
        {
            std::size_t index = 0;
            std::uint32_t current = 0;
            while (index < text.size() && current < utf16Offset)
            {
                current += text[index] > 0xffff ? 2u : 1u;
                ++index;
            }
            return index;
        }

        void ReplaceDisplayRangeWithMermaid(
            DisplayInlineText& display,
            std::uint32_t start,
            std::uint32_t end,
            folia::platform::MermaidSvg svg,
            folia::TextSpan sourceSpan)
        {
            if (start >= end || end > folia::utf16_len(display.text)) return;
            auto startCodepoint = CodepointIndexAtUtf16(display.text, start);
            auto endCodepoint = CodepointIndexAtUtf16(display.text, end);
            display.text.replace(
                startCodepoint,
                endCodepoint - startCodepoint,
                1,
                U'\uFFFC');

            auto eraseBegin = display.displayToSource.begin()
                + static_cast<std::ptrdiff_t>(start);
            auto eraseEnd = display.displayToSource.begin()
                + static_cast<std::ptrdiff_t>(end);
            display.displayToSource.erase(eraseBegin, eraseEnd);
            display.displayToSource.insert(
                display.displayToSource.begin()
                    + static_cast<std::ptrdiff_t>(start),
                EditorDisplayPosition{
                    {
                        sourceSpan.container_id,
                        sourceSpan.source_range.start,
                        folia::TextAffinity::Downstream,
                    },
                    EditorDisplayPositionKind::Generated,
                });

            auto const removed = end - start;
            auto const shift = static_cast<std::int64_t>(1)
                - static_cast<std::int64_t>(removed);
            std::erase_if(
                display.ranges,
                [&](InlineStyleRange& range)
                {
                    auto rangeEnd = range.start + range.length;
                    if (rangeEnd <= start) return false;
                    if (range.start >= end)
                    {
                        range.start = static_cast<std::uint32_t>(
                            static_cast<std::int64_t>(range.start) + shift);
                        return false;
                    }
                    return true;
                });
            auto shiftOverlay = [&](auto& overlays)
            {
                std::erase_if(
                    overlays,
                    [&](auto& overlay)
                    {
                        if (overlay.displayStart >= end)
                        {
                            overlay.displayStart = static_cast<std::uint32_t>(
                                static_cast<std::int64_t>(overlay.displayStart)
                                + shift);
                            return false;
                        }
                        return overlay.displayStart >= start;
                    });
            };
            shiftOverlay(display.mathOverlays);
            shiftOverlay(display.imageOverlays);
            shiftOverlay(display.mermaidOverlays);
            shiftOverlay(display.indentOverlays);
            shiftOverlay(display.blockSpacingOverlays);
            shiftOverlay(display.taskCheckboxOverlays);
            shiftOverlay(display.footnoteOverlays);

            auto style = folia::InlineStyle::plain();
            style.code = folia::CodeStyle::Block;
            display.ranges.push_back({
                start,
                1,
                style,
                false,
                SyntaxHighlightKind::None,
            });
            display.mermaidOverlays.push_back({
                start,
                std::move(svg),
                sourceSpan,
            });
        }

        void CollectInlineOwners(
            std::vector<folia::InlineRenderItem> const& items,
            std::unordered_set<std::uint64_t>& seen,
            std::vector<folia::NodeId>& owners)
        {
            for (auto const& item : items)
            {
                auto owner = item.source_span.container_id;
                if (owner.v != 0 && seen.insert(owner.v).second) owners.push_back(owner);
                CollectInlineOwners(item.special().semantic().children, seen, owners);
            }
        }

        void CollectRenderOwners(
            folia::RenderBlock const& block,
            std::unordered_set<std::uint64_t>& seen,
            std::vector<folia::NodeId>& owners)
        {
            auto add = [&](folia::NodeId owner)
            {
                if (owner.v != 0 && seen.insert(owner.v).second) owners.push_back(owner);
            };
            add(block.id);
            add(block.source_span.container_id);
            add(block.content_span.container_id);
            CollectInlineOwners(block.inline_items, seen, owners);
            for (auto const& cell : block.special().table_cells)
                CollectInlineOwners(cell, seen, owners);
            for (auto const& span : block.special().table_cell_spans) add(span.container_id);
            for (auto const& child : block.child_blocks)
                CollectRenderOwners(child, seen, owners);
        }
    }

    EditorDocumentBlockPreparer::EditorDocumentBlockPreparer(
        detail::EditorRenderFrame const& valueFrame,
        EditorRenderResources& valueResources,
        EditorRenderCache& valueRenderCache,
        EditorStyleSheet const& valueStyleSheet,
        EditorTextLayoutEngine& valueTextLayoutEngine,
        EditorInlineImageRenderer& valueInlineImages,
        EditorDocumentPainter& valueDocumentPainter,
        folia::platform::MathJaxRenderer& valueMathJax,
        folia::platform::MermaidRenderer& valueMermaid,
        folia::platform::SvgNormalizer& valueSvgNormalizer,
        TreeSitterHighlighter& valueTreeSitter,
        folia::TextPosition valueCaret,
        float valueDocumentWidth,
        bool valueMathSvgSupported,
        bool valueMermaidSvgSupported,
        bool valueDarkTheme,
        std::uint64_t valueEmbeddedGeneration,
        std::uint64_t valueRemoteImageGeneration)
        : frame(valueFrame),
          resources(valueResources),
          renderCache(valueRenderCache),
          styleSheet(valueStyleSheet),
          textLayoutEngine(valueTextLayoutEngine),
          inlineImages(valueInlineImages),
          documentPainter(valueDocumentPainter),
          mathJax(valueMathJax),
          mermaid(valueMermaid),
          svgNormalizer(valueSvgNormalizer),
          treeSitter(valueTreeSitter),
          caret(valueCaret),
          documentWidth(valueDocumentWidth),
          mathSvgSupported(valueMathSvgSupported),
          mermaidSvgSupported(valueMermaidSvgSupported),
          darkTheme(valueDarkTheme),
          embeddedGeneration(valueEmbeddedGeneration),
          remoteImageGeneration(valueRemoteImageGeneration)
    {
    }

    void EditorDocumentBlockPreparer::InitializeMetadata(
        EditorPreparedDocument::Block& prepared,
        folia::RenderBlock const& block) const
    {
        prepared.sourceId = block.id;
        prepared.presentationKey = block.presentation_key;
        prepared.sourceMode = block.source_mode;
        prepared.code = block.kind == folia::RenderBlockKind::Code
            || block.kind == folia::RenderBlockKind::Frontmatter
            || block.kind == folia::RenderBlockKind::Unsupported;
    }

    void EditorDocumentBlockPreparer::InitializeContentMetadata(
        EditorPreparedDocument::Block& prepared,
        folia::RenderBlock const& block) const
    {
        prepared.containsMath = RenderBlockContainsMath(block);
        prepared.containsImage = RenderBlockContainsImage(block);
        prepared.containsMermaid = RenderBlockContainsMermaid(block);
        std::unordered_set<std::uint64_t> owners;
        CollectRenderOwners(block, owners, prepared.owners);
    }

    DisplayInlineText EditorDocumentBlockPreparer::BuildDisplay(
        folia::RenderBlock const& block,
        float width,
        bool requestEmbedded,
        bool highPriority)
    {
        if (block.kind == folia::RenderBlockKind::Blank)
        {
            DisplayInlineText display;
            AppendGeneratedText(
                display,
                U"\u200B",
                {block.id, 0, folia::TextAffinity::Downstream},
                folia::InlineStyle::plain());
            display.displayToSource.push_back({block.id, 0, folia::TextAffinity::Downstream});
            return display;
        }
        if (block.kind == folia::RenderBlockKind::Code)
            return BuildCodeBlockText(block, caret, treeSitter);
        if (block.kind == folia::RenderBlockKind::Math)
            return BuildMathBlockText(
                block,
                caret,
                mathJax,
                svgNormalizer,
                styleSheet.textColor,
                styleSheet.body.size,
                width,
                mathSvgSupported,
                requestEmbedded,
                highPriority);
        if (!block.inline_items.empty())
        {
            auto sourceEnd = InlineItemsEndPosition(
                block.inline_items,
                {block.id, block.content_span.source_range.end, folia::TextAffinity::Downstream});
            return BuildDisplayInlineText(
                block.inline_items,
                caret,
                sourceEnd,
                mathJax,
                svgNormalizer,
                styleSheet.textColor,
                styleSheet.body.size,
                width,
                mathSvgSupported,
                requestEmbedded,
                highPriority);
        }

        DisplayInlineText display;
        if (block.kind == folia::RenderBlockKind::Image)
        {
            auto const& special = block.special();
            auto position = folia::TextPosition{block.id, 0, folia::TextAffinity::Downstream};
            auto start = static_cast<std::uint32_t>(display.displayToSource.size());
            AppendGeneratedText(display, U"\uFFFC", position, folia::InlineStyle::plain());
            display.imageOverlays.push_back({
                start,
                block.source_span,
                special.src,
                special.alt,
                special.image_width,
                special.image_height,
                true,
            });
        }
        else if (block.kind == folia::RenderBlockKind::Toc)
        {
            for (auto const* item : frame.renderModel.outline.flat_items())
            {
                AppendGeneratedText(
                    display,
                    folia::utf8_to_cps(item->title_plain_text) + U"\n",
                    {block.id, 0, folia::TextAffinity::Downstream},
                    folia::InlineStyle{.link = true});
            }
        }
        else
        {
            auto const& special = block.special();
            auto raw = folia::utf8_to_cps(
                special.raw.empty() ? special.reason_text : special.raw);
            AppendSourceText(
                display,
                raw,
                {block.id, {0, raw.size()}},
                folia::InlineStyle::plain(),
                false);
        }
        if (display.text.empty())
            AppendGeneratedText(
                display,
                U"\u200B",
                {block.id, 0, folia::TextAffinity::Downstream},
                folia::InlineStyle::plain());
        display.displayToSource.push_back({
            block.id,
            block.source_span.source_range.end,
            folia::TextAffinity::Downstream,
        });
        return display;
    }

    void EditorDocumentBlockPreparer::ApplyMermaidPresentation(
        DisplayInlineText& display,
        folia::RenderBlock const& block,
        bool requestEmbedded)
    {
        if (!mermaidSvgSupported) return;
        struct Replacement
        {
            std::uint32_t start = 0;
            std::uint32_t end = 0;
            folia::platform::MermaidSvg svg;
            folia::TextSpan sourceSpan;
        };
        std::vector<Replacement> replacements;
        auto collect = [&](auto& self, folia::RenderBlock const& candidate) -> void
        {
            if (!candidate.source_mode
                && candidate.kind == folia::RenderBlockKind::Code
                && IsMermaidLanguage(candidate.special().language)
                && !candidate.special().code_text.empty())
            {
                auto rendered = mermaid.GetOrQueue(
                    folia::cps_to_utf8(candidate.special().code_text),
                    darkTheme,
                    requestEmbedded);
                if (!rendered)
                {
                    display.pendingMermaid =
                        display.pendingMermaid || requestEmbedded;
                }
                else if (static_cast<bool>(*rendered))
                {
                    if (caret.container_id == candidate.id)
                    {
                        display.mermaidPreviews.push_back({
                            std::move(*rendered),
                        });
                    }
                    else if (auto range = SourceDisplayRangeForContainer(
                                 display.displayToSource,
                                 candidate.id,
                                 folia::utf16_len(display.text)))
                    {
                        replacements.push_back({
                            static_cast<std::uint32_t>(range->first),
                            static_cast<std::uint32_t>(range->second),
                            std::move(*rendered),
                            candidate.source_span,
                        });
                    }
                }
            }
            for (auto const& child : candidate.child_blocks) self(self, child);
        };
        collect(collect, block);
        std::ranges::sort(
            replacements,
            std::greater<>{},
            &Replacement::start);
        for (auto& replacement : replacements)
        {
            ReplaceDisplayRangeWithMermaid(
                display,
                replacement.start,
                replacement.end,
                std::move(replacement.svg),
                replacement.sourceSpan);
        }
        std::ranges::sort(
            display.mermaidOverlays,
            {},
            &DisplayInlineText::MermaidOverlay::displayStart);
    }

    EditorPreparedDocument::Block EditorDocumentBlockPreparer::Prepare(
        folia::RenderBlock const& block,
        bool requestEmbedded,
        bool highPriority,
        std::optional<EditorFlowLayoutContext> flowContext)
    {
        mathJax.BeginBlock(block.id.v);
        EditorPreparedDocument::Block prepared;
        InitializeMetadata(prepared, block);
        InitializeContentMetadata(prepared, block);
        auto& visual = prepared.EnsureVisuals();
        visual.caretPresentation = CaretPresentationState(block, caret);
        prepared.embeddedRequested = requestEmbedded;
        prepared.embeddedGeneration = embeddedGeneration;
        prepared.remoteImageGeneration = remoteImageGeneration;
        std::unordered_set<std::uint64_t> owners;
        for (auto owner : prepared.owners) owners.insert(owner.v);
        auto addOwner = [&](folia::NodeId id)
        {
            if (id.v != 0 && owners.insert(id.v).second) prepared.owners.push_back(id);
        };
        addOwner(block.id);
        addOwner(block.source_span.container_id);
        if (block.kind == folia::RenderBlockKind::Table)
        {
            visual.table = EditorTableBlockRenderer::Prepare(
                block,
                caret,
                documentWidth,
                mathSvgSupported,
                requestEmbedded,
                highPriority,
                resources,
                styleSheet,
                textLayoutEngine,
                inlineImages,
                mathJax,
                svgNormalizer,
                treeSitter,
                frame.completion);
            if (visual.table)
            {
                for (auto const& span : block.special().table_cell_spans)
                    addOwner(span.container_id);
                prepared.pendingImage = visual.table->pendingImage;
                prepared.pendingMath = visual.table->pendingMath;
                prepared.height = visual.table->height;
                prepared.geometryMeasured = true;
                prepared.valid = true;
                mathJax.EndBlock();
                return prepared;
            }
        }
        auto flowContainer = !block.inline_items.empty()
            && (block.kind == folia::RenderBlockKind::Quote
                || block.kind == folia::RenderBlockKind::Callout);
        auto paddingTop = flowContainer ? 0.0f : block.block_style.padding_top;
        auto paddingBottom = flowContainer ? 0.0f : block.block_style.padding_bottom;
        auto contentWidth = EditorBlockContentWidth(block, documentWidth);
        visual.textTop = paddingTop;
        visual.display = BuildDisplay(
            block,
            contentWidth,
            requestEmbedded,
            highPriority);
        HighlightMathSources(visual.display, caret, treeSitter);
        ApplyMermaidPresentation(
            visual.display,
            block,
            requestEmbedded);
        if (block.source_mode
            && block.source_code
            && block.special().language
            && !visual.display.text.empty())
        {
            auto const& language = *block.special().language;
            if (!block.source_code_context
                || sourceCodeContext != block.source_code_context
                || sourceCodeLanguage != language)
            {
                // Adjacent source lines share their complete lexical context,
                // but a whole-document preparation must not retain every code
                // block's tokens. Keep an owning key to avoid pointer reuse and
                // release the previous range storage before parsing the next.
                sourceCodeContext.reset();
                sourceCodeHighlights = std::vector<SyntaxHighlightRange>{};
                auto const& context = block.source_code_context
                    ? *block.source_code_context
                    : visual.display.text;
                sourceCodeHighlights = treeSitter.Highlight(language, folia::cps_to_utf8(context));
                sourceCodeLanguage = language;
                sourceCodeContext = block.source_code_context;
            }
            auto lineStart = block.source_code_context ? block.source_code_context_offset : 0;
            auto lineEnd = lineStart + visual.display.text.size();
            for (auto const& highlight : sourceCodeHighlights)
            {
                auto highlightStart = static_cast<std::size_t>(highlight.start);
                auto highlightEnd = highlightStart + static_cast<std::size_t>(highlight.length);
                if (highlightEnd <= lineStart || highlightStart >= lineEnd) continue;
                auto start = (std::max)(highlightStart, lineStart) - lineStart;
                auto end = (std::min)(highlightEnd, lineEnd) - lineStart;
                auto displayStart = folia::char_index_to_utf16(visual.display.text, start);
                auto displayEnd = folia::char_index_to_utf16(visual.display.text, end);
                if (displayEnd <= displayStart) continue;
                visual.display.ranges.push_back({
                    static_cast<UINT32>(displayStart),
                    static_cast<UINT32>(displayEnd - displayStart),
                    folia::InlineStyle::plain(),
                    false,
                    highlight.kind,
                });
            }
        }
        documentPainter.ApplyNestedCodeHighlights(visual.display, block);
        if (frame.completion) InsertCompletionPreview(visual.display, frame.completion->position, frame.completion->text, frame.completion->before);
        prepared.pendingMermaid = visual.display.pendingMermaid;
        prepared.pendingMath = visual.display.pendingMath;
        prepared.hasFlowContext = true;
        prepared.intrinsicFlow.format = textLayoutEngine.FormatKindFor(
            prepared.code || prepared.sourceMode, visual.display.ranges);
        prepared.appliedFlow = flowContext.value_or(prepared.intrinsicFlow);
        auto format = textLayoutEngine.FormatFor(prepared.appliedFlow.format);
        visual.layout = textLayoutEngine.CreateFlow(
            visual.display,
            format,
            contentWidth,
            [&](IDWriteTextLayout* candidate, DisplayInlineText const& candidateDisplay)
            {
                if (!candidate) return;
                visual.images = inlineImages.Resolve(
                    candidate,
                    candidateDisplay.imageOverlays,
                    contentWidth,
                    requestEmbedded);
                prepared.pendingImage = std::ranges::any_of(
                    visual.images,
                    [](auto const& image) { return image.pending; });
                visual.mermaids.clear();
                if (!candidateDisplay.mermaidOverlays.empty())
                    candidate->SetLineSpacing(
                        DWRITE_LINE_SPACING_METHOD_DEFAULT,
                        0.0f,
                        0.0f);
                visual.mermaids.reserve(
                    candidateDisplay.mermaidOverlays.size());
                for (auto const& overlay : candidateDisplay.mermaidOverlays)
                {
                    float pointX = 0.0f;
                    float ignoredY = 0.0f;
                    DWRITE_HIT_TEST_METRICS hit{};
                    if (FAILED(candidate->HitTestTextPosition(
                            overlay.displayStart,
                            FALSE,
                            &pointX,
                            &ignoredY,
                            &hit)))
                        continue;
                    auto available = (std::max)(
                        1.0f,
                        contentWidth - pointX);
                    auto scale = (std::min)(
                        1.0f,
                        available / overlay.svg.width);
                    auto width = (std::max)(
                        1.0f,
                        overlay.svg.width * scale);
                    auto height = (std::max)(
                        1.0f,
                        overlay.svg.height * scale);
                    auto lineHeight = (std::max)(
                        styleSheet.body.lineHeight,
                        height);
                    auto advance = (std::max)(width, available);
                    ApplyInlinePlaceholder(
                        candidate,
                        overlay.displayStart,
                        advance,
                        lineHeight,
                        lineHeight);
                    visual.mermaids.push_back({
                        overlay.displayStart,
                        overlay.svg,
                        width,
                        height,
                        lineHeight,
                        advance,
                    });
                }
                DWRITE_LINE_SPACING_METHOD method{};
                FLOAT spacing = 0.0f, baseline = 0.0f;
                if (SUCCEEDED(candidate->GetLineSpacing(&method, &spacing, &baseline)))
                    prepared.intrinsicFlow.naturalLineSpacing =
                        prepared.intrinsicFlow.naturalLineSpacing
                        || method == DWRITE_LINE_SPACING_METHOD_DEFAULT;
                if (flowContext && flowContext->naturalLineSpacing)
                    candidate->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, 0.0f);
            });
        prepared.appliedFlow.naturalLineSpacing = prepared.intrinsicFlow.naturalLineSpacing
            || (flowContext && flowContext->naturalLineSpacing);
        if (visual.layout && !block.source_mode && block.block_style.text_alignment)
        {
            auto alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
            switch (*block.block_style.text_alignment)
            {
                case folia::TextAlignment::Start:
                    alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
                    break;
                case folia::TextAlignment::Center:
                    alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
                    break;
                case folia::TextAlignment::End:
                    alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
                    break;
                case folia::TextAlignment::Justify:
                    alignment = DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
                    break;
            }
            visual.layout->SetTextAlignment(alignment);
        }
        visual.mathPreviews.reserve(visual.display.mathPreviews.size());
        for (auto const& preview : visual.display.mathPreviews)
        {
            visual.mathPreviews.push_back(PrepareMathPreview(preview, textLayoutEngine,
                resources.textFormat.Get(), (std::max)(1.0f, contentWidth - 16.0f),
                resources.surfaceWidthDip, styleSheet.mathPreview));
        }
        visual.mermaidPreviews.reserve(
            visual.display.mermaidPreviews.size());
        for (auto const& preview : visual.display.mermaidPreviews)
        {
            auto available = (std::max)(1.0f, contentWidth - 16.0f);
            auto scale = (std::min)(
                1.0f,
                available / (std::max)(1.0f, preview.svg.width));
            visual.mermaidPreviews.push_back({
                preview.svg,
                (std::max)(1.0f, preview.svg.width * scale),
                (std::max)(1.0f, preview.svg.height * scale),
            });
        }
        visual.textHeight = textLayoutEngine.MeasureHeight(
            visual.layout.Get(),
            textLayoutEngine.LineHeightFor(prepared.appliedFlow.format),
            !prepared.appliedFlow.fragmented);
        visual.layoutLines = folia::platform::editor::BuildEditorTextLines(
            visual.layout.Get(), visual.display.displayToSource, block.source_span, contentWidth);
        visual.paintPlan = folia::platform::editor::EditorTextPaintPlan::Prepare(visual.layout.Get());
        auto previewHeight = 0.0f;
        for (auto const& preview : visual.mathPreviews)
            if (!preview.floating) previewHeight += preview.Height() + 24.0f;
        for (auto const& preview : visual.mermaidPreviews)
            previewHeight += preview.height + 24.0f;
        prepared.previewHeight = previewHeight;
        prepared.height = visual.textHeight + previewHeight + paddingTop + paddingBottom;
        prepared.geometryMeasured = true;
        visual.display.displayToSource.for_each_run(0, visual.display.displayToSource.size(),
            [&](auto, auto, auto first, auto) { addOwner(first.container_id); return true; });
        prepared.valid = true;
        mathJax.EndBlock();
        return prepared;
    }

    std::vector<std::string> EditorDocumentBlockPreparer::ImageSources(
        EditorPreparedDocument::Block const& prepared)
    {
        std::vector<std::string> sources;
        auto const& visual = prepared.Visuals();
        sources.reserve(visual.images.size());
        for (auto const& image : visual.images)
            if (!image.source.empty()) sources.push_back(image.source);
        if (visual.table)
        {
            for (auto const& cellImages : visual.table->imageDraws)
                for (auto const& image : cellImages)
                    if (!image.source.empty()) sources.push_back(image.source);
        }
        return sources;
    }
}
