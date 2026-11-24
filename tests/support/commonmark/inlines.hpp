#pragma once

// Lossless inline CST traversal and semantic whitespace projection.
#include "support/commonmark/tree.hpp"

namespace folia::test::commonmark::projection_detail {

inline void project_inline_nodes(
    Node& parent,
    InlineDocument const& doc,
    InlineCstNodes const& nodes,
    std::vector<std::string>& limitations) {
    using K = InlineCstKind;
    for (auto const& item : nodes) {
        auto raw = [&] { return utf8(inline_source_slice(doc, item.range)); };
        auto content = [&] { return utf8(inline_source_slice(doc, item.delimiter_ranges().content)); };
        switch (item.kind) {
            case K::Emphasis: {
                auto n = element("em");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Strong: {
                auto n = element("strong");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Strikethrough: {
                auto n = element("del");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Highlight: {
                auto n = element("mark");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::CodeSpan: {
                auto n = element("code");
                append_text(n, cps_to_utf8(normalize_inline_code_span(
                    inline_source_slice(doc, item.delimiter_ranges().content), doc.syntax_mode)));
                append_node(parent, std::move(n));
                break;
            }
            case K::Link: {
                auto attrs = std::vector<std::pair<std::string, std::string>>{{"href", item.semantics().href}};
                if (item.semantics().title) attrs.emplace_back("title", *item.semantics().title);
                auto n = element("a", std::move(attrs));
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Image: {
                auto attrs = std::vector<std::pair<std::string, std::string>>{
                    {"alt", item.semantics().alt}, {"src", item.semantics().href}};
                if (item.semantics().title) attrs.emplace_back("title", *item.semantics().title);
                append_node(parent, element("img", std::move(attrs)));
                break;
            }
            case K::Entity:
                append_text(parent, utf8(decode_inline_entity(inline_source_slice(doc, item.range))));
                break;
            case K::Escape: {
                auto source = inline_source_slice(doc, item.range);
                append_text(parent, source.empty() ? std::string{} : utf8(std::u32string_view(source).substr(source.size() - 1)));
                break;
            }
            case K::SoftBreak:
                if (item.range.end == doc.source.size()) break;
                append_text(parent, "\n");
                break;
            case K::HardBreak:
                if (item.range.end == doc.source.size()) break;
                append_node(parent, element("br"));
                append_text(parent, "\n");
                break;
            case K::Autolink:
                // The CST exposes content and the source range; use semantic href
                // where available and keep the visible spelling from source.
                append_node(parent, element("a", {{"href", item.semantics().href}}));
                if (!parent.children.empty() && parent.children.back().node) {
                    append_text(*parent.children.back().node, content());
                }
                break;
            case K::HtmlElement: {
                const auto& semantic = item.semantics();
                if (item.status != ParseStatus::Complete || semantic.html_tag.empty()) {
                    append_node(parent, placeholder("html-policy/projection", "source", raw()));
                    limitation(limitations, "inline-html-policy/projection");
                    break;
                }
                std::vector<std::pair<std::string, std::string>> attrs;
                attrs.reserve(semantic.html_attributes.size());
                for (const auto& [name, value] : semantic.html_attributes) {
                    attrs.emplace_back(name, value);
                }
                auto n = element(semantic.html_tag, std::move(attrs));
                const auto& ranges = item.delimiter_ranges();
                if (ranges.opening.valid_for(doc.source.size())) {
                    n.html_prefix = utf8(inline_source_slice(doc, ranges.opening));
                }
                if (semantic.html_tag != "img" && semantic.html_tag != "br") {
                    project_inline_nodes(n, doc, item.children, limitations);
                }
                if (ranges.closing && ranges.closing->valid_for(doc.source.size())) {
                    n.html_suffix = utf8(inline_source_slice(doc, *ranges.closing));
                }
                append_node(parent, std::move(n));
                break;
            }
            case K::HtmlComment: {
                const auto& content_range = item.delimiter_ranges().content;
                if (item.status != ParseStatus::Complete || !content_range.valid_for(doc.source.size())) {
                    append_node(parent, placeholder("html-policy/projection", "source", raw()));
                    limitation(limitations, "inline-html-policy/projection");
                    break;
                }
                auto comment = element("comment", {{"text", utf8(inline_source_slice(doc, content_range))}});
                comment.html_override = raw();
                append_node(parent, std::move(comment));
                break;
            }
            case K::Raw: {
                const auto tag = scan_markdown_html_tag(doc.source, item.range.start, item.range.end);
                const auto special = scan_markdown_html_special(doc.source, item.range.start, item.range.end);
                if ((tag && tag->range.end == item.range.end)
                    || (special && special->range.end == item.range.end)) {
                    append_raw_html(parent, raw());
                } else append_text(parent, raw());
                break;
            }
            case K::Incomplete: {
                const auto& semantic = item.semantics();
                const auto& ranges = item.delimiter_ranges();
                if (!semantic.html_tag.empty() && ranges.opening.valid_for(doc.source.size())) {
                    auto n = element(semantic.html_tag);
                    n.html_prefix = utf8(inline_source_slice(doc, ranges.opening));
                    project_inline_nodes(n, doc, item.children, limitations);
                    append_node(parent, std::move(n));
                } else append_text(parent, raw());
                break;
            }
            case K::Error:
                // Preserve the exact incomplete/error spelling as literal text.
                append_text(parent, raw());
                break;
            case K::FootnoteRef: {
                Node ref{"#footnote-ref", {{"label", item.semantics().label}, {"source", raw()}}, {}};
                append_node(parent, std::move(ref));
                break;
            }
            case K::InlineMath:
            case K::WikiLink:
            case K::Extension:
                append_node(parent, placeholder("extension", "kind", std::to_string(static_cast<int>(item.kind))));
                limitation(limitations, "inline-extension:" + std::to_string(static_cast<int>(item.kind)));
                break;
            case K::Text:
            case K::Whitespace:
            case K::Delimiter:
                append_text(parent, raw());
                break;
        }
    }
}

inline InlineCstNodes semantic_inline_nodes(
    InlineDocument const& document,
    InlineBlockWhitespacePolicy policy) {
    auto nodes = document.tree.nodes;
    const auto hidden = inline_block_semantic_whitespace_ranges(document, policy);
    for (const auto& range : hidden) {
        for (auto& node : nodes) {
            if (node.children.empty()
                && (node.kind == InlineCstKind::Text || node.kind == InlineCstKind::Whitespace)
                && range.start >= node.range.start && range.end <= node.range.end) {
                if (range.start == node.range.start) node.range.start = range.end;
                else if (range.end == node.range.end) node.range.end = range.start;
            }
        }
    }
    return nodes;
}

inline bool inline_lossless_nodes(InlineCstNodes const& nodes, InlineDocument const& doc) {
    for (auto const& node : nodes) {
        if (!node.range.valid_for(doc.source.size()) || node.range.empty()) return false;
        if (!inline_lossless_nodes(node.children, doc)) return false;
    }
    return true;
}


} // namespace folia::test::commonmark::projection_detail
