// folia.core.editor — owns the authoritative block tree, its single selection,
// reversible operation history, and the command pipeline.
export module folia.core.editor;
import std;
import folia.core.types;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.core.command;
import folia.core.input;
import folia.markdown.utf;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.core.document_edit;
import folia.core.document_history;
import folia.core.document_operation_apply;
import folia.core.document_search_replace;
import folia.core.document_index_update;
import folia.core.document_transaction;
import folia.markdown.document_symbols;
import folia.markdown.text_edit;
import folia.markdown.symbols;
import folia.markdown.outline;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.core.search;
import folia.core.source_editor;
import folia.core.source_patch;
import folia.core.source_projection;
import folia.core.source_patch_adapter;

export namespace folia {

struct EditorDocumentChange {
    std::vector<DocumentTextOperation> text_operations;
    std::vector<NodeId> structural_anchors;
    std::vector<NodeId> moved_roots;
    bool structural = false;
    bool structural_locality_known = true;
    bool structural_outline_may_change = false;
    bool forward = true;
    std::uint64_t revision_before = 0;
    std::uint64_t revision_after = 0;
};

inline EditorDocumentChange summarize_document_change(
    const std::vector<DocumentOperation>& operations,
    bool forward,
    std::uint64_t revision_before,
    std::uint64_t revision_after) {
    EditorDocumentChange change;
    change.forward = forward;
    change.revision_before = revision_before;
    change.revision_after = revision_after;
    auto subtree_has_heading = [&](auto& self, const BlockNode& block) -> bool {
        if (block.kind == BlockKind::Heading) return true;
        return std::ranges::any_of(block.children, [&](const auto& child) {
            return self(self, child);
        });
    };
    for (const auto& operation : operations) {
        if (const auto* text = std::get_if<DocumentTextOperation>(&operation)) {
            change.text_operations.push_back(*text);
        } else if (const auto* tree_operation=std::get_if<DocumentTreeEdit>(&operation)) {
            change.structural = true;
            const auto& tree = *tree_operation;
            if (tree.kind == DocumentTreeEditKind::Move) {
                change.structural_anchors.push_back(tree.parent_id);
                change.structural_anchors.push_back(tree.other_parent_id);
                change.structural_anchors.push_back(tree.moved_id);
                change.moved_roots.push_back(tree.moved_id);
            } else {
                if (tree.parent_id.v != 0) {
                    change.structural_anchors.push_back(tree.parent_id);
                }
                if (tree.before.id.v != 0) {
                    change.structural_anchors.push_back(tree.before.id);
                }
                if (tree.after.id.v != 0) {
                    change.structural_anchors.push_back(tree.after.id);
                }
                change.structural_outline_may_change =
                    change.structural_outline_may_change
                    || subtree_has_heading(subtree_has_heading, tree.before)
                    || subtree_has_heading(subtree_has_heading, tree.after);
            }
        } else {
            change.structural=true;
        }
    }
    std::ranges::sort(change.structural_anchors, {}, &NodeId::v);
    change.structural_anchors.erase(
        std::unique(change.structural_anchors.begin(), change.structural_anchors.end()),
        change.structural_anchors.end());
    std::ranges::sort(change.moved_roots, {}, &NodeId::v);
    change.moved_roots.erase(
        std::unique(change.moved_roots.begin(), change.moved_roots.end()),
        change.moved_roots.end());
    return change;
}

class Editor {
public:
    Editor() { rebuild_document_full_({}); }
    explicit Editor(
        std::string text,
        MarkdownDialect dialect = default_dialect(),
        ParseProgressCallback progress = {})
        : dialect_(std::move(dialect)) {
        rebuild_document_full_(std::move(text), std::nullopt, std::move(progress));
    }

    const EditorDocument& document() const { return document_; }
    const DocumentSymbolIndex& symbols() const { return symbols_; }
    const Outline& outline() const { return outline_; }
    std::u32string text_cps() const { return source_view_ ? source_view_->editor.source() : serialize_markdown_cps(document_); }
    std::string text_utf8() const { return source_view_ ? cps_to_utf8(source_view_->editor.source()) : serialize_markdown(document_); }
    std::u32string markdown_cps() const { return text_cps(); }
    std::string markdown_utf8() const { return text_utf8(); }
    const TextSelection& selection() const { return rendered_selection_.value(); }
    std::optional<std::u32string> editable_source(NodeId id) const {
        return document_editable_text(document_, id);
    }
    std::u32string selected_text_cps() const {
        if (source_view_) return source_view_->editor.selected_text();
        return document_selected_text(document_, rendered_selection_.value()).value_or(std::u32string{});
    }
    std::u32string selected_markdown_cps() const {
        if (source_view_) return source_view_->editor.selected_text();
        return document_selected_markdown(document_, rendered_selection_.value()).value_or(std::u32string{});
    }
    std::uint64_t revision() const { return document_.revision; }
    std::uint64_t content_state() const noexcept { return document_history_.state(); }
    void set_selection(TextSelection selection) {
        const auto anchor_length = document_edit_detail::editable_length(document_, selection.anchor.container_id);
        const auto active_length = document_edit_detail::editable_length(document_, selection.active.container_id);
        if (!anchor_length || !active_length || selection.anchor.source_offset > *anchor_length
            || selection.active.source_offset > *active_length) throw std::out_of_range("selection is outside editable source");
        rendered_selection_.value() = std::move(selection);
    }
    MarkdownDialect const& dialect() const { return dialect_; }
    void set_dialect(MarkdownDialect dialect) {
        auto markdown = serialize_markdown(document_);
        const auto next_revision = document_.revision + 1;
        dialect_ = std::move(dialect);
        rebuild_document_full_(std::move(markdown), next_revision);
    }
    bool has_undo() const { return document_history_.has_undo(); }
    bool has_redo() const { return document_history_.has_redo(); }
    std::optional<EditorDocumentChange> take_last_document_change() {
        return std::exchange(last_document_change_, std::nullopt);
    }

    std::optional<DocumentTransaction> execute_document_enter(TextSelection selection) {
        auto transaction = document_enter(document_, selection);
        if (!transaction) return std::nullopt;
        if (transaction->revision_before == transaction->revision_after) {
            rendered_selection_.value() = transaction->selection_after;
        } else {
            apply_document_transaction_(*transaction);
        }
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_text(TextSelection selection, std::u32string_view text) {
        auto transaction = document_insert_text(document_, selection, text);
        if (!transaction) return std::nullopt;
        if (transaction->revision_before == transaction->revision_after) {
            rendered_selection_.value() = transaction->selection_after;
        } else {
            apply_document_transaction_(*transaction);
        }
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_replace_matches(
        std::span<const RenderedSearchMatch> matches) {
        auto transaction = document_replace_rendered_matches(
            document_, rendered_selection_.value(), matches);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_paste_text(TextSelection selection, std::u32string_view text) {
        auto transaction = document_paste_text(document_, selection, text);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_soft_break(TextSelection selection) {
        auto transaction = document_insert_soft_break(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    bool execute_document_move(DocumentMove movement, bool extend_selection) {
        auto selection = document_move_selection(document_, rendered_selection_.value(), movement, extend_selection);
        if (!selection) return false;
        rendered_selection_.value() = *selection;
        return true;
    }

    std::optional<DocumentTransaction> execute_document_delete_backward(TextSelection selection) {
        auto transaction = document_delete_backward(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_delete_forward(TextSelection selection) {
        auto transaction = document_delete_forward(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_delete_selection(TextSelection selection) {
        auto transaction = document_delete_selection(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_indent_list_item(TextSelection selection) {
        auto transaction = document_indent_list_item(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_outdent_list_item(TextSelection selection) {
        auto transaction = document_outdent_list_item(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_toggle_inline_format(TextSelection selection, InlineFormat kind) {
        auto transaction = document_toggle_inline_format(document_, selection, kind);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_set_heading(TextSelection selection, std::uint8_t level) {
        auto transaction = document_set_heading(document_, selection, level);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_toggle_block_quote(TextSelection selection) {
        auto transaction = document_toggle_block_quote(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_toggle_list(TextSelection selection, bool ordered, bool task) {
        const auto style = task ? ListStyle::Task : ordered ? ListStyle::Ordered : ListStyle::Bullet;
        auto transaction = document_toggle_list(document_, selection, style);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_toggle_task_checkbox(TextSelection selection) {
        auto transaction = document_toggle_task_checkbox(document_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_link(TextSelection selection, const Command& command) {
        auto title = command.title ? std::optional<std::string>{cps_to_utf8(*command.title)} : std::nullopt;
        auto transaction = document_insert_link(document_, selection, cps_to_utf8(command.href), std::move(title));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_image(TextSelection selection, const Command& command) {
        auto transaction = document_insert_image(document_, selection, cps_to_utf8(command.path), cps_to_utf8(command.alt));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_atomic_block(TextSelection selection, const Command& command) {
        BlockNode block;
        if (command.kind == CommandKind::InsertCodeBlock) {
            auto language = command.lang ? std::optional<std::string>{cps_to_utf8(*command.lang)} : std::nullopt;
            block = make_code_block(std::move(language));
        } else if (command.kind == CommandKind::InsertMathBlock) {
            block = make_math_block();
        } else if (command.kind == CommandKind::InsertToc) {
            block = make_toc_block();
        } else if (command.kind == CommandKind::InsertTable) {
            block = make_table_block(document_, command.rows, command.cols);
        } else {
            return std::nullopt;
        }
        auto transaction = document_insert_atomic_block(document_, selection, std::move(block));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_set_code_block_language(
        TextSelection selection,
        const Command& command) {
        auto transaction = document_set_code_block_language(
            document_,
            selection,
            command.lang.value_or(std::u32string{}));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_table_edit(
        TextSelection selection,
        DocumentTableEdit edit,
        TableAlignment alignment = TableAlignment::None,
        std::size_t requested_index = 0) {
        auto transaction = document_edit_table(document_, selection, edit, alignment, requested_index);
        if (!transaction) return std::nullopt;
        if (transaction->revision_before == transaction->revision_after) {
            rendered_selection_.value() = transaction->selection_after;
        } else {
            apply_document_transaction_(*transaction);
        }
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_toggle_callout(TextSelection selection, const Command& command) {
        auto transaction = document_toggle_callout(document_, selection, cps_to_utf8(command.callout_kind));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_insert_footnote(TextSelection selection, const Command&) {
        auto transaction = document_insert_footnote(document_, symbols_, selection);
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    std::optional<DocumentTransaction> execute_document_create_footnote_definition(
        TextSelection selection,
        std::string label) {
        auto transaction = document_create_footnote_definition(
            document_, symbols_, selection, std::move(label));
        if (!transaction) return std::nullopt;
        apply_document_transaction_(*transaction);
        return transaction;
    }

    bool undo() { return replay_history_(false); }
    bool redo() { return replay_history_(true); }

    const SourceEditor* source_editor() const { return source_view_ ? &source_view_->editor : nullptr; }

    void set_source_selection(SourceSelection selection) {
        if (!source_view_) throw std::logic_error("source view is not active");
        source_view_->editor.set_selection(selection);
    }

    bool enter_source_mode() {
        if (source_view_) return false;
        const auto selected=selection();
        auto projection=serialize_markdown_projection(document_);
        SourceViewState view;
        view.index.reset(document_,projection);
        const SourceSelection source_selection{
            view.index.offset(selected.anchor).value_or(0),view.index.offset(selected.active).value_or(0),
            selected.anchor.affinity,selected.active.affinity};
        view.editor=SourceEditor(std::move(projection.text));
        view.editor.set_selection(source_selection);
        source_view_=std::move(view);
        rendered_selection_.reset();
        last_document_change_.reset();
        return true;
    }

    bool exit_source_mode() {
        if (!source_view_) return false;
        const auto selected=source_view_->editor.selection();
        const auto restored=TextSelection{
            source_view_->index.position(document_,selected.anchor,selected.anchor_affinity),
            source_view_->index.position(document_,selected.active,selected.active_affinity)};
        source_view_.reset();
        rendered_selection_=restored;
        last_document_change_.reset();
        return true;
    }

    bool replace_source(SourceRange range,std::u32string replacement) {
        return source_view_ && source_view_->editor.replace(range,std::move(replacement)) && commit_source_patch_();
    }

    bool replace_all_source(std::span<const SourceReplacement> replacements) {
        return source_view_ && source_view_->editor.replace_all(replacements) && commit_source_patch_();
    }

    bool execute_command(const Command& cmd) {
        last_document_change_.reset();
        if (cmd.kind == CommandKind::Undo) return undo();
        if (cmd.kind == CommandKind::Redo) return redo();
        if (source_view_) return execute_source_command_(cmd);
        if (cmd.kind == CommandKind::MoveLeft || cmd.kind == CommandKind::MoveRight
            || cmd.kind == CommandKind::MoveUp || cmd.kind == CommandKind::MoveDown
            || cmd.kind == CommandKind::MoveLineStart || cmd.kind == CommandKind::MoveLineEnd
            || cmd.kind == CommandKind::MoveDocumentStart || cmd.kind == CommandKind::MoveDocumentEnd) {
            auto movement = DocumentMove::Left;
            switch (cmd.kind) {
                case CommandKind::MoveRight: movement = DocumentMove::Right; break;
                case CommandKind::MoveUp: movement = DocumentMove::Up; break;
                case CommandKind::MoveDown: movement = DocumentMove::Down; break;
                case CommandKind::MoveLineStart: movement = DocumentMove::LineStart; break;
                case CommandKind::MoveLineEnd: movement = DocumentMove::LineEnd; break;
                case CommandKind::MoveDocumentStart: movement = DocumentMove::DocumentStart; break;
                case CommandKind::MoveDocumentEnd: movement = DocumentMove::DocumentEnd; break;
                default: break;
            }
            return execute_document_move(movement, cmd.extend_selection);
        }
        if (cmd.kind == CommandKind::SelectAll) {
            auto selection = document_select_all(document_);
            if (!selection) return false;
            rendered_selection_.value() = *selection;
            return true;
        }
        if (cmd.kind == CommandKind::FormatDocument) {
            auto transaction = document_format_markdown(document_, rendered_selection_.value());
            if (!transaction) return false;
            apply_document_transaction_(*transaction);
            return true;
        }
        if (cmd.kind == CommandKind::ToggleCallout
            || cmd.kind == CommandKind::InsertFootnote
            || cmd.kind == CommandKind::CreateFootnoteDefinition) {
            const auto changed = cmd.kind == CommandKind::ToggleCallout
                ? execute_document_toggle_callout(rendered_selection_.value(), cmd).has_value()
                : cmd.kind == CommandKind::InsertFootnote
                    ? execute_document_insert_footnote(rendered_selection_.value(), cmd).has_value()
                    : execute_document_create_footnote_definition(rendered_selection_.value(), cmd.footnote_label).has_value();
            return changed;
        }
        if (cmd.kind == CommandKind::MoveTableCellNext || cmd.kind == CommandKind::MoveTableCellPrevious
            || cmd.kind == CommandKind::InsertTableRowAbove || cmd.kind == CommandKind::InsertTableRowBelow
            || cmd.kind == CommandKind::DeleteTableRow || cmd.kind == CommandKind::MoveTableRowUp
            || cmd.kind == CommandKind::MoveTableRowDown || cmd.kind == CommandKind::InsertTableColumnLeft
            || cmd.kind == CommandKind::InsertTableColumnRight || cmd.kind == CommandKind::DeleteTableColumn
            || cmd.kind == CommandKind::MoveTableColumnLeft || cmd.kind == CommandKind::MoveTableColumnRight
            || cmd.kind == CommandKind::SetTableColumnAlignment || cmd.kind == CommandKind::NormalizeTable
            || cmd.kind == CommandKind::InsertTableRowAt || cmd.kind == CommandKind::InsertTableColumnAt
            || cmd.kind == CommandKind::MoveTableRowTo || cmd.kind == CommandKind::MoveTableColumnTo) {
            auto edit = DocumentTableEdit::Normalize;
            switch (cmd.kind) {
                case CommandKind::MoveTableCellNext: edit = DocumentTableEdit::MoveCellNext; break;
                case CommandKind::MoveTableCellPrevious: edit = DocumentTableEdit::MoveCellPrevious; break;
                case CommandKind::InsertTableRowAbove: edit = DocumentTableEdit::InsertRowAbove; break;
                case CommandKind::InsertTableRowBelow: edit = DocumentTableEdit::InsertRowBelow; break;
                case CommandKind::DeleteTableRow: edit = DocumentTableEdit::DeleteRow; break;
                case CommandKind::MoveTableRowUp: edit = DocumentTableEdit::MoveRowUp; break;
                case CommandKind::MoveTableRowDown: edit = DocumentTableEdit::MoveRowDown; break;
                case CommandKind::InsertTableColumnLeft: edit = DocumentTableEdit::InsertColumnLeft; break;
                case CommandKind::InsertTableColumnRight: edit = DocumentTableEdit::InsertColumnRight; break;
                case CommandKind::DeleteTableColumn: edit = DocumentTableEdit::DeleteColumn; break;
                case CommandKind::MoveTableColumnLeft: edit = DocumentTableEdit::MoveColumnLeft; break;
                case CommandKind::MoveTableColumnRight: edit = DocumentTableEdit::MoveColumnRight; break;
                case CommandKind::SetTableColumnAlignment: edit = DocumentTableEdit::SetColumnAlignment; break;
                case CommandKind::InsertTableRowAt: edit = DocumentTableEdit::InsertRowAt; break;
                case CommandKind::InsertTableColumnAt: edit = DocumentTableEdit::InsertColumnAt; break;
                case CommandKind::MoveTableRowTo: edit = DocumentTableEdit::MoveRowTo; break;
                case CommandKind::MoveTableColumnTo: edit = DocumentTableEdit::MoveColumnTo; break;
                default: break;
            }
            return execute_document_table_edit(rendered_selection_.value(), edit, cmd.table_alignment, cmd.table_index).has_value();
        }
        if (cmd.kind == CommandKind::InsertCodeBlock || cmd.kind == CommandKind::InsertMathBlock
            || cmd.kind == CommandKind::InsertToc || cmd.kind == CommandKind::InsertTable) {
            return execute_document_insert_atomic_block(rendered_selection_.value(), cmd).has_value();
        }
        if (cmd.kind == CommandKind::SetCodeBlockLanguage) {
            return execute_document_set_code_block_language(rendered_selection_.value(), cmd).has_value();
        }
        if (cmd.kind == CommandKind::InsertLink || cmd.kind == CommandKind::InsertImage) {
            const auto changed = cmd.kind == CommandKind::InsertLink
                ? execute_document_insert_link(rendered_selection_.value(), cmd).has_value()
                : execute_document_insert_image(rendered_selection_.value(), cmd).has_value();
            return changed;
        }
        if (cmd.kind == CommandKind::SetHeading || cmd.kind == CommandKind::ClearHeading
            || cmd.kind == CommandKind::ToggleBlockQuote || cmd.kind == CommandKind::ToggleUnorderedList
            || cmd.kind == CommandKind::ToggleOrderedList || cmd.kind == CommandKind::ToggleTaskList
            || cmd.kind == CommandKind::ToggleTaskCheckbox) {
            bool changed = false;
            if (cmd.kind == CommandKind::SetHeading || cmd.kind == CommandKind::ClearHeading) {
                changed = execute_document_set_heading(rendered_selection_.value(),
                    cmd.kind == CommandKind::ClearHeading ? 0 : (std::min)(cmd.level, std::uint8_t{6})).has_value();
            } else if (cmd.kind == CommandKind::ToggleBlockQuote) {
                changed = execute_document_toggle_block_quote(rendered_selection_.value()).has_value();
            } else if (cmd.kind == CommandKind::ToggleTaskCheckbox) {
                changed = execute_document_toggle_task_checkbox(rendered_selection_.value()).has_value();
            } else {
                changed = execute_document_toggle_list(rendered_selection_.value(),
                    cmd.kind == CommandKind::ToggleOrderedList,
                    cmd.kind == CommandKind::ToggleTaskList).has_value();
            }
            return changed;
        }
        if (cmd.kind == CommandKind::ToggleStrong || cmd.kind == CommandKind::ToggleEmphasis
            || cmd.kind == CommandKind::ToggleStrikethrough || cmd.kind == CommandKind::ToggleInlineCode
            || cmd.kind == CommandKind::InsertMathInline) {
            auto kind = InlineFormat::Strong;
            if (cmd.kind == CommandKind::ToggleEmphasis) kind = InlineFormat::Emphasis;
            else if (cmd.kind == CommandKind::ToggleStrikethrough) kind = InlineFormat::Strikethrough;
            else if (cmd.kind == CommandKind::ToggleInlineCode) kind = InlineFormat::Code;
            else if (cmd.kind == CommandKind::InsertMathInline) kind = InlineFormat::Math;
            return execute_document_toggle_inline_format(rendered_selection_.value(), kind).has_value();
        }
        if (cmd.kind == CommandKind::InsertText || cmd.kind == CommandKind::Paste) {
            const auto changed = cmd.kind == CommandKind::Paste
                ? execute_document_paste_text(rendered_selection_.value(), cmd.text).has_value()
                : execute_document_insert_text(rendered_selection_.value(), cmd.text).has_value();
            return changed;
        }
        if (cmd.kind == CommandKind::IndentListItem || cmd.kind == CommandKind::OutdentListItem) {
            const auto changed = cmd.kind == CommandKind::IndentListItem
                ? execute_document_indent_list_item(rendered_selection_.value()).has_value()
                : execute_document_outdent_list_item(rendered_selection_.value()).has_value();
            return changed;
        }
        if (cmd.kind == CommandKind::InsertNewline || cmd.kind == CommandKind::InsertSoftBreak) {
            const auto changed = cmd.kind == CommandKind::InsertNewline
                ? execute_document_enter(rendered_selection_.value()).has_value()
                : execute_document_insert_soft_break(rendered_selection_.value()).has_value();
            return changed;
        }
        if (cmd.kind == CommandKind::DeleteBackward || cmd.kind == CommandKind::DeleteForward || cmd.kind == CommandKind::DeleteSelection) {
            bool changed = false;
            if (!rendered_selection_.value().is_caret() || cmd.kind == CommandKind::DeleteSelection) {
                changed = execute_document_delete_selection(rendered_selection_.value()).has_value();
            } else if (cmd.kind == CommandKind::DeleteBackward) {
                changed = execute_document_delete_backward(rendered_selection_.value()).has_value();
            } else {
                changed = execute_document_delete_forward(rendered_selection_.value()).has_value();
            }
            return changed;
        }
        return false;
    }

    // Translate an input event to a Command (common key bindings). Returns
    // std::nullopt for events that don't synthesise a command (composition,
    // scroll, focus — handled elsewhere).
    std::optional<Command> handle_input(const EditorInputEvent& ev) {
        using K = EditorInputEvent::Kind;
        using TK = KeyCode;
        if (ev.kind == K::TextInput) {
            if (ev.text_input.kind == TextInputEvent::Kind::InsertText && !ev.text_input.text.empty()) {
                return Command::InsertText(ev.text_input.text);
            }
            return std::nullopt;
        }
        if (ev.kind == K::KeyDown) {
            bool ext = ev.key.modifiers.shift;
            switch (ev.key.key_code) {
                case TK::Char:
                    if (ev.key.modifiers.ctrl) {
                        char32_t c = ev.key.ch;
                        if (c == 'b' || c == 'B') { Command c2; c2.kind = CommandKind::ToggleStrong; return c2; }
                        if (c == 'i' || c == 'I') { Command c2; c2.kind = CommandKind::ToggleEmphasis; return c2; }
                        if (c == 'z' || c == 'Z') { Command c2; c2.kind = CommandKind::Undo; return c2; }
                        if (c == 'y' || c == 'Y') { Command c2; c2.kind = CommandKind::Redo; return c2; }
                        if (c == 'c' || c == 'C') { Command c2; c2.kind = CommandKind::Copy; return c2; }
                        if (c == 'x' || c == 'X') { Command c2; c2.kind = CommandKind::Cut; return c2; }
                        if (c == 'v' || c == 'V') { Command c2; c2.kind = CommandKind::Paste; return c2; }
                        if (c == 'a' || c == 'A') { Command c2; c2.kind = CommandKind::SelectAll; return c2; }
                    } else {
                        return Command::InsertText(std::u32string(1, ev.key.ch));
                    }
                    break;
                case TK::Left:  return Command::MoveLeft(ext);
                case TK::Right: return Command::MoveRight(ext);
                case TK::Up:    return Command::MoveUp(ext);
                case TK::Down:  return Command::MoveDown(ext);
                case TK::Home:  { Command c; c.kind = CommandKind::MoveLineStart; c.extend_selection = ext; return c; }
                case TK::End:   { Command c; c.kind = CommandKind::MoveLineEnd;   c.extend_selection = ext; return c; }
                case TK::Backspace: { Command c; c.kind = CommandKind::DeleteBackward; return c; }
                case TK::Delete:    { Command c; c.kind = CommandKind::DeleteForward;  return c; }
                case TK::Enter:     { Command c; c.kind = CommandKind::InsertNewline;  return c; }
                case TK::Tab:       { return Command::InsertText(U"\t"); }
                default: break;
            }
        }
        return std::nullopt;
    }

    void set_caret(TextPosition position) { set_selection(TextSelection::caret(position)); }

private:
    EditorDocument document_;
    DocumentSymbolIndex symbols_;
    DocumentSymbolContributions symbol_contributions_;
    Outline outline_;
    DocumentHistory document_history_{1000};
    std::optional<EditorDocumentChange> last_document_change_;
    std::optional<TextSelection> rendered_selection_{TextSelection{}};
    struct SourceViewState { SourceEditor editor; SourceProjectionIndex index; };
    std::optional<SourceViewState> source_view_;
    MarkdownDialect dialect_ = default_dialect();

    SourceSelection projected_selection_(const TextSelection& selection) const {
        const auto& index=source_view_->index;
        return {index.offset(selection.anchor).value_or(0),index.offset(selection.active).value_or(0),
            selection.anchor.affinity,selection.active.affinity};
    }

    bool execute_source_command_(const Command& command) {
        auto& source=source_view_->editor;
        bool handled=false;
        switch (command.kind) {
            case CommandKind::InsertText:
            case CommandKind::Paste: handled=source.insert_text(command.text);break;
            case CommandKind::InsertNewline:
            case CommandKind::InsertSoftBreak: handled=source.insert_newline();break;
            case CommandKind::DeleteBackward: handled=source.delete_backward();break;
            case CommandKind::DeleteForward: handled=source.delete_forward();break;
            case CommandKind::DeleteSelection: handled=!source.selection().is_caret() && source.replace_selection({});break;
            case CommandKind::IndentListItem: handled=source.indent();break;
            case CommandKind::OutdentListItem: handled=source.outdent();break;
            case CommandKind::FormatDocument: {
                auto formatted=format_markdown_source(source.source(),source.selection(),dialect_);
                handled=source.replace_document(std::move(formatted.source),formatted.selection);break;
            }
            case CommandKind::SelectAll: source.select_all();return true;
            case CommandKind::MoveLeft: source.move_left(command.extend_selection);return true;
            case CommandKind::MoveRight: source.move_right(command.extend_selection);return true;
            case CommandKind::MoveLineStart: source.move_line_start(command.extend_selection);return true;
            case CommandKind::MoveLineEnd: source.move_line_end(command.extend_selection);return true;
            case CommandKind::MoveDocumentStart: source.move_document_start(command.extend_selection);return true;
            case CommandKind::MoveDocumentEnd: source.move_document_end(command.extend_selection);return true;
            default:return false;
        }
        return handled && commit_source_patch_();
    }

    bool commit_source_patch_() {
        auto& view=*source_view_;
        auto patch=view.editor.take_patch();
        if (!patch) return false;
        const auto selected_before=view.index.capture(patch->selection_before);
        const auto allocation_before=document_.next_node_id;
        auto applied=apply_source_document_patch(document_,view.index,view.editor.source(),*patch);
        if (!applied) {
            if (!view.editor.apply_projection_patch(*patch,false)) std::terminate();
            document_.next_node_id=allocation_before;
            return false;
        }
        const auto [first,last]=view.index.current_region(document_,applied->region);
        auto projection=serialize_markdown_region_projection(document_,first,last);
        const auto expected_end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(applied->region.end)
            +source_patch_delta(*patch));
        const auto expected=std::u32string_view(view.editor.source()).substr(applied->region.begin,
            expected_end-applied->region.begin);
        if (projection.text!=expected) {
            if (!apply_document_operations(document_,applied->transaction.operations,false)) std::terminate();
            document_.revision=applied->transaction.revision_before;
            document_.next_node_id=allocation_before;
            if (!view.editor.apply_projection_patch(*patch,false)) std::terminate();
            return false;
        }
        view.index.replace(applied->region,document_,projection,first,last);
        const auto selected_after=view.index.capture(patch->selection_after);
        document_history_.push_source(applied->transaction,selected_before,selected_after);
        last_document_change_=summarize_document_change(applied->transaction.operations,true,
            applied->transaction.revision_before,applied->transaction.revision_after);
        refresh_derived_from_document_(*last_document_change_,applied->transaction.operations);
        return true;
    }

    bool replay_history_(bool forward) {
        last_document_change_.reset();
        const auto* entry=forward ? document_history_.next_redo() : document_history_.next_undo();
        if (!entry) return false;
        const auto selected=entry->selection;
        auto change=summarize_document_change(entry->operations,forward,
            forward ? entry->revision_before : entry->revision_after,
            forward ? entry->revision_after : entry->revision_before);
        std::optional<SourceProjectionRegion> source_region;
        if (source_view_) source_region=source_region_for_operations(document_,source_view_->index,entry->operations);
        TextSelection restored=rendered_selection_.value_or(TextSelection{});
        if (!(forward ? document_history_.redo(document_,restored) : document_history_.undo(document_,restored))) return false;
        const auto* applied=forward ? document_history_.next_undo() : document_history_.next_redo();
        if (!applied) std::terminate();
        last_document_change_=std::move(change);
        refresh_derived_from_document_(*last_document_change_,applied->operations);
        if (source_view_) {
            auto& view=*source_view_;
            const auto [first,last]=view.index.current_region(document_,*source_region);
            auto projection=serialize_markdown_region_projection(document_,first,last);
            auto temporary=view.editor.selection();
            const auto new_size=view.editor.source().size()-(source_region->end-source_region->begin)+projection.text.size();
            temporary.anchor=(std::min)(temporary.anchor,new_size);temporary.active=(std::min)(temporary.active,new_size);
            auto replacement=minimal_source_replacement({source_region->begin,source_region->end},
                std::u32string_view(view.editor.source()).substr(source_region->begin,source_region->end-source_region->begin),projection.text);
            if (replacement) {
                SourcePatch patch{{std::move(*replacement)},view.editor.selection(),temporary};
                if (!view.editor.apply_projection_patch(patch,true)) std::terminate();
            }
            view.index.replace(*source_region,document_,projection,first,last);
            if (const auto* rich=std::get_if<RenderedHistorySelection>(&selected))
                view.editor.set_selection(projected_selection_(forward ? rich->after : rich->before));
            else {
                const auto& source=std::get<SourceHistorySelection>(selected);
                view.editor.set_selection(view.index.restore(forward ? source.after : source.before));
            }
        } else {
            if (const auto* source=std::get_if<SourceHistorySelection>(&selected))
                restored=source_view_selection_in_document(document_,forward ? source->after : source->before);
            rendered_selection_=restored;
        }
        return true;
    }

    void rebuild_document_full_(
        std::string markdown,
        std::optional<std::uint64_t> requested_revision = std::nullopt,
        ParseProgressCallback progress = {}) {
        source_view_.reset();
        rendered_selection_.emplace();
        const auto revision = requested_revision.value_or(document_.revision);
        auto parsed = parse_text(revision, std::move(markdown), dialect_, std::move(progress));
        document_ = std::move(parsed.document);
        symbols_ = std::move(parsed.symbols);
        symbol_contributions_ = std::move(parsed.symbol_contributions);
        outline_ = std::move(parsed.outline);
        document_history_.clear();
        last_document_change_.reset();
        if (document_.root.children.empty()) {
            normalize_document(document_);
            rebuild_document_block_index(document_);
        }
        if (auto position = first_document_text_position(document_)) {
            rendered_selection_.value() = TextSelection::caret(*position);
        }
    }

    void refresh_derived_from_document_(
        const EditorDocumentChange& change,
        const std::vector<DocumentOperation>& operations) {
        if (change.structural) {
            if (!update_document_block_index(document_, operations, change.forward)) {
                rebuild_document_block_index(document_);
            }
            // The index now describes the final tree. An intermediate payload
            // update may target a container removed later in the transaction;
            // an absent final locator is expected, not a reason to scan again.
            auto live_block = [&](NodeId id) -> const BlockNode* {
                const auto path = cached_document_block_path(document_, id);
                if (!path) return nullptr;
                const auto* block = block_at_path(document_.root, *path);
                return block && block->id == id ? block : nullptr;
            };
            bool heading_changed = change.structural_outline_may_change;
            bool symbol_contributions_changed = false;
            bool symbol_order_changed = false;
            auto inspect_moved_subtree = [&](auto& self, const BlockNode& block) -> void {
                symbol_order_changed = symbol_order_changed
                    || symbol_contributions_.by_block.contains(block.id.v);
                heading_changed = heading_changed || block.kind == BlockKind::Heading;
                if (symbol_order_changed && heading_changed) return;
                for (const auto& child : block.children) self(self, child);
            };
            for (auto moved : change.moved_roots) {
                if (const auto* block = live_block(moved)) {
                    inspect_moved_subtree(inspect_moved_subtree, *block);
                }
                if (symbol_order_changed && heading_changed) break;
            }

            for (const auto& operation : operations) {
                const auto* tree = std::get_if<DocumentTreeEdit>(&operation);
                if (!tree || tree->kind == DocumentTreeEditKind::Move) continue;
                const BlockNode* snapshot = nullptr;
                if (tree->kind == DocumentTreeEditKind::Insert) snapshot = &tree->after;
                else if (tree->kind == DocumentTreeEditKind::Remove) snapshot = &tree->before;
                else snapshot = tree->after.id.v != 0 ? &tree->after : &tree->before;
                if (!snapshot || snapshot->id.v == 0) continue;
                symbol_contributions_changed = erase_document_symbol_contributions(
                    *snapshot, symbol_contributions_) || symbol_contributions_changed;
                const bool live_expected = tree->kind == DocumentTreeEditKind::UpdatePayload
                    || (tree->kind == DocumentTreeEditKind::Insert && change.forward)
                    || (tree->kind == DocumentTreeEditKind::Remove && !change.forward);
                if (live_expected) {
                    const auto* live = live_block(snapshot->id);
                    if (!live) continue;
                    if (tree->kind == DocumentTreeEditKind::UpdatePayload) {
                        symbol_contributions_changed = update_document_symbol_contribution(
                            *live, symbol_contributions_) || symbol_contributions_changed;
                    } else {
                        symbol_contributions_changed = update_document_symbol_contributions(
                            *live, symbol_contributions_) || symbol_contributions_changed;
                    }
                }
            }
            std::unordered_set<std::uint64_t> refreshed;
            for (const auto& operation : change.text_operations) {
                const auto& edit = change.forward ? operation.forward : operation.inverse;
                if (!refreshed.insert(edit.container_id.v).second) continue;
                if (const auto* block = live_block(edit.container_id)) {
                    heading_changed = heading_changed || block->kind == BlockKind::Heading;
                    symbol_contributions_changed = update_document_symbol_contribution(
                        *block, symbol_contributions_) || symbol_contributions_changed;
                }
            }
            if (symbol_contributions_changed || symbol_order_changed) {
                symbols_ = project_document_symbol_index(document_, symbol_contributions_);
            }
            if (heading_changed) {
                outline_ = build_outline_from_headings(
                    document_.revision, symbols_.headings);
            }
            outline_.revision = document_.revision;
            return;
        }

        bool heading_changed = false;
        bool symbol_contributions_changed = false;
        std::unordered_set<std::uint64_t> refreshed;
        for (const auto& operation : change.text_operations) {
            const auto& edit = change.forward ? operation.forward : operation.inverse;
            if (!refreshed.insert(edit.container_id.v).second) continue;
            if (const auto* block = find_document_block(document_, edit.container_id)) {
                heading_changed = heading_changed || block->kind == BlockKind::Heading;
                symbol_contributions_changed = update_document_symbol_contribution(
                    *block, symbol_contributions_) || symbol_contributions_changed;
            }
        }
        if (symbol_contributions_changed) {
            symbols_ = project_document_symbol_index(document_, symbol_contributions_);
        }
        if (heading_changed) {
            outline_ = build_outline_from_headings(document_.revision, symbols_.headings);
        }
        outline_.revision = document_.revision;
    }

    void apply_document_transaction_(const DocumentTransaction& transaction) {
        document_history_.push(transaction);
        rendered_selection_.value() = transaction.selection_after;
        last_document_change_ = summarize_document_change(
            transaction.operations,
            true,
            transaction.revision_before,
            transaction.revision_after);
        refresh_derived_from_document_(*last_document_change_, transaction.operations);
    }

};

} // namespace folia
