// folia.markdown.inline_cst — lossless editable inline concrete syntax tree.
export module folia.markdown.inline_cst;
import std;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.image_dimension;
import folia.markdown.text_edit;

export namespace folia {

enum class ParseStatus {
    Complete,
    MissingCloser,
    Invalid,
    Ambiguous,
};

// The token stream is a non-overlapping, exhaustive partition of source.
enum class TokenKind {
    Text,
    Whitespace,
    SoftBreak,
    HardBreak,
    Escape,
    Entity,
    Delimiter,
    Raw,
};

struct InlineToken {
    // Tokens are an ordered exhaustive partition, so each start is the
    // previous token's end.  Keep only end+kind in one word instead of
    // storing a second stable identity and two machine-word offsets for every
    // lexical run. Interactive identity belongs to structural CST nodes.
    std::uint64_t packed_end_and_kind = 0;

    static constexpr std::uint64_t kind_bits = 3;
    static constexpr std::uint64_t kind_mask = (std::uint64_t{1} << kind_bits) - 1;
    static constexpr std::uint64_t maximum_end =
        (std::numeric_limits<std::uint64_t>::max)() >> kind_bits;

    InlineToken() = default;
    InlineToken(TokenKind token_kind, std::size_t end) {
        if (end > maximum_end) throw std::length_error("inline token offset exceeds compact range");
        packed_end_and_kind = (static_cast<std::uint64_t>(end) << kind_bits)
            | static_cast<std::uint64_t>(token_kind);
    }

    TokenKind kind() const {
        return static_cast<TokenKind>(packed_end_and_kind & kind_mask);
    }

    std::size_t end() const {
        return static_cast<std::size_t>(packed_end_and_kind >> kind_bits);
    }
};

static_assert(sizeof(InlineToken) == sizeof(std::uint64_t));

enum class InlineCstKind {
    Text, Whitespace, SoftBreak, HardBreak, Escape, Entity, Delimiter, Raw,
    CodeSpan, InlineMath, Emphasis, Strong, Strikethrough,
    Link, Image, Autolink, HtmlElement, HtmlComment,
    FootnoteRef, WikiLink, Extension,
    Error,
    Incomplete,
};

struct DelimitedRanges {
    SourceRange full;
    SourceRange opening;
    SourceRange content;
    std::optional<SourceRange> closing;
    bool operator==(const DelimitedRanges&) const=default;
};

struct InlineCstSemantic {
    std::string href;
    std::optional<std::string> title;
    std::string alt;
    std::string label;
    std::string target;
    std::optional<std::string> alias;
    MathDelimiter math_delim = MathDelimiter::InlineDollar;
    std::string ext_name;
    std::optional<ImageDimension> image_width;
    std::optional<ImageDimension> image_height;
    std::string html_tag;
    // HTML is a lossless embedded syntax island.  The exact attribute
    // spelling remains in InlineDocument.source; this case-folded projection
    // is used only by safe rendering and interaction policy.
    std::unordered_map<std::string, std::string> html_attributes;
    bool operator==(const InlineCstSemantic&) const=default;
};

struct InlineCstNode;

// Vector-compatible owner for the existing CST nodes. Node stays an aggregate;
// ownership operations, unlike node queries, must handle arbitrary tree depth.
class InlineCstNodes : public std::vector<InlineCstNode> {
    using Base = std::vector<InlineCstNode>;
    void clear_owned_tree() noexcept;
    void copy_owned_tree(Base const& source);
public:
    using Base::Base;
    InlineCstNodes() = default;
    InlineCstNodes(InlineCstNodes const& source);
    InlineCstNodes(InlineCstNodes&& source) noexcept = default;
    InlineCstNodes(Base const& source);
    InlineCstNodes(Base&& source) noexcept;
    InlineCstNodes& operator=(InlineCstNodes const& source);
    InlineCstNodes& operator=(InlineCstNodes&& source) noexcept;
    InlineCstNodes& operator=(Base const& source);
    InlineCstNodes& operator=(Base&& source) noexcept;
    InlineCstNodes& operator=(std::initializer_list<InlineCstNode> source);
    ~InlineCstNodes();
};

struct InlineCstNode {
    NodeId id{};
    InlineCstKind kind = InlineCstKind::Text;
    SourceRange range;
    ParseStatus status = ParseStatus::Complete;
    InlineCstNodes children;
    // Plain text is by far the most frequent CST node. Delimiter ranges and
    // semantic projection data are therefore allocated only for node kinds
    // that use them instead of widening every text node.
    std::shared_ptr<DelimitedRanges> delimited;
    std::shared_ptr<InlineCstSemantic> semantic;

    DelimitedRanges const& delimiter_ranges() const {
        static const DelimitedRanges empty{};
        return delimited ? *delimited : empty;
    }

    DelimitedRanges& ensure_delimiter_ranges() {
        if (!delimited) delimited = std::make_shared<DelimitedRanges>();
        return *delimited;
    }

    InlineCstSemantic const& semantics() const {
        static const InlineCstSemantic empty{};
        return semantic ? *semantic : empty;
    }

    InlineCstSemantic& ensure_semantics() {
        if (!semantic) semantic = std::make_shared<InlineCstSemantic>();
        return *semantic;
    }
};

inline void InlineCstNodes::clear_owned_tree() noexcept {
    if (empty()) return;
    // Ordinary leaves and shallow sibling collections allocate no worklist.
    if (std::ranges::none_of(*this, [](InlineCstNode const& node) {
            return std::ranges::any_of(node.children, [](InlineCstNode const& child) { return !child.children.empty(); });
        })) {
        Base::clear();
        return;
    }
    std::vector<InlineCstNodes*> frames;
    frames.push_back(this);
    while (!frames.empty()) {
        auto* nodes = frames.back();
        if (nodes->empty()) { frames.pop_back(); continue; }
        if (!nodes->back().children.empty()) {
            frames.push_back(&nodes->back().children);
        } else {
            // Its child owner is already empty, so this destruction cannot
            // recurse. Metadata keeps precisely the usual shared ownership.
            nodes->pop_back();
        }
    }
}

inline void InlineCstNodes::copy_owned_tree(Base const& source) {
    if (std::ranges::none_of(source, [](InlineCstNode const& node) {
            return std::ranges::any_of(node.children, [](InlineCstNode const& child) { return !child.children.empty(); });
        })) {
        Base::operator=(source);
        return;
    }
    struct Frame { Base const* source; InlineCstNodes* destination; };
    std::vector<Frame> frames;
    frames.push_back({&source, this});
    while (!frames.empty()) {
        const auto frame = frames.back();
        frames.pop_back();
        frame.destination->reserve(frame.source->size());
        for (auto const& node : *frame.source) {
            frame.destination->push_back(InlineCstNode{
                node.id, node.kind, node.range, node.status, {}, node.delimited, node.semantic});
        }
        for (std::size_t index = frame.source->size(); index > 0; --index) {
            const auto& original = (*frame.source)[index-1];
            if (!original.children.empty()) frames.push_back({
                &original.children, &(*frame.destination)[index-1].children});
        }
    }
}

inline InlineCstNodes::InlineCstNodes(InlineCstNodes const& source) { copy_owned_tree(source); }
inline InlineCstNodes::InlineCstNodes(Base const& source) { copy_owned_tree(source); }
inline InlineCstNodes::InlineCstNodes(Base&& source) noexcept : Base(std::move(source)) {}
inline InlineCstNodes::~InlineCstNodes() { clear_owned_tree(); }
inline InlineCstNodes& InlineCstNodes::operator=(std::initializer_list<InlineCstNode> source) {
    InlineCstNodes copy(source); swap(copy); return *this;
}
inline InlineCstNodes& InlineCstNodes::operator=(InlineCstNodes const& source) {
    if (this != &source) { InlineCstNodes copy(source); swap(copy); }
    return *this;
}
inline InlineCstNodes& InlineCstNodes::operator=(InlineCstNodes&& source) noexcept {
    if (this != &source) { clear_owned_tree(); Base::operator=(std::move(source)); }
    return *this;
}
inline InlineCstNodes& InlineCstNodes::operator=(Base const& source) {
    if (static_cast<Base const*>(this) != &source) { InlineCstNodes copy(source); swap(copy); }
    return *this;
}
inline InlineCstNodes& InlineCstNodes::operator=(Base&& source) noexcept {
    if (static_cast<Base*>(this) != &source) { clear_owned_tree(); Base::operator=(std::move(source)); }
    return *this;
}

static_assert(std::is_aggregate_v<InlineCstNode>);
static_assert(std::is_nothrow_move_constructible_v<InlineCstNode>);
static_assert(std::is_nothrow_move_assignable_v<InlineCstNode>);

struct InlineCstTree {
    InlineCstNodes nodes;
    std::vector<InlineToken> tokens;
};

inline std::u32string flatten_tokens(const InlineCstTree& tree, std::u32string_view source) {
    std::u32string flattened;
    flattened.reserve(source.size());
    std::size_t cursor = 0;
    for (const auto& token : tree.tokens) {
        const auto end = token.end();
        if (end <= cursor || end > source.size()) return {};
        flattened.append(source.substr(cursor, end - cursor));
        cursor = end;
    }
    return cursor == source.size() ? flattened : std::u32string{};
}

inline std::u32string serialize_lossless(const InlineCstTree& tree, std::u32string_view source) {
    return flatten_tokens(tree, source);
}

inline bool tokens_partition_source(const InlineCstTree& tree, std::size_t source_length) {
    std::size_t cursor = 0;
    for (const auto& token : tree.tokens) {
        const auto end = token.end();
        if (end <= cursor || end > source_length) return false;
        cursor = end;
    }
    return cursor == source_length;
}

inline bool roots_partition_source(const InlineCstTree& tree, std::size_t source_length) {
    std::size_t cursor = 0;
    for (const auto& node : tree.nodes) {
        if (!node.range.valid_for(source_length) || node.range.empty() || node.range.start != cursor) {
            return false;
        }
        cursor = node.range.end;
    }
    return cursor == source_length;
}

} // namespace folia
