#pragma once
#include "editor/session/EditorSourceLocation.h"

#include "editor/session/EditorRenderFrame.h"
import folia.encoding;

import folia.core.editor;
import folia.markdown.parser;
import folia.core.command;
import folia.core.render_model;
import folia.core.search;
import folia.core.latex_completion;
import folia.core.source_editor;
import folia.core.theme;
import folia.platform.editor_shortcuts;
import folia.math.editor.syntax;
import folia.completion;

namespace winrt::Folia
{
    struct EditorDocumentInteraction
    {
        std::optional<std::string> link;
        std::optional<std::string> tooltip;
    };

    struct EditorSearchSummary
    {
        std::size_t matchCount = 0;
        std::optional<std::string> error;
    };

    struct EditorLatexCompletionSource
    {
        folia::NodeId container;
        std::u32string_view source;
        std::size_t caret = 0;
    };

    struct EditorReloadViewState
    {
        std::size_t anchor = 0;
        std::size_t active = 0;
        folia::TextAffinity anchorAffinity = folia::TextAffinity::Downstream;
        folia::TextAffinity activeAffinity = folia::TextAffinity::Downstream;
        bool sourceMode = false;
    };

    namespace detail
    {
        struct EditorSessionCore
        {
            struct SearchTarget
            {
                std::vector<folia::TextSpan> spans;
                std::optional<folia::SourceRange> sourceRange;
            };

            folia::Editor editor;
            folia::RenderModel renderModel;
            folia::ThemeProfile theme = folia::default_theme_profile();
            std::wstring baseDirectory;
            std::unordered_map<std::uint64_t, float> tableScrollOffsets;
            std::size_t characterCount = 0;
            std::vector<SearchTarget> searchTargets;
            std::vector<EditorSearchHighlight> searchHighlights;
            std::optional<std::size_t> activeSearchMatch;
            std::vector<folia::RenderedSearchFragment> renderedSearchFragments;
            std::uint64_t renderedSearchRevision = (std::numeric_limits<std::uint64_t>::max)();
        };
    }

    struct EditorSession
    {
        using LoadProgress = folia::ParseProgressCallback;

        EditorSession();
        ~EditorSession();
        EditorSession(EditorSession const&) = delete;
        EditorSession& operator=(EditorSession const&) = delete;
        EditorSession(EditorSession&&) noexcept;
        EditorSession& operator=(EditorSession&&) noexcept;

        void Open(
            winrt::Windows::Storage::StorageFile const& file,
            winrt::hstring const& text,
            folia::encoding::DocumentEncoding encoding,
            std::vector<folia::encoding::DocumentEncodingCandidate> encodingCandidates,
            LoadProgress progress = {},
            bool encodingExplicit = false);
        void SaveAs(winrt::Windows::Storage::StorageFile const& file);
        void SetText(winrt::hstring const& text);
        folia::MarkdownStream BeginMarkdownStream() const;
        void AppendMarkdown(folia::MarkdownStream&, std::string_view);
        std::string SelectedDisplayText() const;
        void SetTheme(folia::ThemeProfile const& theme);
        bool IsSourceMode() const;
        bool EnterSourceMode();
        bool ExitSourceMode();
        bool ToggleSourceMode();
        bool ExecuteCommand(folia::Command const& command);
        void SetSelection(folia::TextPosition anchor, folia::TextPosition active);
        void SetSelection(folia::TextSelection selection);
        bool HasSelection() const;
        bool SelectionCovers(folia::TextPosition position) const;
        std::u32string SelectedSource() const;
        std::string SelectedTextUtf8() const;
        bool CanCopyTable() const;
        bool SelectionInSameTable(folia::TextPosition position) const;
        std::optional<folia::TextPosition> TableEnterTarget() const;
        std::optional<folia::NodeId> TableTextOwner(folia::TextPosition position) const;
        std::optional<folia::NodeId> TableTextOwner(folia::TextSelection selection) const;
        std::optional<folia::TextSelection> TableTextContents(folia::TextPosition position) const;
        std::optional<folia::TextSelection> TableEscapeSelection() const;
        folia::TextSelection TextInputSelection() const;
        std::optional<folia::TableCellDetails> TableCellDetails(folia::TextPosition position) const;
        std::optional<folia::TableCellAddress> TableAddress(folia::TextPosition position) const;
        bool TableSelectionIncludes(folia::TextPosition position) const;
        std::optional<folia::TableCellSelection> TableSelection() const;
        void SetTableScrollOffset(folia::NodeId table, float offset);
        std::optional<std::pair<std::u32string, std::u32string>> TableClipboard(bool selected = false, bool plain = false) const;
        bool HasFile() const;
        winrt::Windows::Storage::StorageFile File() const;
        winrt::hstring Text() const;
        std::string TextUtf8() const;
        folia::encoding::DocumentEncoding const& Encoding() const;
        std::span<folia::encoding::DocumentEncodingCandidate const> EncodingCandidates() const;
        void SetEncoding(folia::encoding::DocumentEncoding encoding);
        bool EncodingWasExplicit() const;
        EditorReloadViewState CaptureReloadViewState() const;
        void JumpToSourceLocation(EditorSourceLocation);
        void RestoreReloadViewState(EditorReloadViewState const& state);
        winrt::hstring DisplayName() const;
        winrt::hstring Path() const;
        uint64_t Revision() const;
        std::uint64_t ContentState() const noexcept;
        std::size_t CharacterCount() const;
        std::wstring TextInputTextUtf16(folia::NodeId containerId) const;
        std::size_t TextInputAcpOffset(folia::TextPosition position) const;
        folia::TextPosition TextInputPositionFromAcp(
            folia::NodeId containerId,
            std::size_t offset,
            folia::TextAffinity affinity = folia::TextAffinity::Downstream) const;
        std::optional<std::u32string> EditableSource(folia::NodeId id) const;
        std::optional<folia::TextSelection> WordSelectionAt(folia::TextPosition position) const;
        std::optional<folia::completion::Context> CaptureCompletionContext(folia::completion::Budget budget = {}) const;
        std::optional<EditorDocumentInteraction> InteractionAt(folia::TextPosition position) const;
        folia::TextSelection Selection() const;
        folia::InlineStyle ActiveInlineStyle() const;
        folia::platform::editor::EditorShortcutScope ShortcutScope() const;
        std::optional<EditorLatexCompletionSource> LatexCompletionSourceAtCaret() const;
        folia::RenderModel const& RenderModel() const;
        folia::RenderModel BuildPrintRenderModel() const;
        std::optional<folia::TextPosition> FootnoteDefinitionTarget(std::string_view label) const;
        std::optional<folia::TextPosition> FirstFootnoteReferenceTarget(std::string_view label) const;
        std::string FootnotePreview(std::string_view label) const;
        std::wstring const& BaseDirectory() const;
        EditorSearchSummary Search(
            std::u32string_view query,
            folia::SearchOptions options = {});
        bool ActivateSearchMatch(std::size_t index);
        bool ReplaceSearchMatch(
            std::size_t index,
            std::u32string_view query,
            std::u32string_view replacement,
            folia::SearchOptions options = {});
        bool ReplaceAllSearchMatches(
            std::u32string_view query,
            std::u32string_view replacement,
            folia::SearchOptions options = {});
        void ClearSearch();
        detail::EditorRenderFrame RenderFrame();

    private:
        void RebuildCore(LoadProgress progress = {});
        void RebuildRenderModel(folia::EditorDocumentChange const* change = nullptr);
        bool ShouldVirtualizeRenderModel() const;
        void MaterializeRenderBlocks(std::size_t begin, std::size_t end);
        void ReleaseRenderBlocksOutside(std::size_t begin, std::size_t end);
        void RefreshCharacterCount();
        bool TryMathBracketEdit(folia::Command const& command);
        std::optional<std::u32string_view> TextInputSourceView(folia::NodeId containerId) const;
        void RebuildSearchHighlights();
        bool CommitRenderedSearchReplacement(std::span<const folia::RenderedSearchMatch> matches);
        std::span<const folia::RenderedSearchFragment> RenderedSearchFragments();

        winrt::Windows::Storage::StorageFile file_{ nullptr };
        winrt::hstring text_;
        folia::encoding::DocumentEncoding encoding_;
        std::vector<folia::encoding::DocumentEncodingCandidate> encodingCandidates_;
        bool encodingExplicit_ = false;
        uint64_t revision_ = 0;
        std::unique_ptr<detail::EditorSessionCore> core_;
        std::unique_ptr<folia::math::editor::SyntaxSession> bracketSyntax_;
    };

}
