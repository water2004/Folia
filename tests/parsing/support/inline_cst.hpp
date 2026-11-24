#pragma once

namespace {

InlineParseContext test_context() {
    InlineParseContext context;
    context.dialect = default_dialect();
    context.next_id = NodeId{1000};
    return context;
}

bool is_lossless(std::u32string_view source) {
    const auto tree = parse_inline(source, test_context());
    return tokens_partition_source(tree, source.size())
        && roots_partition_source(tree, source.size())
        && flatten_tokens(tree, source) == source
        && serialize_lossless(tree, source) == source;
}

const InlineCstNode* first_node(const InlineCstTree& tree, InlineCstKind kind) {
    std::function<const InlineCstNode*(const InlineCstNodes&)> find =
        [&](const InlineCstNodes& nodes) -> const InlineCstNode* {
            for (const auto& node : nodes) {
                if (node.kind == kind) return &node;
                if (const auto* nested = find(node.children)) return nested;
            }
            return nullptr;
        };
    return find(tree.nodes);
}

const InlineCstNode* first_html_node(
    const InlineCstNodes& nodes,
    std::string_view tag) {
    for (const auto& node : nodes) {
        if (node.kind == InlineCstKind::HtmlElement
            && node.semantics().html_tag == tag) {
            return &node;
        }
        if (const auto* nested = first_html_node(node.children, tag)) return nested;
    }
    return nullptr;
}

bool contains_kind(const InlineCstNodes& nodes, InlineCstKind kind) {
    for (const auto& node : nodes) {
        if (node.kind == kind || contains_kind(node.children, kind)) return true;
    }
    return false;
}

} // namespace
