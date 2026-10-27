// Canonical serializer event/line-plan writer. Formatting and block-kind
// dispatch remain in the action visitor; this module owns only projection.
export module folia.markdown.serializer_writer;
import std;
import folia.markdown.ast;
import folia.markdown.container_source;
import folia.markdown.ids;

export namespace folia::serializer_detail {

using SerializerLineId = std::uint64_t;
using SerializerLineRecordId = std::uint32_t;
using SerializerRopeId = std::uint32_t;
using SerializerEventId = std::uint32_t;

inline constexpr SerializerLineId no_serializer_line = 0;
inline constexpr SerializerLineRecordId no_serializer_line_record = 0;
inline constexpr SerializerRopeId no_serializer_rope = 0;
inline constexpr SerializerEventId no_serializer_event =
    (std::numeric_limits<SerializerEventId>::max)();

struct SerializerLineFacts {
    bool empty = true;
    bool all_space_or_tab = true;
    bool terminal_line_ending = false;
    // Block separation historically inspects the last emitted code point and
    // compresses only an LF-terminated block to one additional LF. A lone CR
    // still uses the ordinary blank-line separator.
    bool terminal_lf = false;
};

inline SerializerLineFacts summarize_line_text(std::u32string_view text) {
    SerializerLineFacts facts;
    facts.empty = text.empty();
    facts.all_space_or_tab = std::ranges::all_of(text, [](char32_t ch) {
        return ch == U' ' || ch == U'\t';
    });
    facts.terminal_line_ending = !text.empty()
        && (text.back() == U'\r' || text.back() == U'\n');
    facts.terminal_lf = !text.empty() && text.back() == U'\n';
    return facts;
}

inline SerializerLineFacts combine_line_facts(
    SerializerLineFacts left,
    SerializerLineFacts right) {
    return {
        left.empty && right.empty,
        left.all_space_or_tab && right.all_space_or_tab,
        right.empty ? left.terminal_line_ending : right.terminal_line_ending,
        right.empty ? left.terminal_lf : right.terminal_lf,
    };
}

enum class SerializerEventKind : std::uint8_t {
    SourceSpan,
    GeneratedSpan,
    SyntaxAnchor,
    SyntaxAnchorBeforeEnding,
    BeginPrefixScope,
    EndPrefixScope,
    BlockBegin,
    BlockCompletion,
    SiblingSeparator,
};

enum class SerializerPrefixRule : std::uint8_t {
    BlockQuote,
    ListItem,
    FixedLines,
};
enum class SerializerMapRule : std::uint8_t {
    FullSource,
    AtomicEndpoints,
};
enum class SerializerRopeKind : std::uint8_t {
    SourceSpan,
    GeneratedSpan,
    OwnedGeneratedSpan,
    SyntaxAnchor,
    AtomicMapBoundary,
    Concat,
};

struct SerializerPrefixPolicy {
    SerializerPrefixRule rule = SerializerPrefixRule::FixedLines;
    std::u32string marker;
    std::u32string nonempty_prefix;
    std::u32string empty_prefix;
    bool terminal_newline = false;
    bool explicit_empty_paragraph = false;
    bool fixed_prefix_on_first_line = false;
    bool fixed_prefix_on_empty_line = false;
    bool suppress_if_empty_body = false;
};

struct SerializerPrefixScope {
    NodeId scope_id{};
    ContainerSource const* source = nullptr;
    SerializerPrefixPolicy policy;
};

struct SerializerEvent {
    SerializerEventKind kind = SerializerEventKind::GeneratedSpan;
    // Source points into immutable BlockNode-owned data for this serialization
    // call. Generated owns small token text in the flat event vector.
    std::u32string const* source = nullptr;
    std::size_t source_begin = 0;
    std::size_t source_end = 0;
    std::size_t full_source_size = 0;
    NodeId owner{};
    SerializerMapRule map_rule = SerializerMapRule::FullSource;
    std::u32string generated;
    ContainerPrefixAnchor anchor{};
    SerializerPrefixScope prefix_scope{};
    NodeId previous_block{};
    bool previous_empty_paragraph = false;
    bool next_is_list = false;
    bool list_item_context = false;
    bool has_explicit_separator = false;
    std::u32string separator;
};

struct SerializerRopeNode {
    SerializerRopeKind kind = SerializerRopeKind::GeneratedSpan;
    SerializerRopeId left = no_serializer_rope;
    SerializerRopeId right = no_serializer_rope;
    SerializerEventId event = 0;
    ContainerPrefixAnchor anchor{};
    NodeId map_owner{};
    bool map_end_boundary = false;
    bool include_source_end_boundary = true;
    SerializerLineFacts facts{};
    std::size_t codepoint_count = 0;
    std::size_t text_begin = 0;
    std::size_t text_end = 0;
};

struct SerializerPlannedLine {
    SerializerLineId id = no_serializer_line;
    SerializerRopeId content = no_serializer_rope;
    SerializerRopeId ending = no_serializer_rope;
    SerializerLineFacts facts{};
    bool terminal = false;
    bool terminal_lf = false;
};

struct SerializerEolPiece {
    std::size_t content_end = 0;
    std::size_t ending_end = 0;
};

// Splits only the prefix token itself. A CRLF remains one line ending with both
// code points in the generated span. A trailing empty piece is retained so
// the original body line remains after a prefix ending in CR/LF/CRLF.
inline std::vector<SerializerEolPiece> split_prefix_eols(std::u32string_view text) {
    std::vector<SerializerEolPiece> pieces;
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == U'\r') {
            const auto ending_end = index + 1 < text.size() && text[index + 1] == U'\n'
                ? index + 2 : index + 1;
            pieces.push_back({index, ending_end});
            index = ending_end - 1;
            start = ending_end;
        } else if (text[index] == U'\n') {
            pieces.push_back({index, index + 1});
            start = index + 1;
        }
    }
    if (start < text.size() || pieces.empty() || pieces.back().ending_end == text.size()) {
        pieces.push_back({text.size(), text.size()});
    }
    return pieces;
}

struct SerializerAnchorKey {
    std::uint64_t owner = 0;
    ContainerAnchorPart part = ContainerAnchorPart::Source;
    std::size_t offset = 0;
    bool operator==(SerializerAnchorKey const&) const = default;
};
struct SerializerAnchorKeyHash {
    std::size_t operator()(SerializerAnchorKey const& key) const noexcept {
        auto value = std::hash<std::uint64_t>{}(key.owner);
        value ^= std::hash<std::size_t>{}(static_cast<std::size_t>(key.part))
            + std::size_t{0x9e3779b9} + (value << 6) + (value >> 2);
        value ^= std::hash<std::size_t>{}(key.offset)
            + std::size_t{0x9e3779b9} + (value << 6) + (value >> 2);
        return value;
    }
};
inline SerializerAnchorKey serializer_anchor_key(ContainerPrefixAnchor const& anchor) {
    return {anchor.owner_id.v, anchor.part, anchor.offset};
}

// Shared key -> stable logical line index. Transforming a line preserves its ID
// on the suffix that contains its original body and anchor events; prefix EOLs
// allocate new line IDs. Existing anchors therefore need no per-ancestor map
// copy or remap.
class SerializerAnchorLineIndex {
public:
    void insert_first(ContainerPrefixAnchor const& anchor, SerializerLineId line) {
        if (line == no_serializer_line) return;
        lines_.try_emplace(serializer_anchor_key(anchor), canonical(line));
    }
    std::optional<SerializerLineId> find(ContainerPrefixAnchor const& anchor) const {
        const auto found = lines_.find(serializer_anchor_key(anchor));
        if (found == lines_.end()) return std::nullopt;
        return canonical(found->second);
    }
    void alias(SerializerLineId from, SerializerLineId to) {
        if (from == no_serializer_line || to == no_serializer_line) return;
        from = canonical(from);
        to = canonical(to);
        if (from != to) parents_[from] = to;
    }
    SerializerLineId canonicalize(SerializerLineId line) const { return canonical(line); }
private:
    SerializerLineId canonical(SerializerLineId line) const {
        auto found = parents_.find(line);
        while (found != parents_.end() && found->second != line) {
            line = found->second;
            found = parents_.find(line);
        }
        return line;
    }
    std::unordered_map<SerializerAnchorKey, SerializerLineId, SerializerAnchorKeyHash> lines_;
    std::unordered_map<SerializerLineId, SerializerLineId> parents_;
};

class SerializerScopePrefixIndex {
public:
    void build(
        ContainerSource const& source,
        SerializerAnchorLineIndex const& anchors) {
        lines_.clear();
        // Preserve current first-record-wins behavior for records resolving to
        // the same physical line.
        for (const auto& record : source.lines) {
            if (const auto line = anchors.find(record.anchor)) {
                lines_.try_emplace(*line, &record);
            }
        }
    }
    ContainerLinePrefix const* find(SerializerLineId line) const {
        const auto found = lines_.find(line);
        return found == lines_.end() ? nullptr : found->second;
    }
private:
    std::unordered_map<SerializerLineId, ContainerLinePrefix const*> lines_;
};

inline std::u32string choose_serializer_prefix(
    SerializerPrefixPolicy const& policy,
    ContainerLinePrefix const* explicit_record,
    SerializerLineFacts facts,
    bool first_line,
    bool terminal_line) {
    if (explicit_record) {
        if (policy.rule != SerializerPrefixRule::ListItem) return explicit_record->text;
        if (first_line) {
            auto prefix = policy.marker;
            if (explicit_record->role == ContainerPrefixRole::ItemOpening) {
                prefix += explicit_record->text;
            }
            return prefix;
        }
        if (explicit_record->role == ContainerPrefixRole::ItemOpening) {
            return facts.empty || facts.all_space_or_tab
                ? std::u32string{}
                : std::u32string(policy.marker.size(), U' ');
        }
        return explicit_record->text;
    }

    switch (policy.rule) {
        case SerializerPrefixRule::BlockQuote:
            if (policy.terminal_newline && terminal_line && facts.empty
                && !policy.explicit_empty_paragraph) return {};
            return facts.empty ? U">" : U"> ";
        case SerializerPrefixRule::ListItem:
            if (first_line) return policy.marker;
            return facts.empty || facts.all_space_or_tab
                ? std::u32string{}
                : std::u32string(policy.marker.size(), U' ');
        case SerializerPrefixRule::FixedLines:
            if (first_line && !policy.fixed_prefix_on_first_line) return {};
            if (facts.empty) {
                return policy.fixed_prefix_on_empty_line ? policy.empty_prefix : std::u32string{};
            }
            return policy.nonempty_prefix;
    }
    return {};
}

struct SerializerOwnerMap {
    NodeId owner{};
    std::vector<std::size_t> source_to_serialized;
    std::vector<bool> mapped;
    bool atomic_endpoints = false;
};

// Multiple HTML slots for one owner share one map. First emission wins at
// shared source boundaries and later spans fill only unmapped offsets.
class SerializerOwnerMapIndex {
public:
    SerializerOwnerMap& ensure(NodeId owner, std::size_t source_size) {
        const auto found = indices_.find(owner.v);
        if (found != indices_.end()) {
            auto& map = maps_[found->second];
            if (map.atomic_endpoints) return map;
            if (map.source_to_serialized.size() < source_size + 1) {
                map.source_to_serialized.resize(source_size + 1);
                map.mapped.resize(source_size + 1, false);
            }
            return map;
        }
        const auto index = maps_.size();
        indices_.emplace(owner.v, index);
        maps_.push_back({owner, std::vector<std::size_t>(source_size + 1),
            std::vector<bool>(source_size + 1, false), false});
        return maps_.back();
    }
    void atomic_boundary(NodeId owner, bool end_boundary, std::size_t output_offset) {
        auto& map = ensure_atomic(owner);
        if (!map.atomic_endpoints) return;
        const auto index = end_boundary ? std::size_t{1} : std::size_t{0};
        if (map.mapped[index]) return;
        map.source_to_serialized[index] = output_offset;
        map.mapped[index] = true;
    }
    void first_mapping(NodeId owner, std::size_t source_size,
        std::size_t source_offset, std::size_t output_offset) {
        auto& map = ensure(owner, source_size);
        if (source_offset >= map.source_to_serialized.size() || map.mapped[source_offset]) return;
        map.source_to_serialized[source_offset] = output_offset;
        map.mapped[source_offset] = true;
    }
    std::span<const SerializerOwnerMap> maps() const noexcept { return maps_; }
private:
    SerializerOwnerMap& ensure_atomic(NodeId owner) {
        const auto found = indices_.find(owner.v);
        if (found != indices_.end()) return maps_[found->second];
        const auto index = maps_.size();
        indices_.emplace(owner.v, index);
        maps_.push_back({owner, std::vector<std::size_t>(2), std::vector<bool>(2, false), true});
        return maps_.back();
    }
    std::unordered_map<std::uint64_t, std::size_t> indices_;
    std::vector<SerializerOwnerMap> maps_;
};

// All plan edges are IDs into these flat vectors. No plan node recursively owns
// another node, so abandoning or clearing a 50,000-deep plan has flat teardown.
class SerializerPlanArena {
public:
    SerializerLineId new_line_id() { return next_line_id_++; }

    SerializerRopeId append_source(
        SerializerEventId event,
        std::u32string_view value,
        std::size_t begin = 0,
        std::size_t end = std::u32string_view::npos,
        bool include_end_boundary = true) {
        begin = (std::min)(begin, value.size());
        end = end == std::u32string_view::npos
            ? value.size()
            : (std::clamp)(end, begin, value.size());
        const auto piece = value.substr(begin, end - begin);
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::SourceSpan;
        node.event = event;
        node.facts = summarize_line_text(piece);
        node.codepoint_count = piece.size();
        node.text_begin = begin;
        node.text_end = end;
        node.include_source_end_boundary = include_end_boundary;
        return add_rope(std::move(node));
    }

    SerializerRopeId append_generated(
        SerializerEventId event,
        std::u32string_view value,
        std::size_t begin = 0,
        std::size_t end = std::u32string_view::npos) {
        begin = (std::min)(begin, value.size());
        end = end == std::u32string_view::npos
            ? value.size()
            : (std::clamp)(end, begin, value.size());
        value = value.substr(begin, end - begin);
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::GeneratedSpan;
        node.event = event;
        node.facts = summarize_line_text(value);
        node.codepoint_count = value.size();
        node.text_begin = begin;
        node.text_end = end;
        return add_rope(std::move(node));
    }

    SerializerEventId store_generated_token(std::u32string value) {
        if (generated_.size() >= static_cast<std::size_t>((std::numeric_limits<SerializerEventId>::max)())) {
            throw std::length_error("serializer generated-token arena exhausted");
        }
        const auto index = static_cast<SerializerEventId>(generated_.size());
        generated_.push_back(std::move(value));
        return index;
    }

    SerializerRopeId append_stored_generated(
        SerializerEventId id, std::size_t begin = 0,
        std::size_t end = std::u32string::npos) {
        const auto& value = generated(id);
        begin = (std::min)(begin, value.size());
        end = end == std::u32string::npos
            ? value.size()
            : (std::clamp)(end, begin, value.size());
        const auto piece = std::u32string_view(value).substr(begin, end - begin);
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::OwnedGeneratedSpan;
        node.event = id;
        node.facts = summarize_line_text(piece);
        node.codepoint_count = piece.size();
        node.text_begin = begin;
        node.text_end = end;
        return add_rope(std::move(node));
    }

    SerializerRopeId append_anchor_value(ContainerPrefixAnchor anchor) {
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::SyntaxAnchor;
        node.anchor = anchor;
        node.facts = {true, true, false, false};
        return add_rope(std::move(node));
    }
    SerializerRopeId append_atomic_boundary(NodeId owner, bool end_boundary) {
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::AtomicMapBoundary;
        node.map_owner = owner;
        node.map_end_boundary = end_boundary;
        node.facts = {true, true, false, false};
        return add_rope(std::move(node));
    }

    SerializerRopeId concat(SerializerRopeId left, SerializerRopeId right) {
        if (left == no_serializer_rope) return right;
        if (right == no_serializer_rope) return left;
        SerializerRopeNode node;
        node.kind = SerializerRopeKind::Concat;
        node.left = left;
        node.right = right;
        const auto& l = rope(left);
        const auto& r = rope(right);
        node.facts = combine_line_facts(l.facts, r.facts);
        node.codepoint_count = l.codepoint_count + r.codepoint_count;
        return add_rope(std::move(node));
    }

    SerializerLineRecordId append_line(
        SerializerRopeId content,
        SerializerRopeId ending,
        SerializerLineId preserve_id = no_serializer_line) {
        if (lines_.size() >= static_cast<std::size_t>((std::numeric_limits<SerializerLineRecordId>::max)())) {
            throw std::length_error("serializer line arena exhausted");
        }
        SerializerPlannedLine line;
        line.id = preserve_id == no_serializer_line ? next_line_id_++ : preserve_id;
        line.content = content;
        line.ending = ending;
        line.facts = content == no_serializer_rope
            ? SerializerLineFacts{}
            : rope(content).facts;
        const auto ending_facts = ending == no_serializer_rope
            ? SerializerLineFacts{}
            : rope(ending).facts;
        line.terminal = ending_facts.terminal_line_ending;
        line.terminal_lf = ending_facts.terminal_lf;
        lines_.push_back(std::move(line));
        return static_cast<SerializerLineRecordId>(lines_.size());
    }

    // Wrap the input line with a literal/canonical prefix without copying its
    // content rope. EOL-created prefix lines get fresh IDs; the original line
    // ID remains with its body suffix so source/generated anchors stay valid.
    std::vector<SerializerLineRecordId> prepend_prefix(
        SerializerLineRecordId input_record,
        SerializerPlannedLine input,
        std::u32string prefix) {
        std::vector<SerializerLineRecordId> output;
        if (prefix.empty()) {
            output.push_back(input_record);
            return output;
        }
        const auto pieces = split_prefix_eols(prefix);
        if (generated_.size() >= static_cast<std::size_t>((std::numeric_limits<SerializerEventId>::max)())) {
            throw std::length_error("serializer generated-token arena exhausted");
        }
        const auto prefix_id = store_generated_token(std::move(prefix));
        std::size_t start = 0;
        for (const auto& piece : pieces) {
            const bool has_ending = piece.ending_end > piece.content_end;
            if (has_ending) {
                auto content = append_stored_generated(prefix_id, start, piece.content_end);
                auto ending = append_stored_generated(prefix_id, piece.content_end, piece.ending_end);
                output.push_back(append_line(content, ending));
                start = piece.ending_end;
                continue;
            }
            // The final piece is the part that stays on the original body line.
            auto tail = append_stored_generated(prefix_id, start, piece.content_end);
            auto joined = concat(tail, input.content);
            output.push_back(append_line(joined, input.ending, input.id));
            start = piece.ending_end;
        }
        return output;
    }

    SerializerRopeNode const& rope(SerializerRopeId id) const {
        return ropes_[static_cast<std::size_t>(id - 1)];
    }
    SerializerPlannedLine const& line(SerializerLineRecordId id) const {
        return lines_[static_cast<std::size_t>(id - 1)];
    }
    std::u32string const& generated(SerializerEventId id) const { return generated_[id]; }
    std::span<const SerializerRopeNode> ropes() const noexcept { return ropes_; }
    std::span<const SerializerPlannedLine> lines() const noexcept { return lines_; }

private:
    SerializerRopeId add_rope(SerializerRopeNode node) {
        if (ropes_.size() >= static_cast<std::size_t>(std::numeric_limits<SerializerRopeId>::max())) {
            throw std::length_error("serializer plan arena exhausted");
        }
        ropes_.push_back(std::move(node));
        return static_cast<SerializerRopeId>(ropes_.size());
    }
    std::vector<SerializerRopeNode> ropes_;
    std::vector<SerializerPlannedLine> lines_;
    std::vector<std::u32string> generated_;
    SerializerLineId next_line_id_ = 1;
};

class SerializerEventWriter {
public:
    SerializerEventWriter() = default;
    SerializerEventWriter(SerializerEventWriter const&) = delete;
    SerializerEventWriter& operator=(SerializerEventWriter const&) = delete;
    SerializerEventWriter(SerializerEventWriter&&) = delete;
    SerializerEventWriter& operator=(SerializerEventWriter&&) = delete;

    std::span<const SerializerEvent> events() const noexcept { return events_; }
    bool valid_event_stream() const noexcept { return !invalid_scope_order_ && open_scopes_.empty(); }

    SerializerEventId source_span(
        NodeId owner,
        std::u32string const& source,
        std::size_t begin,
        std::size_t end,
        SerializerMapRule map_rule = SerializerMapRule::FullSource) {
        begin = (std::min)(begin, source.size());
        end = (std::clamp)(end, begin, source.size());
        SerializerEvent event;
        event.kind = SerializerEventKind::SourceSpan;
        event.source = &source;
        event.source_begin = begin;
        event.source_end = end;
        event.full_source_size = source.size();
        event.owner = owner;
        event.map_rule = map_rule;
        return push_event(std::move(event));
    }
    SerializerEventId source_span(NodeId owner, std::u32string const& source) {
        return source_span(owner, source, 0, source.size());
    }
    SerializerEventId atomic_source_span(NodeId owner, std::u32string const& source) {
        return source_span(owner, source, 0, source.size(), SerializerMapRule::AtomicEndpoints);
    }
    SerializerEventId generated_span(std::u32string_view value) {
        if (value.empty()) return push_event({});
        SerializerEvent event;
        event.kind = SerializerEventKind::GeneratedSpan;
        event.generated.assign(value);
        return push_event(std::move(event));
    }
    SerializerEventId generated_owned(std::u32string value) {
        if (value.empty()) return push_event({});
        SerializerEvent event;
        event.kind = SerializerEventKind::GeneratedSpan;
        event.generated = std::move(value);
        return push_event(std::move(event));
    }
    SerializerEventId atomic_generated_span(NodeId owner, std::u32string value) {
        SerializerEvent event;
        event.kind = SerializerEventKind::GeneratedSpan;
        event.generated = std::move(value);
        event.owner = owner;
        event.map_rule = SerializerMapRule::AtomicEndpoints;
        return push_event(std::move(event));
    }
    SerializerEventId syntax_anchor(ContainerPrefixAnchor anchor) {
        SerializerEvent event;
        event.kind = SerializerEventKind::SyntaxAnchor;
        event.anchor = anchor;
        return push_event(std::move(event));
    }
    SerializerEventId syntax_anchor_before_ending(ContainerPrefixAnchor anchor) {
        SerializerEvent event;
        event.kind = SerializerEventKind::SyntaxAnchorBeforeEnding;
        event.anchor = anchor;
        return push_event(std::move(event));
    }
    SerializerEventId begin_prefix_scope(
        NodeId id,
        ContainerSource const& source,
        SerializerPrefixPolicy policy) {
        SerializerEvent event;
        event.kind = SerializerEventKind::BeginPrefixScope;
        event.prefix_scope = {id, &source, std::move(policy)};
        const auto event_id = push_event(std::move(event));
        open_scopes_.push_back(id);
        return event_id;
    }
    SerializerEventId end_prefix_scope(NodeId id) {
        if (open_scopes_.empty() || open_scopes_.back() != id) {
            invalid_scope_order_ = true;
            return no_serializer_event;
        }
        open_scopes_.pop_back();
        SerializerEvent event;
        event.kind = SerializerEventKind::EndPrefixScope;
        event.owner = id;
        return push_event(std::move(event));
    }
    SerializerEventId block_completion(NodeId id) {
        SerializerEvent event;
        event.kind = SerializerEventKind::BlockCompletion;
        event.owner = id;
        return push_event(std::move(event));
    }
    SerializerEventId block_begin(NodeId id) {
        SerializerEvent event;
        event.kind = SerializerEventKind::BlockBegin;
        event.owner = id;
        return push_event(std::move(event));
    }
    SerializerEventId sibling_separator(
        NodeId previous,
        NodeId current,
        bool previous_empty_paragraph,
        bool next_is_list,
        std::optional<std::u32string> explicit_separator,
        bool list_item_context = false) {
        SerializerEvent event;
        event.kind = SerializerEventKind::SiblingSeparator;
        event.previous_block = previous;
        event.owner = current;
        event.previous_empty_paragraph = previous_empty_paragraph;
        event.next_is_list = next_is_list;
        event.list_item_context = list_item_context;
        event.has_explicit_separator = explicit_separator.has_value();
        if (explicit_separator) event.separator = std::move(*explicit_separator);
        return push_event(std::move(event));
    }
private:
    SerializerEventId push_event(SerializerEvent event) {
        if (events_.size() >= static_cast<std::size_t>(std::numeric_limits<SerializerEventId>::max())) {
            throw std::length_error("serializer event tape exhausted");
        }
        const auto id = static_cast<SerializerEventId>(events_.size());
        events_.push_back(std::move(event));
        return id;
    }
    std::vector<SerializerEvent> events_;
    std::vector<NodeId> open_scopes_;
    bool invalid_scope_order_ = false;
};

struct SerializerBlockRange {
    NodeId owner{};
    std::size_t begin=0;
    std::size_t end=0;
};

class SerializerLinePlanner {
public:
    SerializerLinePlanner(SerializerEventWriter& writer, SerializerPlanArena& plan)
        : writer_(writer), plan_(plan) {}

    std::vector<SerializerLineRecordId> build() {
        if (!writer_.valid_event_stream()) {
            throw std::logic_error("invalid serializer prefix scope nesting");
        }
        const auto events = writer_.events();
        for (const auto& event : events) {
            if (event.kind != SerializerEventKind::BeginPrefixScope || event.prefix_scope.source == nullptr) continue;
            for (const auto& record : event.prefix_scope.source->lines) {
                requested_anchors_.insert(serializer_anchor_key(record.anchor));
            }
        }

        stack_.push_back(make_frame({}));
        for (SerializerEventId event_id = 0; event_id < events.size(); ++event_id) {
            const auto& event = events[event_id];
            switch (event.kind) {
                case SerializerEventKind::SourceSpan:
                    append_source(event_id, event);
                    break;
                case SerializerEventKind::GeneratedSpan:
                    append_generated(event_id, event.generated);
                    break;
                case SerializerEventKind::SyntaxAnchor:
                    attach_anchor(event_id, event.anchor);
                    break;
                case SerializerEventKind::SyntaxAnchorBeforeEnding:
                    attach_anchor_before_ending(event.anchor);
                    break;
                case SerializerEventKind::BeginPrefixScope:
                    stack_.push_back(make_frame(event.prefix_scope));
                    break;
                case SerializerEventKind::EndPrefixScope:
                    close_scope(event.owner);
                    break;
                case SerializerEventKind::BlockBegin:
                    block_starts_[event.owner.v] = stack_.back().total_codepoints;
                    if (stack_.size()==1) outer_starts_[event.owner.v]=stack_.back().total_codepoints;
                    break;
                case SerializerEventKind::BlockCompletion:
                    complete_block(event.owner);
                    if (stack_.size()==1) {
                        const auto found=outer_starts_.find(event.owner.v);
                        if (found!=outer_starts_.end())
                            block_ranges_.push_back({event.owner,found->second,stack_.back().total_codepoints});
                    }
                    break;
                case SerializerEventKind::SiblingSeparator:
                    append_separator(event);
                    break;
            }
        }
        if (stack_.size() != 1) throw std::logic_error("unclosed serializer prefix scope");
        finish_line(stack_.back());
        return std::move(stack_.back().lines);
    }

    SerializerAnchorLineIndex const& anchor_lines() const noexcept { return anchor_lines_; }
    std::span<const SerializerBlockRange> block_ranges() const noexcept { return block_ranges_; }

private:
    struct Frame {
        bool prefix_scope = false;
        SerializerPrefixScope scope{};
        SerializerRopeId content = no_serializer_rope;
        SerializerLineId current_line = no_serializer_line;
        std::size_t total_codepoints = 0;
        std::vector<SerializerLineRecordId> lines;
    };

    Frame make_frame(SerializerPrefixScope scope) {
        Frame frame;
        frame.prefix_scope = scope.source != nullptr;
        frame.scope = std::move(scope);
        frame.current_line = plan_.new_line_id();
        return frame;
    }

    void add_source_anchor_boundaries(
        SerializerEvent const& event,
        std::size_t source_begin,
        std::size_t source_end,
        SerializerLineId line) {
        for (std::size_t source_offset = source_begin; source_offset <= source_end; ++source_offset) {
            const SerializerAnchorKey key{
                event.owner.v, ContainerAnchorPart::Source, source_offset};
            if (requested_anchors_.contains(key)) {
                anchor_lines_.insert_first({event.owner, ContainerAnchorPart::Source,
                    source_offset}, line);
            }
        }
    }

    void append_source(SerializerEventId event_id, SerializerEvent const& event) {
        if (event.source == nullptr) return;
        if (event.map_rule == SerializerMapRule::AtomicEndpoints) {
            stack_.back().content = plan_.concat(stack_.back().content,
                plan_.append_atomic_boundary(event.owner, false));
        }
        const auto begin = (std::min)(event.source_begin, event.source->size());
        const auto end = (std::clamp)(event.source_end, begin, event.source->size());
        std::size_t cursor = begin;
        while (cursor < end) {
            std::size_t eol = cursor;
            while (eol < end && (*event.source)[eol] != U'\r' && (*event.source)[eol] != U'\n') ++eol;
            add_source_anchor_boundaries(event, cursor, eol, stack_.back().current_line);
            if (eol > cursor) {
                const auto rope = plan_.append_source(event_id, *event.source, cursor, eol);
                stack_.back().content = plan_.concat(stack_.back().content, rope);
                stack_.back().total_codepoints += eol - cursor;
            }
            if (eol == end) {
                cursor = end;
                break;
            }
            std::size_t ending_end = eol + 1;
            if ((*event.source)[eol] == U'\r' && ending_end < end && (*event.source)[ending_end] == U'\n') {
                ++ending_end;
            }
            add_source_anchor_boundaries(event, eol, ending_end - 1,
                stack_.back().current_line);
            const auto ending = plan_.append_source(
                event_id, *event.source, eol, ending_end, false);
            stack_.back().total_codepoints += ending_end - eol;
            finish_line(stack_.back(), ending);
            cursor = ending_end;
            // The source boundary after an EOL belongs to the next logical
            // line. Leave it on that line so a later prefix is emitted before
            // its serialized position, matching prefix projection semantics.
            const auto next_line_boundary = plan_.append_source(
                event_id, *event.source, cursor, cursor);
            stack_.back().content = plan_.concat(stack_.back().content, next_line_boundary);
        }
        if (begin == end) {
            add_source_anchor_boundaries(event, begin, begin, stack_.back().current_line);
            const auto empty_source = plan_.append_source(event_id, *event.source, begin, end);
            stack_.back().content = plan_.concat(stack_.back().content, empty_source);
        } else add_source_anchor_boundaries(event, end, end, stack_.back().current_line);
        if (event.map_rule == SerializerMapRule::AtomicEndpoints) {
            stack_.back().content = plan_.concat(stack_.back().content,
                plan_.append_atomic_boundary(event.owner, true));
        }
    }

    void append_generated(SerializerEventId event_id, std::u32string const& value) {
        const auto& event = writer_.events()[event_id];
        if (event.map_rule == SerializerMapRule::AtomicEndpoints) {
            stack_.back().content = plan_.concat(stack_.back().content,
                plan_.append_atomic_boundary(event.owner, false));
        }
        std::size_t cursor = 0;
        while (cursor < value.size()) {
            std::size_t eol = cursor;
            while (eol < value.size() && value[eol] != U'\r' && value[eol] != U'\n') ++eol;
            if (eol > cursor) {
                const auto rope = plan_.append_generated(event_id, value, cursor, eol);
                stack_.back().content = plan_.concat(stack_.back().content, rope);
                stack_.back().total_codepoints += eol - cursor;
            }
            if (eol == value.size()) break;
            std::size_t ending_end = eol + 1;
            if (value[eol] == U'\r' && ending_end < value.size() && value[ending_end] == U'\n') ++ending_end;
            const auto ending = plan_.append_generated(event_id, value, eol, ending_end);
            stack_.back().total_codepoints += ending_end - eol;
            finish_line(stack_.back(), ending);
            cursor = ending_end;
        }
        if (event.map_rule == SerializerMapRule::AtomicEndpoints) {
            stack_.back().content = plan_.concat(stack_.back().content,
                plan_.append_atomic_boundary(event.owner, true));
        }
    }

    void attach_anchor(SerializerEventId event_id, ContainerPrefixAnchor anchor) {
        anchor_lines_.insert_first(anchor, stack_.back().current_line);
        (void)event_id;
        const auto rope = plan_.append_anchor_value(anchor);
        stack_.back().content = plan_.concat(stack_.back().content, rope);
    }

    void attach_anchor_value(ContainerPrefixAnchor anchor) {
        anchor_lines_.insert_first(anchor, stack_.back().current_line);
        const auto rope = plan_.append_anchor_value(anchor);
        stack_.back().content = plan_.concat(stack_.back().content, rope);
    }

    void attach_anchor_before_ending(ContainerPrefixAnchor anchor) {
        if (!stack_.back().lines.empty()) {
            const auto last_record = stack_.back().lines.back();
            const auto& last = plan_.line(last_record);
            if (last.terminal && (stack_.back().content == no_serializer_rope
                || plan_.rope(stack_.back().content).codepoint_count == 0)) {
                anchor_lines_.insert_first(anchor, last.id);
                const auto anchor_rope = plan_.append_anchor_value(anchor);
                const auto content = plan_.concat(last.content, anchor_rope);
                stack_.back().lines.back() = plan_.append_line(content, last.ending, last.id);
                return;
            }
        }
        attach_anchor_value(anchor);
    }

    void finish_line(Frame& frame, SerializerRopeId ending = no_serializer_rope) {
        const bool empty_content = frame.content == no_serializer_rope
            || plan_.rope(frame.content).codepoint_count == 0;
        if (ending != no_serializer_rope && empty_content && !frame.lines.empty()) {
            const auto previous = plan_.line(frame.lines.back());
            const auto& ending_facts = plan_.rope(ending).facts;
            const bool previous_is_lone_cr = previous.terminal && !previous.terminal_lf
                && previous.ending != no_serializer_rope
                && plan_.rope(previous.ending).codepoint_count == 1;
            const bool current_is_lone_lf = ending_facts.terminal_lf
                && plan_.rope(ending).codepoint_count == 1;
            if (previous_is_lone_cr && current_is_lone_lf) {
                auto merged_ending = plan_.concat(previous.ending, frame.content);
                merged_ending = plan_.concat(merged_ending, ending);
                frame.lines.back() = plan_.append_line(
                    previous.content, merged_ending, previous.id);
                anchor_lines_.alias(frame.current_line, previous.id);
                frame.content = no_serializer_rope;
                frame.current_line = plan_.new_line_id();
                return;
            }
        }
        frame.lines.push_back(plan_.append_line(frame.content, ending, frame.current_line));
        frame.content = no_serializer_rope;
        frame.current_line = plan_.new_line_id();
    }

    void append_child_line(Frame& parent, SerializerPlannedLine const& child) {
        anchor_lines_.alias(parent.current_line, child.id);
        parent.current_line = child.id;
        parent.content = plan_.concat(parent.content, child.content);
        if (child.content != no_serializer_rope) {
            parent.total_codepoints += plan_.rope(child.content).codepoint_count;
        }
        if (child.ending != no_serializer_rope) {
            parent.total_codepoints += plan_.rope(child.ending).codepoint_count;
        }
        if (child.ending != no_serializer_rope) {
            finish_line(parent, child.ending);
        }
    }

    void complete_block(NodeId id) {
        const auto found = block_starts_.find(id.v);
        const auto start = found == block_starts_.end() ? stack_.back().total_codepoints : found->second;
        const bool emitted = stack_.back().total_codepoints > start;
        const bool current_line_empty = stack_.back().content == no_serializer_rope
            || plan_.rope(stack_.back().content).codepoint_count == 0;
        const bool terminal = emitted && current_line_empty
            && !stack_.back().lines.empty() && plan_.line(stack_.back().lines.back()).terminal_lf;
        completed_terminal_.insert_or_assign(id.v, terminal);
    }

    void append_separator(SerializerEvent const& event) {
        std::u32string separator;
        if (event.has_explicit_separator) {
            separator = event.separator;
        } else if (event.list_item_context) {
            separator = event.next_is_list ? U"\n" : U"\n\n";
        } else if (event.previous_empty_paragraph) {
            separator = U"\n";
        } else {
            const auto found = completed_terminal_.find(event.previous_block.v);
            separator = found != completed_terminal_.end() && found->second ? U"\n" : U"\n\n";
        }
        const auto token = plan_.store_generated_token(std::move(separator));
        const auto& value = plan_.generated(token);
        attach_anchor_value({event.owner, ContainerAnchorPart::Separator, 0});
        std::size_t cursor = 0;
        while (cursor < value.size()) {
            std::size_t eol = cursor;
            while (eol < value.size() && value[eol] != U'\r' && value[eol] != U'\n') ++eol;
            if (eol > cursor) {
                const auto rope = plan_.append_stored_generated(token, cursor, eol);
                stack_.back().content = plan_.concat(stack_.back().content, rope);
                stack_.back().total_codepoints += eol - cursor;
            }
            if (eol == value.size()) break;
            std::size_t ending_end = eol + 1;
            if (value[eol] == U'\r' && ending_end < value.size() && value[ending_end] == U'\n') ++ending_end;
            const auto ending = plan_.append_stored_generated(token, eol, ending_end);
            stack_.back().total_codepoints += ending_end - eol;
            finish_line(stack_.back(), ending);
            cursor = ending_end;
            attach_anchor_value({event.owner, ContainerAnchorPart::Separator, cursor});
        }
    }

    void close_scope(NodeId id) {
        if (stack_.size() <= 1 || !stack_.back().prefix_scope || stack_.back().scope.scope_id != id) {
            throw std::logic_error("serializer scope close does not match open scope");
        }
        finish_line(stack_.back());
        const bool suppress_empty = stack_.back().scope.policy.suppress_if_empty_body
            && stack_.back().total_codepoints == 0;
        auto child_lines = std::move(stack_.back().lines);
        const auto scope = stack_.back().scope;
        stack_.pop_back();
        if (suppress_empty) return;

        SerializerScopePrefixIndex scope_anchors;
        scope_anchors.build(*scope.source, anchor_lines_);
        const bool terminal_newline = child_lines.size() > 1
            && plan_.line(child_lines.back()).facts.empty
            && plan_.line(child_lines[child_lines.size() - 2]).terminal;
        auto policy = scope.policy;
        policy.terminal_newline = terminal_newline;
        const auto terminal_line = child_lines.empty() ? std::size_t{} : child_lines.size() - 1;
        std::vector<SerializerLineRecordId> prefixed_lines;
        for (std::size_t index = 0; index < child_lines.size(); ++index) {
            const auto& input = plan_.line(child_lines[index]);
            const auto* explicit_prefix = scope_anchors.find(anchor_lines_.canonicalize(input.id));
            auto prefix = choose_serializer_prefix(policy, explicit_prefix,
                input.facts, index == 0,
                terminal_newline && index == terminal_line);
            auto pieces = plan_.prepend_prefix(child_lines[index], input, std::move(prefix));
            prefixed_lines.insert(prefixed_lines.end(), pieces.begin(), pieces.end());
        }
        for (const auto line_id : prefixed_lines) {
            append_child_line(stack_.back(), plan_.line(line_id));
        }
    }

    SerializerEventWriter& writer_;
    SerializerPlanArena& plan_;
    SerializerAnchorLineIndex anchor_lines_;
    std::unordered_set<SerializerAnchorKey, SerializerAnchorKeyHash> requested_anchors_;
    std::unordered_map<std::uint64_t, std::size_t> block_starts_;
    std::unordered_map<std::uint64_t, bool> completed_terminal_;
    std::unordered_map<std::uint64_t, std::size_t> outer_starts_;
    std::vector<SerializerBlockRange> block_ranges_;
    std::vector<Frame> stack_;
};

struct SerializerEmittedAnchor {
    ContainerPrefixAnchor anchor{};
    std::size_t serialized_offset = 0;
};

struct SerializerEmittedMap {
    NodeId owner{};
    std::vector<std::size_t> source_to_serialized;
};

struct SerializerEmission {
    std::u32string text;
    std::vector<SerializerEmittedMap> source_maps;
    std::vector<SerializerEmittedAnchor> syntax_anchors;
    std::vector<SerializerBlockRange> blocks;
};

// Final output traversal. Rope edges are numeric IDs and the traversal stack is
// explicit, so neither output nor cleanup recurses through block depth.
inline SerializerEmission emit_serializer_lines(
    SerializerPlanArena const& plan,
    std::span<const SerializerLineRecordId> output_lines,
    std::span<const SerializerEvent> events) {
    SerializerEmission result;
    SerializerOwnerMapIndex owner_maps;
    struct PendingRope { SerializerRopeId id; };
    std::vector<PendingRope> pending;

    auto emit_rope = [&](SerializerRopeId root) {
        if (root == no_serializer_rope) return;
        pending.push_back({root});
        while (!pending.empty()) {
            const auto id = pending.back().id;
            pending.pop_back();
            const auto& node = plan.rope(id);
            if (node.kind == SerializerRopeKind::Concat) {
                pending.push_back({node.right});
                pending.push_back({node.left});
                continue;
            }
            if (node.kind == SerializerRopeKind::OwnedGeneratedSpan) {
                const auto& value = plan.generated(node.event);
                const auto begin = (std::min)(node.text_begin, value.size());
                const auto end = (std::clamp)(node.text_end, begin, value.size());
                result.text.append(value, begin, end - begin);
                continue;
            }
            if (node.kind == SerializerRopeKind::SyntaxAnchor) {
                result.syntax_anchors.push_back({node.anchor, result.text.size()});
                continue;
            }
            if (node.kind == SerializerRopeKind::AtomicMapBoundary) {
                owner_maps.atomic_boundary(node.map_owner, node.map_end_boundary, result.text.size());
                continue;
            }
            if (node.event >= events.size()) continue;
            const auto& event = events[node.event];
            if (node.kind == SerializerRopeKind::GeneratedSpan) {
                const auto begin = (std::min)(node.text_begin, event.generated.size());
                const auto end = (std::clamp)(node.text_end, begin, event.generated.size());
                result.text.append(event.generated, begin, end - begin);
                continue;
            }
            if (node.kind != SerializerRopeKind::SourceSpan || event.source == nullptr) continue;

            const auto begin = (std::min)(node.text_begin, event.source->size());
            const auto end = (std::clamp)(node.text_end, begin, event.source->size());
            if (event.map_rule == SerializerMapRule::AtomicEndpoints) {
                result.text.append(*event.source, begin, end - begin);
            } else {
                for (std::size_t source_offset = begin; source_offset <= end; ++source_offset) {
                    if (source_offset == end && !node.include_source_end_boundary) break;
                    owner_maps.first_mapping(event.owner, event.full_source_size,
                        source_offset, result.text.size() + (source_offset - begin));
                }
                result.text.append(*event.source, begin, end - begin);
            }
        }
    };

    for (const auto line_id : output_lines) {
        const auto& line = plan.line(line_id);
        emit_rope(line.content);
        emit_rope(line.ending);
    }
    result.source_maps.reserve(owner_maps.maps().size());
    for (const auto& map : owner_maps.maps()) {
        result.source_maps.push_back({map.owner, map.source_to_serialized});
    }
    return result;
}

} // namespace folia::serializer_detail
