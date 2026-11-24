#pragma once

// HTML escaping and emission for already projected tree nodes.
#include "support/commonmark/tree.hpp"

namespace folia::test::commonmark::projection_detail {

inline void append_html_raw_display(std::string& out, std::string_view value) {
    for (const char ch : value) {
        if (ch == '\0') out += "\xef\xbf\xbd";
        else out.push_back(ch);
    }
}

inline void append_html_text(std::string& out, std::string_view value) {
    for (const char ch : value) {
        switch (ch) {
            case '\0': out += "\xef\xbf\xbd"; break;
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(ch); break;
        }
    }
}

inline void append_html_attribute(std::string& out, std::string_view value) {
    for (const char ch : value) {
        switch (ch) {
            case '\0': out += "\xef\xbf\xbd"; break;
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '\"': out += "&quot;"; break;
            default: out.push_back(ch); break;
        }
    }
}

inline void html_start_tag(
    std::string& out,
    std::string_view tag,
    std::vector<std::pair<std::string, std::string>> attrs = {}) {
    std::ranges::sort(attrs, {}, [](auto const& attr) -> auto const& { return attr.first; });
    out.push_back('<');
    out.append(tag);
    for (const auto& [name, value] : attrs) {
        out.push_back(' ');
        out += name;
        out += "=\"";
        append_html_attribute(out, value);
        out.push_back('\"');
    }
    out.push_back('>');
}

inline bool html_void_element(std::string_view tag) {
    return tag == "area" || tag == "base" || tag == "br" || tag == "col"
        || tag == "embed" || tag == "hr" || tag == "img" || tag == "input"
        || tag == "link" || tag == "meta" || tag == "param" || tag == "source"
        || tag == "track" || tag == "wbr";
}

inline void render_html_node(Node const& node, std::string& out) {
    if (node.tag == "#raw-html") {
        for (const auto& child : node.children) {
            if (child.node) render_html_node(*child.node, out);
            else append_html_raw_display(out, child.text);
        }
        return;
    }
    if (node.tag == "root") {
        for (const auto& child : node.children) {
            if (child.node) render_html_node(*child.node, out);
            else append_html_text(out, child.text);
        }
        return;
    }
    if (node.html_override) {
        append_html_raw_display(out, *node.html_override);
        return;
    }
    if (!node.html_prefix.empty()) append_html_raw_display(out, node.html_prefix);
    else html_start_tag(out, node.tag, node.attrs);
    const bool table_structure = node.tag == "table" || node.tag == "thead"
        || node.tag == "tbody" || node.tag == "tr";
    if (table_structure) out.push_back('\n');
    for (const auto& child : node.children) {
        if (child.node) render_html_node(*child.node, out);
        else append_html_text(out, child.text);
    }
    if (!node.html_suffix.empty()) append_html_raw_display(out, node.html_suffix);
    else if (node.html_prefix.empty() && !html_void_element(node.tag)) out += "</" + node.tag + ">";
    // These line endings belong to generated Markdown block markup. They
    // matter inside an open raw HTML element and even within an HTML comment.
    if (is_block(node.tag) || table_structure || node.tag == "td" || node.tag == "th") {
        out.push_back('\n');
    }
}

inline std::string render_html(Node const& root) {
    std::string result;
    render_html_node(root, result);
    return result;
}


} // namespace folia::test::commonmark::projection_detail
