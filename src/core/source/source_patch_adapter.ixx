// Translate literal source-view edits to the existing source/tree transaction
// model. Recognition is local; replay never invokes the block parser.
export module folia.core.source_patch_adapter;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_edit_primitives;
import folia.core.document_block_edit;
import folia.core.document_ids;
import folia.core.document_operation_apply;
import folia.core.document_references;
import folia.markdown.document_text;
import folia.core.document_transaction;
import folia.markdown.dialect;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.metadata;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.core.source_patch;
import folia.core.source_projection;
import folia.markdown.text_edit;

export namespace folia {

struct SourceDocumentPatch {
    DocumentTransaction transaction;
    SourceProjectionRegion region;
};

inline SourceProjectionRegion source_region_for_operations(
    const EditorDocument& document,const SourceProjectionIndex& projection,
    std::span<const DocumentOperation> operations) {
    const auto count=document.root.children.size();
    std::size_t first=count-1,last=0;
    bool found=false;
    auto note_index=[&](std::size_t index) {
        index=(std::min)(index,count-1);
        first=(std::min)(first,index);last=(std::max)(last,index);found=true;
    };
    auto note_owner=[&](NodeId owner) {
        if (owner==document.root.id) return;
        if (const auto path=document_block_path(document,owner);path && !path->empty()) note_index(path->front());
    };
    for (const auto& operation:operations) {
        if (const auto* text=std::get_if<DocumentTextOperation>(&operation)) note_owner(text->forward.container_id);
        else if (const auto* tree=std::get_if<DocumentTreeEdit>(&operation)) {
            if (tree->parent_id==document.root.id) note_index(tree->index);
            else note_owner(tree->parent_id);
            if (tree->other_parent_id==document.root.id) note_index(tree->other_index);
            else note_owner(tree->other_parent_id);
            note_owner(tree->before.id);note_owner(tree->after.id);note_owner(tree->moved_id);
        }
    }
    if (!found) first=last=count-1;
    return projection.region(first ? first-1 : 0,(std::min)(last+2,count));
}

namespace source_patch_adapter_detail {

inline std::vector<LinkDefinitionData> definitions(const BlockVec& roots,std::size_t first,std::size_t last) {
    std::vector<LinkDefinitionData> result;
    for (auto i=first;i<last;++i) walk_blocks(roots[i],[&](const BlockNode& block) {
        if (const auto& definition=block.atomic_special().reference_definition) result.push_back(*definition);
    });
    return result;
}

inline std::optional<std::pair<std::string,FrontmatterFormat>> frontmatter(
    const BlockVec& roots,std::size_t first,std::size_t last) {
    for (auto i=first;i<last;++i) if (roots[i].kind==BlockKind::Frontmatter)
        return std::pair{roots[i].atomic_special().raw,roots[i].atomic_special().fmt};
    return std::nullopt;
}

// Source-identical references may acquire different semantics when their
// definition changes. Such owners need a recorded replacement as well.
inline bool same_semantics(const BlockNode& before,const BlockNode& after) {
    std::vector<std::pair<const BlockNode*,const BlockNode*>> blocks{{&before,&after}};
    while (!blocks.empty()) {
        const auto [old,node]=blocks.back();blocks.pop_back();
        if (old->kind!=node->kind || old->children.size()!=node->children.size()
            || old->inline_content.tree.nodes.size()!=node->inline_content.tree.nodes.size()) return false;
        std::vector<std::pair<const InlineCstNode*,const InlineCstNode*>> nodes;
        for (std::size_t i=0;i<old->inline_content.tree.nodes.size();++i)
            nodes.emplace_back(&old->inline_content.tree.nodes[i],&node->inline_content.tree.nodes[i]);
        while (!nodes.empty()) {
            const auto [a,b]=nodes.back();nodes.pop_back();
            if (a->kind!=b->kind || a->status!=b->status || a->range!=b->range
                || a->semantics()!=b->semantics() || a->delimiter_ranges()!=b->delimiter_ranges()
                || a->children.size()!=b->children.size()) return false;
            for (std::size_t i=0;i<a->children.size();++i) nodes.emplace_back(&a->children[i],&b->children[i]);
        }
        for (std::size_t i=0;i<old->children.size();++i) blocks.emplace_back(&old->children[i],&node->children[i]);
    }
    return true;
}

inline std::u32string ending(std::u32string_view source) {
    if (source.ends_with(U"\r\n")) return U"\r\n";
    if (source.ends_with(U"\n")) return U"\n";
    if (source.ends_with(U"\r")) return U"\r";
    return {};
}

inline std::u32string before_region(std::u32string_view after,const SourcePatch& patch,
    const SourceProjectionRegion& region) {
    const auto end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(region.end)+source_patch_delta(patch));
    std::u32string result(after.substr(region.begin,end-region.begin));
    for (auto i=patch.edits.size();i>0;--i) {
        const auto& edit=patch.edits[i-1];
        result.replace(edit.range_before.start-region.begin,edit.inserted.size(),edit.removed);
    }
    return result;
}

inline std::u32string_view body(std::u32string_view source,std::size_t origin,const SourceProjectionBlock& block) {
    return source.substr(block.begin-origin,block.end-block.begin);
}

struct BoundInlineEdit { NodeId owner; SourceRange range; std::u32string replacement; };

inline std::optional<std::vector<BoundInlineEdit>> bind_inline_edits(
    const EditorDocument& document,const SourceProjectionIndex& projection,const SourcePatch& patch) {
    std::vector<BoundInlineEdit> result;
    for (const auto& edit:patch.edits) {
        if (edit.inserted.find_first_of(U"\r\n")!=std::u32string::npos
            || edit.removed.find_first_of(U"\r\n")!=std::u32string::npos) return std::nullopt;
        const auto& root=projection.blocks()[projection.block_at(edit.range_before.start)];
        bool bound=false;
        for (const auto& map:root.owners) {
            const auto* block=find_document_block(document,map.container_id);
            if (!block || block->has_html_source() || !editable_inline_document(*block)
                || block->inline_content.syntax_mode==InlineSyntaxMode::HtmlText) continue;
            const auto begin=edit.range_before.start-root.begin;
            const auto end=edit.range_before.end-root.begin;
            const auto first=std::ranges::lower_bound(map.source_to_serialized,begin);
            const auto last=std::ranges::lower_bound(map.source_to_serialized,end);
            if (first==map.source_to_serialized.end() || last==map.source_to_serialized.end()
                || *first!=begin || *last!=end || static_cast<std::size_t>(last-first)!=edit.range_before.length()) continue;
            bool contiguous=true;
            for (auto p=first;p!=last;++p) if (*(p+1)!=*p+1) {contiguous=false;break;}
            if (!contiguous) continue;
            result.push_back({map.container_id,{static_cast<std::size_t>(first-map.source_to_serialized.begin()),
                static_cast<std::size_t>(last-map.source_to_serialized.begin())},edit.inserted});
            bound=true;break;
        }
        if (!bound) return std::nullopt;
    }
    return result;
}

inline bool same_inline_structure(const BlockVec& before,std::size_t first,std::size_t last,
    const BlockVec& after,std::span<const BoundInlineEdit> edits) {
    if (last-first!=after.size()) return false;
    std::vector<std::pair<const BlockNode*,const BlockNode*>> pending;
    for (auto i=first;i<last;++i) pending.emplace_back(&before[i],&after[i-first]);
    while (!pending.empty()) {
        const auto [old,node]=pending.back();pending.pop_back();
        if (old->kind!=node->kind || old->children.size()!=node->children.size()
            || old->has_html_source() || node->has_html_source()) return false;
        if (const auto* owner=editable_inline_document(*old)) {
            auto expected=owner->source;
            for (const auto& edit:edits) if (edit.owner==old->id)
                expected.replace(edit.range.start,edit.range.length(),edit.replacement);
            if (expected!=node->inline_content.source || owner->syntax_mode!=node->inline_content.syntax_mode) return false;
        } else if (old->block_source.source()!=node->block_source.source()) return false;
        for (std::size_t i=0;i<old->children.size();++i) pending.emplace_back(&old->children[i],&node->children[i]);
    }
    return true;
}

} // namespace source_patch_adapter_detail

inline std::optional<SourceDocumentPatch> apply_source_document_patch(
    EditorDocument& document,const SourceProjectionIndex& projection,
    std::u32string_view after_source,const SourcePatch& patch) {
    using namespace source_patch_adapter_detail;
    if (patch.edits.empty()) return std::nullopt;
    const auto first_touched=projection.block_at(patch.edits.back().range_before.start);
    const auto last_touched=projection.block_at(patch.edits.front().range_before.end);
    auto first=first_touched ? first_touched-1 : 0;
    auto last=(std::min)(last_touched+2,projection.blocks().size());
    ParsedBlockFragment parsed;
    SerializedMarkdownProjection parsed_projection;
    SourceProjectionRegion region;
    std::u32string old_text,new_text;
    for (;;) {
        region=projection.region(first,last);
        const auto new_end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(region.end)+source_patch_delta(patch));
        new_text=after_source.substr(region.begin,new_end-region.begin);
        old_text=before_region(after_source,patch,region);
        auto dialect=document.dialect;
        if (first) dialect.frontmatter={false,false,false};
        auto external=[&](const std::string& label,bool before)->std::optional<InlineLinkDef> {
            for (const auto& binding:document_reference_bindings(document,label)) {
                const auto path=document_block_path(document,binding.owner);
                if (path && !path->empty() && (before ? path->front()<first : path->front()>=last))
                    return binding.syntax->target;
            }
            return std::nullopt;
        };
        parsed=parse_block_fragment(new_text,dialect,
            [&](const std::string& label){return external(label,true);},
            [&](const std::string& label){return external(label,false);});
        // Reference definitions are document-scoped. Changing their contribution
        // really can alter owners before and after this source region.
        if ((first || last!=projection.blocks().size())
            && definitions(document.root.children,first,last)!=definitions(parsed.blocks,0,parsed.blocks.size())) {
            first=0;last=projection.blocks().size();continue;
        }
        parsed_projection=serialize_markdown_fragment_projection(parsed.blocks);
        const bool at_end=last==projection.blocks().size();
        // A stable unchanged final neighbour closes the grammar context. An
        // unclosed fence/HTML/container must keep expanding into following blocks.
        bool stable=at_end;
        if (!stable && !parsed.blocks.empty() && !parsed_projection.blocks.empty()) {
            const auto& old=document.root.children[last-1];
            const auto& node=parsed.blocks.back();
            const auto& range=parsed_projection.blocks.back();
            stable=old.kind==node.kind && new_text.substr(range.begin,range.end-range.begin)
                ==body(old_text,region.begin,projection.blocks()[last-1]);
        }
        if (stable) break;
        ++last;
    }

    const auto revision=document.revision;
    const auto allocation=document.next_node_id;
    const auto metadata_source=frontmatter(document.root.children,first,last);
    const auto next_metadata_source=frontmatter(parsed.blocks,0,parsed.blocks.size());
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document,operations,revision);
    document_edit_detail::NodeAllocator allocator(document);
    auto bindings=bind_inline_edits(document,projection,patch);
    if (bindings && same_inline_structure(document.root.children,first,last,parsed.blocks,*bindings)) {
        for (const auto& edit:*bindings) {
            auto applied=document_edit_detail::edit_inline(document,edit.owner,edit.range,edit.replacement,allocator);
            if (!applied) return std::nullopt;
            document_edit_detail::append_source_operation(operations,std::move(*applied));
        }
    } else {
        if (!parsed.blocks.empty()) parsed.blocks.front().separator_before=document.root.children[first].separator_before;
        auto same=[&](std::size_t old_index,std::size_t new_index) {
            if (!same_semantics(document.root.children[old_index],parsed.blocks[new_index])) return false;
            const auto& old=projection.blocks()[old_index];
            const auto& range=parsed_projection.blocks[new_index];
            if (body(old_text,region.begin,old)!=new_text.substr(range.begin,range.end-range.begin)) return false;
            if (old_index==first || new_index==0)
                return document.root.children[old_index].separator_before==parsed.blocks[new_index].separator_before;
            const auto old_gap=old_text.substr(projection.blocks()[old_index-1].end-region.begin,
                old.begin-projection.blocks()[old_index-1].end);
            const auto new_gap=new_text.substr(parsed_projection.blocks[new_index-1].end,
                range.begin-parsed_projection.blocks[new_index-1].end);
            return old_gap==new_gap;
        };
        // Match untouched emitted ranges through the literal patch. This is a
        // boundary lookup, not a tree diff. It also keeps roots between distant
        // edits when either edit changes the number of parsed blocks.
        std::vector<std::pair<std::size_t,std::size_t>> retained;
        auto lower=patch.edits.size();
        std::ptrdiff_t delta=0;
        for (auto i=first;i<last;++i) {
            const auto& old=projection.blocks()[i];
            while (lower>0 && patch.edits[lower-1].range_before.end<=old.begin) {
                const auto& edit=patch.edits[--lower];
                delta+=static_cast<std::ptrdiff_t>(edit.inserted.size())-static_cast<std::ptrdiff_t>(edit.removed.size());
            }
            if (lower>0) {
                const auto range=patch.edits[lower-1].range_before;
                if ((range.start<old.end && range.end>old.begin)
                    || (range.empty() && range.start>=old.begin && range.start<=old.end)) continue;
            }
            const auto begin=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(old.begin-region.begin)+delta);
            const auto end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(old.end-region.begin)+delta);
            const auto found=std::ranges::lower_bound(parsed_projection.blocks,begin,{},&SerializedBlockRange::begin);
            if (found==parsed_projection.blocks.end() || found->begin!=begin || found->end!=end) continue;
            const auto index=static_cast<std::size_t>(found-parsed_projection.blocks.begin());
            if ((!retained.empty() && index<=retained.back().second) || !same(i,index)) continue;
            retained.emplace_back(i,index);
        }
        auto splice=[&](std::size_t old_begin,std::size_t old_end,std::size_t new_begin,std::size_t new_end) {
            for (auto i=old_end;i>old_begin;--i) {
                DocumentTreeEdit remove;
                remove.kind=DocumentTreeEditKind::Remove;remove.parent_id=document.root.id;remove.index=i-1;
                remove.before=document.root.children[i-1];
                operations.emplace_back(std::move(remove));
                if (!apply_document_operation(document,operations.back(),true)) std::terminate();
            }
            for (auto i=new_begin;i<new_end;++i) {
                auto& node=parsed.blocks[i];
                assign_fresh_document_ids(document,node);
                DocumentTreeEdit insert;
                insert.kind=DocumentTreeEditKind::Insert;insert.parent_id=document.root.id;
                insert.index=old_begin+i-new_begin;insert.after=std::move(node);
                operations.emplace_back(std::move(insert));
                if (!apply_document_operation(document,operations.back(),true)) std::terminate();
            }
        };
        auto old_end=last,new_end=parsed.blocks.size();
        for (auto i=retained.size();i>0;--i) {
            const auto [old_index,new_index]=retained[i-1];
            splice(old_index+1,old_end,new_index+1,new_end);
            old_end=old_index;new_end=new_index;
        }
        splice(first,old_end,0,new_end);
        if (document.root.children.empty()) {
            DocumentTreeEdit insert;
            insert.parent_id=document.root.id;
            insert.after=document_edit_detail::empty_paragraph(allocator,document);
            operations.emplace_back(std::move(insert));
            if (!apply_document_operation(document,operations.back(),true)) std::terminate();
        }
    }
    const auto trailing=ending(after_source);
    if (metadata_source!=next_metadata_source) {
        auto metadata=next_metadata_source
            ? from_frontmatter(next_metadata_source->first,next_metadata_source->second)
            : metadata_from_text(after_source);
        operations.emplace_back(DocumentMetadataEdit{document.metadata,std::move(metadata)});
        if (!apply_document_operation(document,operations.back(),true)) std::terminate();
    }
    if (trailing!=document.trailing_line_ending) {
        operations.emplace_back(DocumentEndingEdit{document.trailing_line_ending,trailing});
        if (!apply_document_operation(document,operations.back(),true)) std::terminate();
    }
    if (operations.empty()) {document.next_node_id=allocation;return std::nullopt;}
    document.revision=revision+1;
    rollback.commit();
    return SourceDocumentPatch{make_recorded_document_transaction(std::move(operations),{},{},revision,
        document.revision,DocumentTransactionReason::Replace),region};
}

} // namespace folia
