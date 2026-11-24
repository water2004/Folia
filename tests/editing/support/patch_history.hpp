#pragma once

namespace {

std::string patch_document_state(const EditorDocument& document) {
    std::ostringstream out;
    auto text=[&](std::u32string_view value){out<<std::quoted(cps_to_utf8(value))<<';';};
    text(document.trailing_line_ending);
    walk_blocks(document.root,[&](const BlockNode& block) {
        out<<block.id.v<<':'<<int(block.kind)<<':'<<block.children.size()<<':';
        out<<block.separator_before.has_value();if (block.separator_before) text(*block.separator_before);
        text(block.inline_content.source);out<<int(block.inline_content.syntax_mode);
        for (const auto& token:block.inline_content.tree.tokens) out<<':'<<token.packed_end_and_kind;
        std::vector<const InlineCstNode*> pending;
        for (auto i=block.inline_content.tree.nodes.size();i>0;--i) pending.push_back(&block.inline_content.tree.nodes[i-1]);
        while (!pending.empty()) {
            const auto* node=pending.back();pending.pop_back();
            out<<'['<<node->id.v<<':'<<int(node->kind)<<':'<<int(node->status)<<':'<<node->range.start<<':'<<node->range.end;
            const auto& sem=node->semantics();
            out<<std::quoted(sem.href)<<std::quoted(sem.title.value_or(""))<<std::quoted(sem.label)
                <<std::quoted(sem.target)<<std::quoted(sem.alt)<<std::quoted(sem.html_tag);
            if (auto delimiters = node->stored_delimiters()) {
                const auto& d=*delimiters;
                out<<':'<<d.opening.start<<':'<<d.opening.end
                    <<':'<<d.content.start<<':'<<d.content.end<<':'<<d.closing.has_value();
                if (d.closing) out<<':'<<d.closing->start<<':'<<d.closing->end;
            }
            out<<']';
            for (auto i=node->children.size();i>0;--i) pending.push_back(&node->children[i-1]);
        }
        const auto& heading=block.text_special();
        out<<int(heading.level)<<std::quoted(heading.slug);text(heading.opening_marker);text(heading.closing_marker);
        const auto& list=block.list_special();out<<list.ordered<<list.start<<int(list.delimiter)<<list.tight;
        out<<block.item_special().checked;text(block.item_special().marker);
        out<<std::quoted(block.container_special().callout_kind)<<std::quoted(block.container_special().footnote_label);
        const auto& atomic=block.atomic_special();
        out<<atomic.code_indented<<int(atomic.math_delim)<<int(atomic.toc_marker)<<int(atomic.fmt)
            <<std::quoted(atomic.raw)<<int(atomic.unsup_reason)<<std::quoted(atomic.ext_name);
        if (atomic.reference_definition) out<<std::quoted(atomic.reference_definition->label)
            <<std::quoted(atomic.reference_definition->target.href)
            <<std::quoted(atomic.reference_definition->target.title.value_or(""));
        const auto& image=block.image_special();
        out<<std::quoted(image.src)<<std::quoted(image.image_alt)<<std::quoted(image.image_title.value_or(""))
            <<std::quoted(image.image_link.value_or(""));
        if (block.block_source.storage) {
            text(block.block_source.source());text(block_source_content(block.block_source));
            text(block_source_code_projection(block.block_source));
            for (const auto& context:block.block_source.storage->indentation_contexts)
                out<<context.source_offset<<':'<<context.physical_column<<':'<<context.residual_columns;
        }
        const auto& table=block.table_special();
        text(table.table_separator_source);for (auto alignment:table.table_aligns) out<<int(alignment);
        for (const auto& ending:table.table_internal_line_endings) text(ending);
        if (table.row_syntax) {
            for (auto owner:table.row_syntax->cell_owners) out<<owner.v<<',';
            for (auto owner:table.row_syntax->implicit_cell_owners) out<<owner.v<<',';
            for (const auto& gap:table.row_syntax->gaps) text(gap);
        }
        for (const auto& prefix:block.container_source().lines) {
            out<<prefix.anchor.owner_id.v<<':'<<int(prefix.anchor.part)<<':'<<prefix.anchor.offset
                <<':'<<prefix.lazy<<':'<<int(prefix.role);text(prefix.text);
        }
        if (block.has_html_source()) {
            text(block.html_special().source);
            for (const auto& slot:block.html_special().content_slots)
                out<<slot.owner_id.v<<':'<<slot.source_range.start<<':'<<slot.source_range.end;
        }
        out<<'\n';
    });
    return out.str();
}

std::string patch_render_state(const Editor& editor) {
    const auto model=build_render_model(editor.document(),editor.outline(),editor.symbols(),default_theme_profile());
    std::ostringstream out;
    auto text=[&](std::u32string_view value){out<<std::quoted(cps_to_utf8(value));};
    auto items=[&](const auto& self,const std::vector<InlineRenderItem>& nodes)->void {
        for (const auto& item:nodes) {
            out<<int(item.kind)<<':';text(item.text);text(item.special().display_text);
            out<<std::quoted(item.special().semantic().href)<<std::quoted(item.special().semantic().src);
            self(self,item.special().semantic().children);
        }
    };
    std::vector<const RenderBlock*> pending;
    for (auto i=model.blocks.size();i>0;--i) pending.push_back(&model.blocks[i-1]);
    while (!pending.empty()) {
        const auto* block=pending.back();pending.pop_back();
        out<<int(block->kind)<<':'<<block->child_blocks.size();items(items,block->inline_items);
        const auto& special=block->special();text(special.code_text);text(special.tex);
        out<<std::quoted(special.src)<<std::quoted(special.alt)<<std::quoted(special.raw);
        for (const auto& cell:special.table_cells) items(items,cell);
        for (auto i=block->child_blocks.size();i>0;--i) pending.push_back(&block->child_blocks[i-1]);
    }
    return out.str();
}

void patch_expect_valid(const Editor& editor) {
    expect(bool(validate_document(editor.document()).empty()));
    walk_blocks(editor.document().root,[&](const BlockNode& block) {
        if (const auto* source=editable_inline_document(block)) {
            expect(bool(flatten_tokens(source->tree,source->source)==source->source));
            expect(bool(serialize_lossless(source->tree,source->source)==source->source));
        }
    });
}

void patch_expect_local_counters() {
    const auto counters=read_core_operation_counters();
    expect(counters.full_document_parses==0_u);
    expect(counters.full_document_serializations==0_u);
    expect(counters.full_tree_transaction_diffs==0_u);
}

Command patch_insert(std::u32string text) {
    Command command;command.kind=CommandKind::InsertText;command.text=std::move(text);return command;
}

} // namespace
