// Image block finalization. Both helpers consume already-built CSTs; neither
// parses Markdown/HTML or changes the block's authoritative source.
export module folia.markdown.image_block_promotion;
import std;
import folia.markdown.ast;
import folia.markdown.html_cst;
import folia.markdown.image_dimension;
import folia.markdown.inline_cst;
import folia.markdown.utf;

export namespace folia {

// Presentation optimization applied only after the block grammar has chosen a
// complete Paragraph and its one InlineDocument CST is final. Heading/table
// precedence and paragraph boundaries have already been decided.
inline bool promote_image_paragraph(BlockNode& block) {
    if (block.kind != BlockKind::Paragraph
        || block.inline_content.syntax_mode != InlineSyntaxMode::Markdown
        || block.has_html_source()
        || std::ranges::any_of(block.inline_content.source, [](char32_t ch) {
            return ch == U'\r' || ch == U'\n';
        })
        || block.inline_content.tree.nodes.size() != 1) {
        return false;
    }

    const auto& root = block.inline_content.tree.nodes.front();
    const InlineCstNode* image = nullptr;
    std::optional<std::string> link;
    if (root.kind == InlineCstKind::Image) {
        image = &root;
    } else if (root.kind == InlineCstKind::Link && root.children.size() == 1
        && root.children.front().kind == InlineCstKind::Image) {
        image = &root.children.front();
        link = root.semantics().href;
    }
    if (!image || image->status != ParseStatus::Complete) return false;

    auto& metadata = block.ensure_image_special();
    metadata.src = image->semantics().href;
    metadata.image_alt = image->semantics().alt;
    metadata.image_title = image->semantics().title;
    metadata.image_link = std::move(link);
    block.kind = BlockKind::ImageBlock;
    return true;
}

// Build image semantics from a raw HTML element already recognized by the
// block HTML CST. The raw HTML dispatcher owns the enclosing source/tree,
// content slots, shape, and root-tag metadata after this helper returns.
// `allow_image_block` belongs at the call site: only the single-root-element
// raw HTML case may pass true, while recursive HTML semantic conversion keeps
// the default false and leaves nested <img> elements in their existing owner.
template<class ReadAttribute, class SafeImageTarget>
std::optional<BlockNode> make_html_image_block_from_element(
    const HtmlCstNode& node,
    NodeId id,
    bool allow_image_block,
    ReadAttribute&& read_attribute,
    SafeImageTarget&& safe_image_target) {
    if (!allow_image_block
        || node.kind != HtmlCstKind::Element
        || node.status != HtmlParseStatus::Complete
        || node.tag_name != "img") {
        return std::nullopt;
    }

    auto attribute = [&](std::string_view name) -> std::optional<std::string> {
        auto value = std::invoke(read_attribute, node, name);
        if (!value) return std::nullopt;
        return cps_to_utf8(*value);
    };

    const auto src = attribute("src").value_or(std::string{});
    if (!std::invoke(std::forward<SafeImageTarget>(safe_image_target), src, true)) {
        return std::nullopt;
    }

    BlockNode block;
    block.id = id;
    block.kind = BlockKind::ImageBlock;
    auto& image = block.ensure_image_special();
    image.src = src;
    image.image_alt = attribute("alt").value_or(std::string{});
    image.image_title = attribute("title");
    if (const auto width = attribute("width")) {
        image.image_width = parse_html_image_dimension(*width);
    }
    if (const auto height = attribute("height")) {
        image.image_height = parse_html_image_dimension(*height);
    }

    return block;
}

}
