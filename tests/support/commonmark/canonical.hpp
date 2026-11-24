#pragma once

// Strict/relaxed tree normalization and canonical JSON encoding.
#include "support/commonmark/tree.hpp"

namespace folia::test::commonmark::projection_detail {

inline std::string json_string(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.push_back('"');
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[ch >> 4]);
                    out.push_back(hex[ch & 0xf]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
        }
    }
    out.push_back('"');
    return out;
}

inline bool whitespace_only(std::string const& text) {
    // Match the explicit ASCII whitespace set used by the HTML canonicalizer
    // contract, including vertical tab; UTF-8 non-ASCII bytes stay significant.
    return std::ranges::all_of(text, [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    });
}

inline void clean(Node& node, bool relax) {
    std::vector<Child> cleaned;
    for (auto& child : node.children) {
        if (!child.node) {
            if (is_container(node.tag) && whitespace_only(child.text)) continue;
            if (!cleaned.empty() && !cleaned.back().node) cleaned.back().text += child.text;
            else cleaned.emplace_back(std::move(child.text));
            continue;
        }
        clean(*child.node, relax);
        if (relax && node.tag == "li" && child.node->tag == "p") {
            for (auto& grandchild : child.node->children) {
                if (!grandchild.node) {
                    if (!cleaned.empty() && !cleaned.back().node) cleaned.back().text += grandchild.text;
                    else cleaned.emplace_back(std::move(grandchild.text));
                } else cleaned.emplace_back(std::move(*grandchild.node));
            }
        } else {
            cleaned.emplace_back(std::move(*child.node));
        }
    }
    node.children = std::move(cleaned);
    if (!is_container(node.tag)) return;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        auto& child = node.children[i];
        if (child.node) continue;
        auto value = child.text;
        bool previous_block = i == 0;
        bool next_block = i + 1 == node.children.size();
        if (i > 0 && node.children[i - 1].node) previous_block = is_block(node.children[i - 1].node->tag);
        if (i + 1 < node.children.size() && node.children[i + 1].node) next_block = is_block(node.children[i + 1].node->tag);
        if (previous_block) while (!value.empty() && value.front() == '\n') value.erase(value.begin());
        if (next_block) while (!value.empty() && value.back() == '\n') value.pop_back();
        child.text = std::move(value);
    }
    std::erase_if(node.children, [](Child const& child) { return !child.node && child.text.empty(); });
}

inline void encode(Node const& node, std::string& out) {
    out.push_back('[');
    out += json_string(node.tag);
    out += ",[";
    for (std::size_t i = 0; i < node.attrs.size(); ++i) {
        if (i) out.push_back(',');
        out.push_back('[');
        out += json_string(node.attrs[i].first);
        out.push_back(',');
        out += json_string(node.attrs[i].second);
        out.push_back(']');
    }
    out += "],[";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        if (i) out.push_back(',');
        auto const& child = node.children[i];
        if (child.node) encode(*child.node, out);
        else out += json_string(child.text);
    }
    out += "]]";
}


} // namespace folia::test::commonmark::projection_detail
