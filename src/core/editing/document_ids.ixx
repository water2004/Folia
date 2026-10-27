// folia.core.document_ids — document-owned monotonic NodeId allocation.
export module folia.core.document_ids;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_cst;

export namespace folia {

namespace document_id_detail {

inline void scan_inline_ids(const InlineCstNodes& nodes, std::uint64_t& maximum) {
    for (const auto& node : nodes) {
        maximum = (std::max)(maximum, node.id.v);
        scan_inline_ids(node.children, maximum);
    }
}

inline void scan_block_ids(const BlockNode& block, std::uint64_t& maximum) {
    maximum = (std::max)(maximum, block.id.v);
    scan_inline_ids(block.inline_content.tree.nodes, maximum);
    for (const auto& child : block.children) scan_block_ids(child, maximum);
}

} // namespace document_id_detail

inline NodeId allocate_document_node_id(EditorDocument& document) {
    if (document.next_node_id == 0) {
        throw std::logic_error("document node id cursor is not initialized");
    }
    return NodeId{document.next_node_id++};
}

// Import a parsed fragment without parsing its CST again. Every reference to
// a block identity is translated in the same pass as that identity's owner.
inline void assign_fresh_document_ids(EditorDocument& document,BlockNode& root) {
    std::unordered_map<std::uint64_t,NodeId> remapped;
    walk_blocks(root,[&](BlockNode& block) {
        const auto old=block.id;
        block.id=allocate_document_node_id(document);
        remapped.emplace(old.v,block.id);
        std::vector<InlineCstNode*> pending;
        for (auto& node:block.inline_content.tree.nodes) pending.push_back(&node);
        while (!pending.empty()) {
            auto* node=pending.back(); pending.pop_back();
            node->id=allocate_document_node_id(document);
            for (auto& child:node->children) pending.push_back(&child);
        }
    });
    auto translate=[&](NodeId& id) {
        if (const auto found=remapped.find(id.v);found!=remapped.end()) id=found->second;
    };
    walk_blocks(root,[&](BlockNode& block) {
        if (block.has_container_source())
            for (auto& line:block.ensure_container_source().lines) translate(line.anchor.owner_id);
        if (block.has_html_source())
            for (auto& slot:block.ensure_html_special().content_slots) translate(slot.owner_id);
        if (block.kind==BlockKind::TableRow && block.table_special().row_syntax) {
            auto& row=*block.ensure_table_special().row_syntax;
            for (auto& id:row.cell_owners) translate(id);
            for (auto& id:row.implicit_cell_owners) translate(id);
        }
    });
}

// Blocks may cross a document boundary through a semantic paste/import. Keep
// their existing identities, but reserve the occupied range before assigning
// IDs to missing descendants.
inline void reserve_document_node_ids(EditorDocument& document, const BlockNode& block) {
    if (document.next_node_id == 0) {
        throw std::logic_error("document node id cursor is not initialized");
    }
    auto maximum = document.next_node_id - 1;
    document_id_detail::scan_block_ids(block, maximum);
    document.next_node_id = (std::max)(document.next_node_id, maximum + 1);
}

} // namespace folia
