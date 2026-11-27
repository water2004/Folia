#pragma once

import folia.highlight;
import folia.platform.editor_interaction;
import folia.platform.editor_scroll_state;

#include "editor/session/EditorRenderFrame.h"
#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/mermaid/MermaidRenderer.h"
#include "windows/media/svg/SvgNormalizer.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/media/EditorMathPreparation.h"
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

    using folia::highlight::TreeSitterHighlighter;

    struct EditorPreparedDocument;
    enum class EditorScrollAlignment { Nearest, Start };

    struct EditorSurfaceRenderer
    {
        EditorSurfaceRenderer();
        ~EditorSurfaceRenderer();

        void Initialize(winrt::Microsoft::UI::Xaml::FrameworkElement const& host);
        void InitializeForPdf();
        void Resize(winrt::Microsoft::UI::Xaml::FrameworkElement const& host, double width, double height);
        void Render(detail::EditorRenderFrame const& frame);
        // A document transition may draw several layouts to restore its
        // viewport. Submit its final buffer after the shell frame has rendered.
        class FrameUpdate
        {
        public:
            ~FrameUpdate();
            FrameUpdate(FrameUpdate const&) = delete;
            FrameUpdate& operator=(FrameUpdate const&) = delete;
        private:
            friend struct EditorSurfaceRenderer;
            explicit FrameUpdate(EditorSurfaceRenderer& renderer);
            EditorSurfaceRenderer& renderer_;
        };
        [[nodiscard]] FrameUpdate UpdateWithXaml();
        EditorPdfExportProgress ExportPdfStep(
            std::filesystem::path const& path,
            std::wstring const& title,
            detail::EditorRenderFrame const& frame);
        void CancelPdfExport();
        void SetTheme(folia::ThemeProfile const& value);
        bool SetTextLayout(TextLayoutOptions const& value);
        folia::ThemeProfile Theme() const;
        void SetMediaDisplaySettings(MediaDisplaySettings const& value);
        MediaDisplaySettings const& MediaSettings() const noexcept { return mediaSettings; }
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
        folia::platform::MathJaxRenderer TakeMathCache();
        void RestoreMathCache(folia::platform::MathJaxRenderer cache);
        void SetInvalidateCallback(std::function<void()> callback);

        std::optional<D2D1_RECT_F> InlineMathPreviewBounds() const noexcept { return inlineMathPreviewBounds; }
        std::optional<folia::TextPosition> HitTest(float x, float y) const;
        std::optional<folia::TextPosition> HitTestTableText(float x, float y, folia::NodeId owner) const;
        std::optional<folia::TextPosition> HitTestTextUnit(float x, float y,
            folia::platform::editor::TextHitMode mode = folia::platform::editor::TextHitMode::CharacterStart) const;
        std::optional<folia::TextPosition> ViewportPosition() const;
        std::optional<folia::TextPosition> TaskCheckboxAt(float x, float y) const;
        std::optional<EditorVisualFootnoteHit> FootnoteAt(float x, float y) const;
        std::optional<EditorVisualRawBlock> RawBlockFor(folia::NodeId owner) const;
        std::optional<EditorVisualRawBlock> CodeLanguageAt(float x, float y) const;
        std::optional<D2D1_RECT_F> CaretBounds(folia::TextPosition position) const;
        std::optional<folia::TextPosition> MoveCaretVertically(folia::TextPosition position, bool down, float& goalX) const;
        std::optional<folia::TextPosition> MoveCaretByPage(folia::TextPosition position, bool down, float& goalX) const;
        std::optional<folia::TextPosition> VisualLineStart(folia::TextPosition position) const;
        std::optional<folia::TextPosition> VisualLineEnd(folia::TextPosition position) const;
        void UpdatePointer(float x, float y);
        void ClearPointer();
        folia::platform::editor::EditorTableControls TableControlsAt(float x, float y) const;
        std::optional<folia::TextSelection> TableAxisSelection(EditorTableAction const&) const;
        std::optional<folia::TableFormatEdit> TableResizeFormat(EditorTableAction const&, float width) const;
        std::optional<folia::TextSpan> TableCellAt(float x, float y) const;
        std::optional<folia::TableCoordinate> TableFillTargetAt(folia::NodeId table, float x, float y) const;
        std::optional<std::pair<folia::NodeId, float>> ScrollTableAt(float x, float y, float delta) const;
        std::optional<float> RevealTableColumn(const folia::TableCellAddress& address) const;
        std::optional<std::size_t> TableDropIndexAt(float x, float y, bool rows) const;
        void SetTableDrag(std::optional<EditorTableAction> action, std::optional<std::size_t> dropIndex);
        void ScrollBy(float delta);
        void QueueScrollBy(float delta);
        bool AdvanceScrollAnimation(float elapsedSeconds);
        void SetScrollOffset(float value);
        float ScrollOffset() const;
        float MaximumScrollOffset() const;
        float DocumentHeight() const noexcept { return totalDocumentHeight; }
        float ViewportHeight() const;
        float ViewportWidth() const;
        void FocusCaret(bool focused) {
            caretAnimation.Focus(focused);
            selectionAnimation.Focus(focused);
            if (!focused) tableSelectionAnimation.Reset();
        }
        void ConfigureCaret(bool animations, double blinkHalfPeriod) {
            caretAnimation.Configure(styleSheet.editor, animations, blinkHalfPeriod);
            selectionAnimation.Configure(animations ? styleSheet.editor.caret_move_ms / 1000.0 : 0.0);
            tableSelectionAnimation.Configure(animations
                ? (std::clamp)(static_cast<double>(styleSheet.editor.caret_move_ms) / 1000.0, 0.0, 0.080) : 0.0);
        }
        EditorCaretFrame const& CaretAnimationFrame() const { return caretAnimation.Frame(); }
        bool SelectionAnimating() const { return selectionAnimation.Animating() || tableSelectionAnimation.Animating(); }
        bool ScrollToPosition(folia::TextPosition position, EditorScrollAlignment alignment = EditorScrollAlignment::Nearest);
        std::size_t TableTextLayoutsBuilt() const { return resources.tableTextLayoutsBuilt; }
        bool SelectionBackgroundAt(float x, float y) const { return selectionAnimation.HitTest(x, y); }
        bool SetSelectionDrop(std::optional<folia::TextPosition> position) {
            if (selectionDrop == position) return false;
            selectionDrop = position;
            return true;
        }

    private:
        struct PdfExportState;
        friend struct EditorRendererTestAccess;

        void DrawDocument(
            detail::EditorRenderFrame const& frame,
            std::optional<float> sourceTopOverride = std::nullopt);
        void ClearPreparedDocument();
        void AttachMathCompletion();
        std::function<void()> EmbeddedCompletionCallback();

        void Invalidate();
        void RefreshStyleSheet();
        void PresentPendingFrame();

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
        folia::platform::MathJaxRenderer mathJax;
        folia::platform::MermaidRenderer mermaid;
        folia::platform::SvgNormalizer svgNormalizer;
        TreeSitterHighlighter treeSitter;
        std::shared_ptr<InvalidationState> invalidationState = std::make_shared<InvalidationState>();
        folia::ThemeProfile themeProfile = folia::default_theme_profile();
        TextLayoutOptions textLayout;
        std::uint64_t themeRevision = 1;
        EditorStyleSheet styleSheet = CreateEditorStyleSheet(themeProfile);
        MediaDisplaySettings mediaSettings;
        EditorInteractionMap interactionMap;
        EditorCaretAnimation caretAnimation;
        EditorSelectionAnimation selectionAnimation;
        EditorTableSelectionAnimation tableSelectionAnimation;
        void ResetTextMotion() {
            caretAnimation.Reset();
            selectionAnimation.Reset();
            tableSelectionAnimation.Reset();
        }
        std::vector<D2D1_RECT_F> nonInteractiveRegions;
        std::optional<D2D1_RECT_F> inlineMathPreviewBounds;
        std::unique_ptr<EditorPreparedDocument> preparedDocument;
        std::shared_ptr<PdfExportState> pdfExportState;
        std::uint64_t embeddedGeneration = 0;
        std::optional<std::uint64_t> mathModelRevision;
        std::optional<D2D1_POINT_2F> pointerPosition;
        std::optional<folia::TextPosition> selectionDrop;
        std::optional<EditorTableAction> draggedTableAction;
        std::optional<std::size_t> tableDropIndex;
        EditorScrollState scrollState;
        float totalDocumentHeight = 0.0f;
        bool resizing = false;
        bool rendering = false;
        std::uint32_t frameUpdateDepth = 0;
        bool presentPending = false;
        winrt::event_token xamlPresentation{};
        bool printMode = false;
        std::stop_token mathPreparationStop;
        bool exporting = false;
        std::atomic_bool deferredInvalidate = false;
    };
}
