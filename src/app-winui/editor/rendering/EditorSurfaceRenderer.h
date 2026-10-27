#pragma once

import folia.platform.editor_interaction;
import folia.platform.editor_scroll_state;

#include "editor/session/EditorRenderFrame.h"
#include "media/MathJaxRenderer.h"
#include "media/MermaidRenderer.h"
#include "media/SvgNormalizer.h"
#include "media/TreeSitterHighlighter.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/EditorMathPreparation.h"
#include "editor/rendering/EditorRenderCache.h"
#include "editor/rendering/EditorRenderResources.h"
#include "editor/rendering/EditorCaretAnimation.h"
#include "export/EditorPdfExportTypes.h"

namespace winrt::Folia
{
    using folia::platform::editor::EditorInteractionMap;
    using folia::platform::editor::EditorScrollState;
    using folia::platform::editor::EditorFootnoteControlKind;
    using folia::platform::editor::EditorTableAction;
    using folia::platform::editor::EditorVisualBlock;
    using folia::platform::editor::EditorVisualFootnoteHit;
    using folia::platform::editor::EditorVisualLine;
    using folia::platform::editor::EditorVisualMathHit;
    using folia::platform::editor::EditorVisualRawBlock;
    using folia::platform::editor::EditorVisualTable;
    using folia::platform::editor::EditorVisualTableCell;

    struct EditorPreparedDocument;
    enum class EditorScrollAlignment { Nearest, Start };

    struct EditorSurfaceRenderer
    {
        EditorSurfaceRenderer();
        ~EditorSurfaceRenderer();

        void Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel);
        void InitializeForPdf();
        void Resize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel, double width, double height);
        void Render(detail::EditorRenderFrame const& frame);
        EditorPdfExportProgress ExportPdfStep(
            std::filesystem::path const& path,
            std::wstring const& title,
            detail::EditorRenderFrame const& frame);
        void CancelPdfExport();
        void SetTheme(folia::ThemeProfile const& value);
        folia::ThemeProfile Theme() const;
        EditorMathPreparationContext CaptureMathPreparationContext() const;
        bool MatchesMathPreparationContext(EditorMathPreparationContext const& context) const;
        static std::unique_ptr<EditorMathPreparation> PrepareMath(
            detail::EditorRenderFrame const& frame,
            EditorMathPreparationContext const& context,
            std::stop_token stop,
            std::function<void(std::size_t, std::size_t)> progress);
        void AdoptMathPreparation(std::unique_ptr<EditorMathPreparation> preparation);
        void SetMathPreparationStopToken(std::stop_token stop);
        void ResetDocumentCaches();
        MathJaxRenderer TakeMathCache();
        void RestoreMathCache(MathJaxRenderer cache);
        void SetInvalidateCallback(std::function<void()> callback);

        std::optional<D2D1_RECT_F> InlineMathPreviewBounds() const noexcept { return inlineMathPreviewBounds; }
        std::optional<folia::TextPosition> HitTest(float x, float y) const;
        std::optional<folia::TextPosition> ViewportPosition() const;
        std::optional<folia::TextPosition> TaskCheckboxAt(float x, float y) const;
        std::optional<EditorVisualFootnoteHit> FootnoteAt(float x, float y) const;
        std::optional<EditorVisualRawBlock> RawBlockFor(folia::NodeId owner) const;
        std::optional<EditorVisualRawBlock> CodeLanguageAt(float x, float y) const;
        std::optional<D2D1_RECT_F> CaretBounds(folia::TextPosition position) const;
        std::optional<folia::TextPosition> MoveCaretVertically(folia::TextPosition position, bool down, float& goalX) const;
        std::optional<folia::TextPosition> VisualLineStart(folia::TextPosition position) const;
        std::optional<folia::TextPosition> VisualLineEnd(folia::TextPosition position) const;
        void UpdatePointer(float x, float y);
        void ClearPointer();
        std::optional<EditorTableAction> TableActionAt(float x, float y) const;
        std::optional<std::size_t> TableDropIndexAt(float x, float y, bool rows) const;
        void SetTableDrag(std::optional<EditorTableAction> action, std::optional<std::size_t> dropIndex);
        void ScrollBy(float delta);
        void QueueScrollBy(float delta);
        bool AdvanceScrollAnimation(float elapsedSeconds);
        void SetScrollOffset(float value);
        float ScrollOffset() const;
        float MaximumScrollOffset() const;
        float ViewportHeight() const;
        HANDLE FrameLatencyWaitableObject() const;
        void FocusCaret(bool focused) { caretAnimation.Focus(focused); }
        void ConfigureCaret(bool animations, double blinkHalfPeriod) { caretAnimation.Configure(styleSheet.editor, animations, blinkHalfPeriod); }
        EditorCaretFrame const& CaretAnimationFrame() const { return caretAnimation.Frame(); }
        bool ScrollToPosition(folia::TextPosition position, EditorScrollAlignment alignment = EditorScrollAlignment::Nearest);

    private:
        struct PdfExportState;

        void DrawDocument(
            detail::EditorRenderFrame const& frame,
            std::optional<float> sourceTopOverride = std::nullopt);
        void ClearPreparedDocument();
        void AttachMathCompletion();
        std::function<void()> EmbeddedCompletionCallback();

        void Invalidate();

        struct InvalidationState
        {
            std::mutex mutex;
            std::function<void()> callback;
            bool active = true;
            bool repaintQueued = false;
            bool embeddedQueued = false;
        };

        using VisualBlock = EditorVisualBlock;
        using VisualLine = EditorVisualLine;
        using VisualTableCell = EditorVisualTableCell;
        using VisualTable = EditorVisualTable;
        using VisualMathHit = EditorVisualMathHit;

        winrt::Microsoft::UI::Dispatching::DispatcherQueue mathDispatcher{nullptr};
        EditorRenderResources resources;
        EditorRenderCache renderCache;
        MathJaxRenderer mathJax;
        MermaidRenderer mermaid;
        SvgNormalizer svgNormalizer;
        TreeSitterHighlighter treeSitter;
        std::shared_ptr<InvalidationState> invalidationState = std::make_shared<InvalidationState>();
        folia::ThemeProfile themeProfile = folia::default_theme_profile();
        std::uint64_t themeRevision = 1;
        EditorStyleSheet styleSheet = CreateEditorStyleSheet(themeProfile);
        EditorInteractionMap interactionMap;
        EditorCaretAnimation caretAnimation;
        std::vector<D2D1_RECT_F> nonInteractiveRegions;
        std::optional<D2D1_RECT_F> inlineMathPreviewBounds;
        std::unique_ptr<EditorPreparedDocument> preparedDocument;
        std::shared_ptr<PdfExportState> pdfExportState;
        std::uint64_t embeddedGeneration = 0;
        std::optional<std::uint64_t> mathModelRevision;
        std::optional<D2D1_POINT_2F> pointerPosition;
        std::optional<EditorTableAction> draggedTableAction;
        std::optional<std::size_t> tableDropIndex;
        EditorScrollState scrollState;
        float totalDocumentHeight = 0.0f;
        bool resizing = false;
        bool rendering = false;
        bool printMode = false;
        std::stop_token mathPreparationStop;
        bool exporting = false;
        std::atomic_bool deferredInvalidate = false;
    };
}
