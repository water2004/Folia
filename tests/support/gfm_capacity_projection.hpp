#pragma once

namespace folia::test::gfm_capacity {

enum class ContainerFixture { NestedBlockquotes, DeepLists };

// These projectors accept only the exact native AST vocabulary produced by
// the two pinned pathological inputs.
inline std::optional<std::string> project_pathological_containers(const BlockNode& root, ContainerFixture which) {
    auto inline_text = [](const BlockNode& block) -> std::optional<std::string> {
        if (block.kind != BlockKind::Paragraph
            || block.inline_content.syntax_mode != InlineSyntaxMode::Markdown
            || block.inline_content.source != U"a") return std::nullopt;
        std::u32string reconstructed;
        struct InlineFrame { const InlineCstNodes* nodes; std::size_t index; };
        std::vector<InlineFrame> frames{{&block.inline_content.tree.nodes, 0}};
        while (!frames.empty()) {
            auto& frame = frames.back();
            if (frame.index == frame.nodes->size()) {
                frames.pop_back();
                continue;
            }
            const auto& node = (*frame.nodes)[frame.index++];
            if (node.kind != InlineCstKind::Text || !node.children.empty()
                || node.range.end < node.range.start
                || node.range.end > block.inline_content.source.size()) return std::nullopt;
            const auto value = inline_source_slice(block.inline_content, node.range);
            if (std::ranges::any_of(value, [](char32_t ch) { return ch != U'a'; }))
                return std::nullopt;
            reconstructed.append(value);
        }
        if (reconstructed != block.inline_content.source) return std::nullopt;
        return cps_to_utf8(reconstructed);
    };

    std::string html;
    if (which == ContainerFixture::NestedBlockquotes) {
        if (root.kind != BlockKind::Document || root.children.size() != 1) return std::nullopt;
        std::size_t quote_depth = 0;
        const BlockNode* current = &root.children.front();
        while (current->kind == BlockKind::BlockQuote) {
            if (current->has_html_source() || current->children.size() != 1) return std::nullopt;
            html += "<blockquote>\n";
            ++quote_depth;
            current = &current->children.front();
        }
        const auto text = inline_text(*current);
        if (!text || !current->children.empty()) return std::nullopt;
        html += "<p>" + *text + "</p>\n";
        while (quote_depth > 0) {
            --quote_depth;
            html += "</blockquote>\n";
        }
        return html;
    }

    if (which != ContainerFixture::DeepLists || root.kind != BlockKind::Document
        || root.children.size() != 1) return std::nullopt;
    std::size_t list_depth = 0;
    const BlockNode* list = &root.children.front();
    while (true) {
        if (list->kind != BlockKind::List || list->has_html_source()
            || list->list_special().ordered || !list->list_special().tight
            || list->children.size() != 1) return std::nullopt;
        const auto& item = list->children.front();
        if (item.kind != BlockKind::ListItem || item.children.empty()
            || item.children.size() > 2) return std::nullopt;
        const auto text = inline_text(item.children.front());
        if (!text) return std::nullopt;
        const bool nested = item.children.size() == 2;
        if (nested && item.children.back().kind != BlockKind::List) return std::nullopt;
        ++list_depth;
        html += "<ul>\n<li>" + *text;
        if (!nested) break;
        html += '\n';
        list = &item.children.back();
    }
    while (list_depth > 0) {
        --list_depth;
        html += "</li>\n</ul>\n";
    }
    return html;
}

inline bool pathological_inline_sources_lossless(const BlockNode& root, std::size_t& owner_count) {
    owner_count = 0;
    std::vector<const BlockNode*> blocks{&root};
    while (!blocks.empty()) {
        const auto* block = blocks.back();
        blocks.pop_back();
        if (is_editable_block_owner(block->kind)) {
            const auto& document = block->inline_content;
            if (flatten_tokens(document.tree, document.source) != document.source
                || serialize_lossless(document.tree, document.source) != document.source)
                return false;
            ++owner_count;
        }
        for (const auto& child : block->children) blocks.push_back(&child);
    }
    return owner_count != 0;
}


} // namespace folia::test::gfm_capacity
