// folia.markdown.ast — unified Markdown block tree plus block-local inline CST.
// CommonMark+GFM+math+toc+frontmatter+footnotes+definition_lists+callouts+
// wiki_links+tables+images and safe recursive HTML semantics.
export module folia.markdown.ast;
import std;
import folia.markdown.block_source;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.image_dimension;
import folia.markdown.text_edit;
import folia.markdown.inline_document;
import folia.markdown.html_cst;
import folia.markdown.container_source;

export namespace folia {

struct BlockNode;

using BlockVec = std::vector<BlockNode>;

enum class BlockKind {
    Document,
    Paragraph, Heading, CalloutTitle, BlockQuote, List, TaskList, ListItem, TaskListItem,
    CodeBlock, MathBlock, Table, TableRow, TableCell,
    ImageBlock, Callout, FootnoteDefinition, Toc, Frontmatter,
    ThematicBreak, LinkDefinition, UnsupportedMarkup, HtmlContainer, Extension,
};

inline bool is_editable_block_owner(BlockKind kind) {
    switch (kind) {
        case BlockKind::Paragraph:
        case BlockKind::Heading:
        case BlockKind::CalloutTitle:
        case BlockKind::TableCell:
        case BlockKind::CodeBlock:
        case BlockKind::MathBlock:
        case BlockKind::ImageBlock:
        case BlockKind::Toc:
        case BlockKind::Frontmatter:
        case BlockKind::ThematicBreak:
        case BlockKind::LinkDefinition:
        case BlockKind::UnsupportedMarkup:
        case BlockKind::Extension:
            return true;
        default:
            return false;
    }
}

struct BlockTextSpecial {
    std::uint8_t level = 0;             // Heading
    std::string slug;                   // Heading
    std::u32string opening_marker;
    std::u32string closing_marker;
};

struct BlockListSpecial {
    bool ordered = false;
    std::uint64_t start = 1;
    char32_t delimiter = U'.';
    // CommonMark list looseness is syntax metadata. It does not affect the
    // editor's list layout or source serialization.
    bool tight = true;
};

struct BlockItemSpecial {
    std::u32string marker;              // ListItem / TaskListItem
    bool checked = false;               // TaskListItem
};

struct LinkDefinitionData {
    std::string label;
    InlineLinkDef target;
    bool operator==(const LinkDefinitionData&) const=default;
};

struct BlockAtomicSpecial {
    bool code_indented = false;
    MathDelimiter math_delim = MathDelimiter::BlockDollar; // MathBlock / InlineMath (in inline)
    TocMarkerKind toc_marker = TocMarkerKind::BracketToc; // Toc
    FrontmatterFormat fmt = FrontmatterFormat::Yaml; // Frontmatter
    std::string raw;                    // Frontmatter.raw / UnsupportedMarkup.raw
    UnsupportedMarkupReason unsup_reason = UnsupportedMarkupReason::UnsafeHtml;
    std::string ext_name;               // Extension
    std::shared_ptr<const LinkDefinitionData> reference_definition;
};

struct BlockTableSpecial {
    std::vector<TableAlignment> table_aligns;
    // Exact block syntax that is not owned by TableCell inline sources.
    // One internal line ending joins each logical table line to the next;
    // the final row's ending remains ordinary sibling separator metadata.
    std::u32string table_separator_source;
    std::vector<std::u32string> table_internal_line_endings;
    // Parsed table rows retain only syntax around cell bodies. Cell bodies
    // continue to live exclusively in the referenced TableCell sources.
    struct RowSyntax {
        std::vector<NodeId> cell_owners;
        // IDs of table-width padding cells have no row-source spelling until
        // their source is edited. This distinguishes them from later inserts.
        std::vector<NodeId> implicit_cell_owners;
        // Exactly one more gap than owner. Gaps contain delimiters/padding,
        // never cell-body text. An empty optional distinguishes generated
        // rows from parsed rows with no physical cells.
        std::vector<std::u32string> gaps;
    };
    std::optional<RowSyntax> row_syntax; // TableRow
    bool table_header_row = false;      // TableRow
};

struct BlockImageSpecial {
    std::string src;        // ImageBlock
    std::string image_alt;
    std::optional<std::string> image_title;
    std::optional<std::string> image_link;
    std::optional<ImageDimension> image_width;
    std::optional<ImageDimension> image_height;
};

struct BlockContainerSpecial {
    std::string callout_kind;           // Callout
    std::string footnote_label;        // FootnoteDefinition
};

struct HtmlContentSlot {
    NodeId owner_id{};
    SourceRange source_range;
};

struct HtmlBlockShapeEntry {
    BlockKind kind = BlockKind::Paragraph;
    std::size_t depth = 0;
    std::size_t child_count = 0;

    auto operator<=>(const HtmlBlockShapeEntry&) const = default;
};

enum class TextAlignment {
    Start,
    Center,
    End,
    Justify,
};

struct BlockHtmlSpecial {
    std::u32string source;
    HtmlCstTree tree;
    std::vector<HtmlContentSlot> content_slots;
    std::vector<HtmlBlockShapeEntry> structure_shape;
    std::string root_tag;
    std::u32string opening_marker;
    std::u32string closing_marker;
    std::optional<TextAlignment> text_alignment;
};

// Rare, independently optional payloads share one indirection so the common
// BlockNodeSpecial stays compact.  These values may coexist (an HTML-backed
// table can also carry container or image metadata), so this is deliberately
// not a variant.
struct BlockSupplementalSpecial {
    std::unique_ptr<BlockImageSpecial> image;
    std::unique_ptr<BlockContainerSpecial> container;
    std::unique_ptr<BlockHtmlSpecial> html;
    std::unique_ptr<ContainerSource> source_prefixes;

    BlockSupplementalSpecial() = default;
    BlockSupplementalSpecial(BlockSupplementalSpecial const& other)
        : image(other.image ? std::make_unique<BlockImageSpecial>(*other.image) : nullptr),
          container(other.container ? std::make_unique<BlockContainerSpecial>(*other.container) : nullptr),
          html(other.html ? std::make_unique<BlockHtmlSpecial>(*other.html) : nullptr),
          source_prefixes(other.source_prefixes
              ? std::make_unique<ContainerSource>(*other.source_prefixes) : nullptr) {}
    BlockSupplementalSpecial& operator=(BlockSupplementalSpecial const& other) {
        if (this == &other) return *this;
        *this = BlockSupplementalSpecial(other);
        return *this;
    }
    BlockSupplementalSpecial(BlockSupplementalSpecial&&) noexcept = default;
    BlockSupplementalSpecial& operator=(BlockSupplementalSpecial&&) noexcept = default;
};

struct BlockNodeSpecial {
    std::unique_ptr<BlockTextSpecial> text;
    std::unique_ptr<BlockListSpecial> list;
    std::unique_ptr<BlockItemSpecial> item;
    std::unique_ptr<BlockAtomicSpecial> atomic;
    std::unique_ptr<BlockTableSpecial> table;
    std::unique_ptr<BlockSupplementalSpecial> supplemental;

    BlockNodeSpecial() = default;
    BlockNodeSpecial(BlockNodeSpecial const& other)
        : text(other.text ? std::make_unique<BlockTextSpecial>(*other.text) : nullptr),
          list(other.list ? std::make_unique<BlockListSpecial>(*other.list) : nullptr),
          item(other.item ? std::make_unique<BlockItemSpecial>(*other.item) : nullptr),
          atomic(other.atomic ? std::make_unique<BlockAtomicSpecial>(*other.atomic) : nullptr),
          table(other.table ? std::make_unique<BlockTableSpecial>(*other.table) : nullptr),
          supplemental(other.supplemental
              ? std::make_unique<BlockSupplementalSpecial>(*other.supplemental)
              : nullptr) {}
    BlockNodeSpecial& operator=(BlockNodeSpecial const& other) {
        if (this == &other) return *this;
        *this = BlockNodeSpecial(other);
        return *this;
    }
    BlockNodeSpecial(BlockNodeSpecial&&) noexcept = default;
    BlockNodeSpecial& operator=(BlockNodeSpecial&&) noexcept = default;
};

struct BlockNode {
    NodeId id{};
    BlockKind kind = BlockKind::Paragraph;
    BlockVec children;                  // the one structural child collection for every container
    InlineDocument inline_content;      // Paragraph / Heading / CalloutTitle / TableCell
    BlockSourceDocument block_source;   // CodeBlock / MathBlock, exact full Markdown source
    // Exact source between the preceding direct sibling and this block.
    // Separators are common on ordinary paragraphs, so keeping this compact
    // optional beside the node avoids allocating the much larger kind payload
    // merely to retain a newline.  It is serializer metadata, never an editing
    // coordinate or part of the block-local source.
    std::optional<std::u32string> separator_before;
    // Paragraphs and structural containers must not carry the full payload of
    // every unrelated block kind. The optional payload retains ordinary value
    // semantics through the deep-copy operations below.
    std::unique_ptr<BlockNodeSpecial> payload;

private:
    struct ShallowCopyTag {};
    BlockNode(BlockNode const& other, ShallowCopyTag)
        : id(other.id), kind(other.kind),
          inline_content(other.inline_content), block_source(other.block_source),
          separator_before(other.separator_before),
          payload(other.payload ? std::make_unique<BlockNodeSpecial>(*other.payload) : nullptr) {}
public:
    BlockNode() = default;
    BlockNode(BlockNode const& other) : BlockNode(other, ShallowCopyTag{}) {
        if (other.children.empty()) return;
        if (std::ranges::none_of(other.children, [](BlockNode const& child) { return !child.children.empty(); })) {
            children = other.children;
            return;
        }
        struct Frame { BlockNode const* source; BlockNode* destination; };
        std::vector<Frame> frames{{&other, this}};
        while (!frames.empty()) {
            const auto frame = frames.back(); frames.pop_back();
            frame.destination->children.reserve(frame.source->children.size());
            for (auto const& child : frame.source->children) {
                // The private shallow constructor copies exactly the existing
                // value fields once, leaving only child ownership to the walk.
                frame.destination->children.push_back(BlockNode(child, ShallowCopyTag{}));
            }
            for (std::size_t index=frame.source->children.size(); index>0; --index) {
                if (!frame.source->children[index-1].children.empty()) frames.push_back({
                    &frame.source->children[index-1], &frame.destination->children[index-1]});
            }
        }
    }
    BlockNode& operator=(BlockNode const& other) {
        if (this != &other) *this = BlockNode(other);
        return *this;
    }
    BlockNode(BlockNode&&) noexcept = default;
    BlockNode& operator=(BlockNode&&) noexcept = default;
    ~BlockNode() {
        // Leaf/shallow trees keep the ordinary allocation-free vector cleanup.
        if (std::ranges::none_of(children, [](BlockNode const& child) { return !child.children.empty(); })) return;
        std::vector<BlockVec*> frames{&children};
        while (!frames.empty()) {
            auto* nodes = frames.back();
            if (nodes->empty()) { frames.pop_back(); continue; }
            if (!nodes->back().children.empty()) frames.push_back(&nodes->back().children);
            else nodes->pop_back();
        }
    }

    BlockNodeSpecial const& special() const {
        static const BlockNodeSpecial empty{};
        return payload ? *payload : empty;
    }

    BlockNodeSpecial& ensure_special() {
        if (!payload) payload = std::make_unique<BlockNodeSpecial>();
        return *payload;
    }

    BlockTextSpecial const& text_special() const {
        static const BlockTextSpecial empty{};
        return payload && payload->text ? *payload->text : empty;
    }
    BlockTextSpecial& ensure_text_special() {
        auto& special = ensure_special();
        if (!special.text) special.text = std::make_unique<BlockTextSpecial>();
        return *special.text;
    }
    BlockListSpecial const& list_special() const {
        static const BlockListSpecial empty{};
        return payload && payload->list ? *payload->list : empty;
    }
    BlockListSpecial& ensure_list_special() {
        auto& special = ensure_special();
        if (!special.list) special.list = std::make_unique<BlockListSpecial>();
        return *special.list;
    }
    BlockItemSpecial const& item_special() const {
        static const BlockItemSpecial empty{};
        return payload && payload->item ? *payload->item : empty;
    }
    BlockItemSpecial& ensure_item_special() {
        auto& special = ensure_special();
        if (!special.item) special.item = std::make_unique<BlockItemSpecial>();
        return *special.item;
    }
    BlockAtomicSpecial const& atomic_special() const {
        static const BlockAtomicSpecial empty{};
        return payload && payload->atomic ? *payload->atomic : empty;
    }
    BlockAtomicSpecial& ensure_atomic_special() {
        auto& special = ensure_special();
        if (!special.atomic) special.atomic = std::make_unique<BlockAtomicSpecial>();
        return *special.atomic;
    }
    BlockTableSpecial const& table_special() const {
        static const BlockTableSpecial empty{};
        return payload && payload->table ? *payload->table : empty;
    }
    BlockTableSpecial& ensure_table_special() {
        auto& special = ensure_special();
        if (!special.table) special.table = std::make_unique<BlockTableSpecial>();
        return *special.table;
    }
    BlockImageSpecial const& image_special() const {
        static const BlockImageSpecial empty{};
        return payload && payload->supplemental && payload->supplemental->image
            ? *payload->supplemental->image
            : empty;
    }
    BlockImageSpecial& ensure_image_special() {
        auto& special = ensure_special();
        if (!special.supplemental) {
            special.supplemental = std::make_unique<BlockSupplementalSpecial>();
        }
        if (!special.supplemental->image) {
            special.supplemental->image = std::make_unique<BlockImageSpecial>();
        }
        return *special.supplemental->image;
    }
    BlockContainerSpecial const& container_special() const {
        static const BlockContainerSpecial empty{};
        return payload && payload->supplemental && payload->supplemental->container
            ? *payload->supplemental->container
            : empty;
    }
    BlockContainerSpecial& ensure_container_special() {
        auto& special = ensure_special();
        if (!special.supplemental) {
            special.supplemental = std::make_unique<BlockSupplementalSpecial>();
        }
        if (!special.supplemental->container) {
            special.supplemental->container = std::make_unique<BlockContainerSpecial>();
        }
        return *special.supplemental->container;
    }

    BlockHtmlSpecial const& html_special() const {
        static const BlockHtmlSpecial empty{};
        return payload && payload->supplemental && payload->supplemental->html
            ? *payload->supplemental->html
            : empty;
    }
    BlockHtmlSpecial& ensure_html_special() {
        auto& special = ensure_special();
        if (!special.supplemental) {
            special.supplemental = std::make_unique<BlockSupplementalSpecial>();
        }
        if (!special.supplemental->html) {
            special.supplemental->html = std::make_unique<BlockHtmlSpecial>();
        }
        return *special.supplemental->html;
    }

    ContainerSource const& container_source() const {
        static const ContainerSource empty{};
        return payload && payload->supplemental && payload->supplemental->source_prefixes
            ? *payload->supplemental->source_prefixes
            : empty;
    }
    ContainerSource& ensure_container_source() {
        auto& special = ensure_special();
        if (!special.supplemental) {
            special.supplemental = std::make_unique<BlockSupplementalSpecial>();
        }
        if (!special.supplemental->source_prefixes) {
            special.supplemental->source_prefixes = std::make_unique<ContainerSource>();
        }
        return *special.supplemental->source_prefixes;
    }
    bool has_container_source() const {
        return payload && payload->supplemental && payload->supplemental->source_prefixes;
    }

    bool has_html_source() const {
        return payload && payload->supplemental && payload->supplemental->html
            && !payload->supplemental->html->source.empty();
    }

    bool has_html_element_provenance() const {
        return payload && payload->supplemental && payload->supplemental->html
            && !payload->supplemental->html->root_tag.empty();
    }
};

inline std::vector<HtmlBlockShapeEntry> html_block_structure_shape(const BlockNode& root) {
    std::vector<HtmlBlockShapeEntry> shape;
    auto collect = [&](auto& self, const BlockNode& block, std::size_t depth) -> void {
        shape.push_back({block.kind, depth, block.children.size()});
        for (const auto& child : block.children) self(self, child, depth + 1);
    };
    collect(collect, root, 0);
    return shape;
}

// Heading sibling helper used in several places.
inline const InlineDocument& heading_inline_content(const BlockNode& b) { return b.inline_content; }
inline bool is_heading_block(const BlockNode& b) { return b.kind == BlockKind::Heading; }

} // namespace folia
