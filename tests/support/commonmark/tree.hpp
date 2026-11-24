#pragma once

// Shared projection data and structural tree operations.

namespace folia::test::commonmark {

struct Projection {
    std::string strict_tree;
    std::string relaxed_tree;
    // HTML consumed by the shared Python HTMLParser canonicalizer. Markdown
    // text is escaped during projection; raw chunks come only from classified
    // HTML CST nodes or HTML-backed block sources.
    std::string projected_html;
    std::vector<std::string> limitations;
};

struct ProjectionOptions {
    bool table_prefer_style_attributes = false;
    bool full_info_string = false;
};

namespace projection_detail {

struct Node;
struct Child {
    std::string text;
    std::unique_ptr<Node> node;
    Child(std::string value);
    Child(Node value);
    ~Child();
    Child(Child&&) noexcept;
    Child& operator=(Child&&) noexcept;
    Child(Child const&) = delete;
    Child& operator=(Child const&) = delete;
};
struct Node {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Child> children;
    std::optional<std::string> html_override;
    std::string html_prefix;
    std::string html_suffix;
};
inline Child::Child(std::string value) : text(std::move(value)) {}
inline Child::Child(Node value) : node(std::make_unique<Node>(std::move(value))) {}
inline Child::~Child() = default;
inline Child::Child(Child&&) noexcept = default;
inline Child& Child::operator=(Child&&) noexcept = default;

inline std::string utf8(std::u32string_view value) { return cps_to_utf8(value); }

inline void normalize_attrs(Node& node) {
    std::ranges::sort(node.attrs, {}, [](auto const& attr) -> auto const& { return attr.first; });
}

inline Node element(std::string tag, std::vector<std::pair<std::string, std::string>> attrs = {}) {
    Node out{std::move(tag), std::move(attrs), {}};
    normalize_attrs(out);
    return out;
}

inline void append_text(Node& node, std::string value) {
    if (value.empty()) return;
    if (!node.children.empty() && !node.children.back().node) {
        node.children.back().text += value;
    } else {
        node.children.emplace_back(std::move(value));
    }
}

inline void append_node(Node& parent, Node child) {
    parent.children.emplace_back(std::move(child));
}

inline Node raw_html_node(std::string value) {
    Node raw{"#raw-html", {}, {}};
    raw.children.emplace_back(std::move(value));
    return raw;
}

inline void append_raw_html(Node& parent, std::string value) {
    if (!value.empty()) append_node(parent, raw_html_node(std::move(value)));
}

inline Node clone(Node const& source) {
    Node out{source.tag, source.attrs, {}};
    for (auto const& child : source.children) {
        if (child.node) out.children.emplace_back(clone(*child.node));
        else out.children.emplace_back(child.text);
    }
    return out;
}

inline void limitation(std::vector<std::string>& out, std::string value) {
    if (std::ranges::find(out, value) == out.end()) out.push_back(std::move(value));
}

inline Node placeholder(std::string tag, std::string key, std::string value) {
    return element(std::move(tag), {{std::move(key), std::move(value)}});
}

inline bool is_container(std::string_view tag) {
    return tag == "root" || tag == "blockquote" || tag == "ul" || tag == "ol" || tag == "li";
}
inline bool is_block(std::string_view tag) {
    return is_container(tag) || tag == "p" || tag == "pre" || tag == "hr"
        || tag == "section"
        || (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6');
}

} // namespace projection_detail

} // namespace folia::test::commonmark
