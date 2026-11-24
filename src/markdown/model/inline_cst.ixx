// folia.markdown.inline_cst — lossless editable inline concrete syntax tree.
export module folia.markdown.inline_cst;
import std;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.image_dimension;
import folia.markdown.text_edit;
import folia.markdown.node_sequence;

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

// Token ends are local to a page. Splicing a literal window repacks its
// boundary pages and moves later page origins without touching their words.
class InlineTokens {
    static constexpr std::size_t page_limit = 256;
    static constexpr std::uint32_t maximum_delta =
        (std::numeric_limits<std::uint32_t>::max)() >> InlineToken::kind_bits;

    struct Page {
        std::size_t first = 0;
        std::size_t base = 0;
        std::vector<std::uint32_t> words;
        unsigned kinds = 0;

        InlineToken at(std::size_t index) const {
            const auto word = words[index];
            return {static_cast<TokenKind>(word & InlineToken::kind_mask),
                base + (word >> InlineToken::kind_bits)};
        }
    };

    Page first_;
    std::unique_ptr<std::vector<Page>> following_;

    Page& page(std::size_t index) { return index == 0 ? first_ : (*following_)[index - 1]; }
    Page const& page(std::size_t index) const { return index == 0 ? first_ : (*following_)[index - 1]; }
    std::size_t page_count() const {
        if (first_.words.empty()) return 0;
        return 1 + (following_ ? following_->size() : 0);
    }
    std::size_t page_for(std::size_t index) const {
        if (index < first_.words.size()) return 0;
        auto found = std::ranges::upper_bound(*following_, index, {}, &Page::first);
        return static_cast<std::size_t>(found - following_->begin());
    }
    Page& append_page(Page next) {
        if (first_.words.empty()) return first_ = std::move(next);
        if (!following_) following_ = std::make_unique<std::vector<Page>>();
        return following_->emplace_back(std::move(next));
    }

public:
    struct Iterator {
        using value_type = InlineToken;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using reference = InlineToken;
        using pointer = void;

        InlineTokens const* tokens = nullptr;
        std::size_t page_index = 0;
        std::size_t word_index = 0;

        InlineToken operator*() const { return tokens->page(page_index).at(word_index); }
        Iterator& operator++() {
            if (++word_index == tokens->page(page_index).words.size()) {
                ++page_index;
                word_index = 0;
            }
            return *this;
        }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        bool operator==(Iterator const&) const = default;
    };

    InlineTokens() = default;
    InlineTokens(InlineTokens const& other) : first_(other.first_) {
        if (other.following_) following_ = std::make_unique<std::vector<Page>>(*other.following_);
    }
    InlineTokens& operator=(InlineTokens const& other) {
        if (this != &other) *this = InlineTokens(other);
        return *this;
    }
    InlineTokens(InlineTokens&&) noexcept = default;
    InlineTokens& operator=(InlineTokens&&) noexcept = default;

    bool empty() const { return first_.words.empty(); }
    std::size_t size() const {
        auto const& last = following_ && !following_->empty() ? following_->back() : first_;
        return last.first + last.words.size();
    }
    InlineToken operator[](std::size_t index) const {
        auto const& current = page(page_for(index));
        return current.at(index - current.first);
    }
    Iterator begin() const { return {this, 0, 0}; }
    Iterator end() const { return {this, page_count(), 0}; }

    bool only_kinds(std::size_t first, std::size_t last, unsigned kinds) const {
        if (first == last) return true;
        auto last_page = page_for(last - 1);
        for (auto index = page_for(first); index <= last_page; ++index) {
            auto const& current = page(index);
            if ((current.kinds & ~kinds) == 0) continue;
            auto begin = (std::max)(first, current.first) - current.first;
            auto end = (std::min)(last, current.first + current.words.size()) - current.first;
            if (begin == 0 && end == current.words.size()) return false;
            for (auto word = begin; word < end; ++word)
                if ((kinds & (1u << static_cast<unsigned>(current.at(word).kind()))) == 0) return false;
        }
        return true;
    }

    void emplace_back(TokenKind kind, std::size_t end) {
        auto count = page_count();
        auto* current = count == 0 ? &first_ : &page(count - 1);
        if (count == 0 || current->words.size() == page_limit || end - current->base > maximum_delta)
            current = &append_page(Page{size(), end, {}});
        auto relative = static_cast<std::uint32_t>(end - current->base);
        current->words.push_back((relative << InlineToken::kind_bits) | static_cast<std::uint32_t>(kind));
        current->kinds |= 1u << static_cast<unsigned>(kind);
    }
    void push_back(InlineToken token) { emplace_back(token.kind(), token.end()); }

    void replace(std::size_t first, std::size_t last, InlineTokens const& replacement,
        std::size_t replacement_base, std::ptrdiff_t delta) {
        // The reparse window always covers at least one existing token.
        auto first_page = page_for(first);
        auto last_page = page_for(last - 1);
        auto begin = page(first_page).first;
        auto end = page(last_page).first + page(last_page).words.size();
        InlineTokens middle;
        for (auto index = begin; index < first; ++index) middle.push_back((*this)[index]);
        for (auto token : replacement) middle.emplace_back(token.kind(), replacement_base + token.end());
        for (auto index = last; index < end; ++index) {
            auto token = (*this)[index];
            middle.emplace_back(token.kind(), token.end() + delta);
        }

        InlineTokens next;
        auto count = page_count();
        auto middle_count = middle.page_count();
        auto next_count = first_page + middle_count + (count - last_page - 1);
        if (next_count > 1) {
            next.following_ = std::make_unique<std::vector<Page>>();
            next.following_->reserve(next_count - 1);
        }
        auto append = [&](Page page, std::ptrdiff_t shift) {
            page.first = next.size();
            page.base += shift;
            next.append_page(std::move(page));
        };
        for (std::size_t index = 0; index < first_page; ++index)
            append(std::move(page(index)), 0);
        for (std::size_t index = 0; index < middle_count; ++index)
            append(std::move(middle.page(index)), 0);
        for (std::size_t index = last_page + 1; index < count; ++index)
            append(std::move(page(index)), delta);
        *this = std::move(next);
    }
};

enum class InlineCstKind {
    Text, Whitespace, SoftBreak, HardBreak, Escape, Entity, Delimiter, Raw,
    CodeSpan, InlineMath, Emphasis, Strong, Strikethrough, Highlight,
    Link, Image, Autolink, HtmlElement, HtmlComment,
    FootnoteRef, WikiLink, Extension,
    Error,
    Incomplete,
};

struct DelimitedRanges {
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
class InlineCstNodes : public NodeSequence<InlineCstNode> {
    using Base = NodeSequence<InlineCstNode>;
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

struct InlineCstMetadata {
    std::optional<DelimitedRanges> ranges;
    std::shared_ptr<InlineCstSemantic> semantic;
};

struct InlineCstNode {
    NodeId id{};
    InlineCstKind kind = InlineCstKind::Text;
    ParseStatus status = ParseStatus::Complete;
    SourceRange range;
    InlineCstNodes children;
    // Plain text is by far the most frequent CST node. Delimiter ranges and
    // semantic projection data are therefore allocated only for node kinds
    // that use them instead of widening every text node.
    std::shared_ptr<InlineCstMetadata> metadata;

    InlineCstMetadata& edit_metadata() {
        if (!metadata) metadata = std::make_shared<InlineCstMetadata>();
        else if (metadata.use_count() != 1)
            metadata = std::make_shared<InlineCstMetadata>(*metadata);
        return *metadata;
    }

    DelimitedRanges const* stored_delimiters() const {
        return metadata && metadata->ranges ? &*metadata->ranges : nullptr;
    }

    bool has_semantics() const { return metadata && metadata->semantic; }

    DelimitedRanges delimiter_ranges() const {
        if (auto ranges = stored_delimiters()) return *ranges;
        // These syntax leaves span one lexeme. Their projection needs no
        // stored subdivision, even when the lexeme covers multiple characters.
        switch (kind) {
        case InlineCstKind::SoftBreak:
        case InlineCstKind::HardBreak:
        case InlineCstKind::Escape:
        case InlineCstKind::Entity:
            return {range, range, std::nullopt};
        default:
            return {};
        }
    }

    void set_delimiter_ranges(DelimitedRanges ranges) {
        edit_metadata().ranges = std::move(ranges);
    }

    InlineCstSemantic const& semantics() const {
        static const InlineCstSemantic empty{};
        return has_semantics() ? *metadata->semantic : empty;
    }

    InlineCstSemantic& ensure_semantics() {
        auto& semantic = edit_metadata().semantic;
        if (!semantic) semantic = std::make_shared<InlineCstSemantic>();
        else if (semantic.use_count() != 1) semantic = std::make_shared<InlineCstSemantic>(*semantic);
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
                node.id, node.kind, node.status, node.range, {}, node.metadata});
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
    InlineTokens tokens;
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
