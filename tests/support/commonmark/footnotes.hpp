#pragma once

// Reference ordering, definition expansion and backreferences.
#include "support/commonmark/tree.hpp"

namespace folia::test::commonmark::projection_detail {

inline std::string footnote_id(std::string_view label) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : label) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~'
            || c == '/' || c == '(' || c == ')') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]);
        }
    }
    return out;
}

inline std::string const& attr(Node const& node, std::string_view name) {
    static const std::string empty;
    auto found = std::ranges::find_if(node.attrs, [&](auto const& value) { return value.first == name; });
    return found == node.attrs.end() ? empty : found->second;
}

struct FootnoteState {
    std::unordered_map<std::string, Node> definitions;
    std::unordered_map<std::string, std::size_t> counts;
    std::unordered_map<std::string, std::size_t> ordinals;
    std::vector<std::string> used;
    std::unordered_set<std::string> expanded;
    std::size_t next_ordinal = 1;
};

inline Node footnote_ref(std::string const& label, FootnoteState& state) {
    const auto ordinal = state.ordinals.try_emplace(label, state.next_ordinal).first->second;
    if (ordinal == state.next_ordinal) ++state.next_ordinal;
    const auto occurrence = ++state.counts[label];
    const auto safe = footnote_id(label);
    auto id = "fnref-" + safe + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
    auto anchor = element("a", {{"data-footnote-ref", ""}, {"href", "#fn-" + safe}, {"id", id}});
    append_text(anchor, std::to_string(ordinal));
    auto sup = element("sup", {{"class", "footnote-ref"}});
    append_node(sup, std::move(anchor));
    if (state.expanded.insert(label).second) {
        state.used.push_back(label);
    }
    return sup;
}

inline void append_backrefs(Node& definition, std::string const& label, FootnoteState const& state) {
    const auto count = state.counts.at(label);
    auto make_link = [&](std::size_t occurrence) {
        auto safe = footnote_id(label);
        auto id = "fnref-" + safe + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
        auto index = std::to_string(state.ordinals.at(label))
            + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
        auto link = element("a", {{"aria-label", "Back to reference " + index},
            {"class", "footnote-backref"}, {"data-footnote-backref", ""},
            {"data-footnote-backref-idx", index}, {"href", "#" + id}});
        append_text(link, "↩");
        if (occurrence > 1) {
            auto sup = element("sup", {{"class", "footnote-ref"}});
            append_text(sup, std::to_string(occurrence));
            append_node(link, std::move(sup));
        }
        return link;
    };
    Node* last_paragraph = nullptr;
    auto find_last = [&](auto&& self, Node& node) -> void {
        for (auto& child : node.children) if (child.node) {
            self(self, *child.node);
            if (child.node->tag == "p") last_paragraph = child.node.get();
        }
    };
    find_last(find_last, definition);
    Node* target = last_paragraph ? last_paragraph : &definition;
    for (std::size_t i = 1; i <= count; ++i) {
        if (!target->children.empty()) append_text(*target, " ");
        append_node(*target, make_link(i));
    }
}

inline void finish_footnotes(Node& root) {
    FootnoteState state;
    auto extract = [&](auto&& self, Node& node) -> void {
        std::vector<Child> body;
        for (auto& child : node.children) {
            if (child.node && child.node->tag == "#footnote-definition") {
                auto label = attr(*child.node, "label");
                state.definitions.emplace(std::move(label), std::move(*child.node));
            } else {
                if (child.node) self(self, *child.node);
                body.emplace_back(std::move(child));
            }
        }
        node.children = std::move(body);
    };
    extract(extract, root);
    auto visit = [&](auto&& self, Node& node) -> void {
        std::vector<Child> children;
        for (auto& child : node.children) {
            if (child.node && child.node->tag == "#footnote-ref") {
                if (state.definitions.contains(attr(*child.node, "label")))
                    children.emplace_back(footnote_ref(attr(*child.node, "label"), state));
                else children.emplace_back(attr(*child.node, "source"));
            } else {
                if (child.node) self(self, *child.node);
                children.emplace_back(std::move(child));
            }
        }
        node.children = std::move(children);
    };
    visit(visit, root);
    for (std::size_t index = 0; index < state.used.size(); ++index) {
        auto& definition = state.definitions.at(state.used[index]);
        visit(visit, definition);
    }
    if (state.used.empty()) return;
    auto section = element("section", {{"class", "footnotes"}, {"data-footnotes", ""}});
    auto ol = element("ol");
    for (auto const& label : state.used) {
        auto li = element("li", {{"id", "fn-" + footnote_id(label)}});
        auto& definition = state.definitions.at(label);
        for (auto& child : definition.children) {
            if (child.node) append_node(li, std::move(*child.node));
            else append_text(li, std::move(child.text));
        }
        append_backrefs(li, label, state);
        append_node(ol, std::move(li));
    }
    append_text(section, "\n");
    append_node(section, std::move(ol));
    append_node(root, std::move(section));
}


} // namespace folia::test::commonmark::projection_detail
