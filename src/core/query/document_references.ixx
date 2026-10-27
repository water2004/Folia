// Derived reference-definition lookup shared by editing and history replay.
export module folia.core.document_references;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_document;

export namespace folia {

inline std::span<const ReferenceDefinitionBinding> document_reference_bindings(
    const EditorDocument& document,const std::string& label) {
    if (document.reference_definitions_dirty) {
        document.cached_reference_definitions.clear();
        walk_blocks(document.root,[&](const BlockNode& block) {
            if (block.kind!=BlockKind::LinkDefinition) return;
            const auto& definition=block.atomic_special().reference_definition;
            if (definition) document.cached_reference_definitions[definition->label].push_back({block.id,definition});
        });
        document.reference_definitions_dirty=false;
    }
    const auto found=document.cached_reference_definitions.find(label);
    return found==document.cached_reference_definitions.end()
        ? std::span<const ReferenceDefinitionBinding>{} : std::span<const ReferenceDefinitionBinding>{found->second};
}

inline InlineParseContext document_inline_parse_context(
    const EditorDocument& document,std::function<NodeId()> allocate) {
    InlineParseContext context;
    context.dialect=document.dialect;
    context.allocate_id=std::move(allocate);
    context.resolve_link_label=[&document](const std::string& label)->std::optional<InlineLinkDef> {
        const auto bindings=document_reference_bindings(document,label);
        return bindings.empty() ? std::nullopt : std::optional{bindings.front().syntax->target};
    };
    return context;
}

} // namespace folia
