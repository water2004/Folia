// Parser-private transient physical-source helpers. No editing positions or
// document content are owned by this module.
export module folia.markdown.parser_source_session;
import std;
import folia.markdown.block_source;
import folia.markdown.container_source;
import folia.markdown.ids;

export namespace folia::parser_source_detail {

struct PhysicalSourceRange {
    std::size_t begin = 0;
    std::size_t end = 0;
    bool empty() const { return begin == end; }
    std::size_t size() const { return end - begin; }
};

struct PhysicalLine {
    std::size_t begin = 0;
    std::size_t content_end = 0;
    std::size_t ending_end = 0;
};

// The cursor names only original source code points and physical indentation
// state. Residual tab columns are interpretation context, not generated text.
struct PhysicalLineCursor {
    std::size_t line = 0;
    std::size_t offset = 0;
    std::size_t physical_column = 0;
    std::size_t residual_columns = 0;
    bool lazy = false;
};

class RootPhysicalSource {
    std::u32string_view source_;
    std::vector<PhysicalLine> lines_;
public:
    explicit RootPhysicalSource(std::u32string_view source) : source_(source) {
        std::size_t begin = 0;
        while (begin < source.size()) {
            auto end = begin;
            while (end < source.size() && source[end] != U'\r' && source[end] != U'\n') ++end;
            auto ending_end = end;
            if (ending_end < source.size()) {
                ++ending_end;
                if (source[end] == U'\r' && ending_end < source.size() && source[ending_end] == U'\n') ++ending_end;
            }
            lines_.push_back({begin, end, ending_end});
            begin = ending_end;
        }
        // The EOF line gives empty owners and trailing empty content one exact
        // physical point. Its source interval is empty; it invents no newline.
        if (lines_.empty() || lines_.back().ending_end > lines_.back().content_end)
            lines_.push_back({source.size(), source.size(), source.size()});
    }
    std::u32string_view source() const { return source_; }
    std::span<const PhysicalLine> lines() const { return lines_; }
    const PhysicalLine& line(std::size_t index) const { return lines_.at(index); }
    std::size_t line_at(std::size_t offset) const {
        if (offset > source_.size()) throw std::out_of_range("physical source offset");
        const auto next = std::ranges::upper_bound(lines_, offset, {}, &PhysicalLine::begin);
        return static_cast<std::size_t>(next - lines_.begin() - 1);
    }
    PhysicalLineCursor begin_line(std::size_t index, std::size_t physical_column = 0,
            std::size_t residual_columns = 0, bool lazy = false) const {
        return {index, line(index).begin, physical_column, residual_columns, lazy};
    }
    std::u32string_view text(PhysicalSourceRange range) const {
        if (range.end < range.begin || range.end > source_.size()) throw std::out_of_range("physical source range");
        return source_.substr(range.begin, range.size());
    }
    char32_t peek(PhysicalLineCursor const& cursor) const {
        return cursor.offset < line(cursor.line).content_end ? source_[cursor.offset] : char32_t{};
    }
    bool advance(PhysicalLineCursor& cursor) const {
        if (cursor.offset >= line(cursor.line).content_end) return false;
        const auto ch = source_[cursor.offset++];
        cursor.physical_column += ch == U'\t' ? 4 - cursor.physical_column % 4 : 1;
        cursor.residual_columns = 0;
        return true;
    }
    BlockIndentation indentation(PhysicalLineCursor const& cursor,
            std::size_t maximum_columns = (std::numeric_limits<std::size_t>::max)()) const {
        return scan_block_indentation(source_, cursor.offset, maximum_columns,
            cursor.physical_column, cursor.residual_columns);
    }
    void consume_indentation(PhysicalLineCursor& cursor, BlockIndentation const& indentation) const {
        if (indentation.end < cursor.offset || indentation.end > line(cursor.line).content_end)
            throw std::out_of_range("physical indentation range");
        cursor.offset = indentation.end;
        cursor.physical_column = indentation.physical_column;
        cursor.residual_columns = indentation.residual_columns;
    }
};

// Only an actual leaf collects these spans. Container frames retain cursors
// and their own prefix spans, never a piece list of the whole remaining body.
struct LeafSourcePiece {
    PhysicalSourceRange physical;
    std::size_t source_begin = 0;
};

class LeafSourceView {
    const RootPhysicalSource* root_ = nullptr;
    std::vector<LeafSourcePiece> pieces_;
    std::size_t size_ = 0;
    std::optional<std::size_t> empty_physical_offset_;
public:
    explicit LeafSourceView(const RootPhysicalSource& root) : root_(&root) {}
    std::size_t size() const { return size_; }
    std::span<const LeafSourcePiece> pieces() const { return pieces_; }
    void append(PhysicalSourceRange physical) {
        (void)root_->text(physical); // validate before changing state
        if (physical.empty()) {
            if (pieces_.empty()) empty_physical_offset_ = physical.begin;
            return;
        }
        if (!pieces_.empty() && pieces_.back().physical.end == physical.begin) {
            pieces_.back().physical.end = physical.end;
        } else pieces_.push_back({physical, size_});
        size_ += physical.size();
    }
    // The sole source materialization point for this leaf. Its result is moved
    // into the existing InlineDocument/BlockSourceDocument, never into a frame.
    std::u32string materialize() const {
        std::u32string source;
        source.reserve(size_);
        for (const auto& piece : pieces_) source.append(root_->text(piece.physical));
        return source;
    }
    std::optional<std::size_t> physical_offset(std::size_t source_offset) const {
        if (source_offset > size_) return std::nullopt;
        if (pieces_.empty()) return empty_physical_offset_;
        if (source_offset == size_) return pieces_.back().physical.end;
        const auto next = std::ranges::upper_bound(pieces_, source_offset, {}, &LeafSourcePiece::source_begin);
        const auto& piece = *std::prev(next);
        return piece.physical.begin + source_offset - piece.source_begin;
    }
    std::optional<PhysicalSourceRange> physical_range(std::size_t begin,std::size_t end) const {
        if (end<begin || end>size_) return std::nullopt;
        const auto first=physical_offset(begin);
        if (!first) return std::nullopt;
        if (begin==end) return PhysicalSourceRange{*first,*first};
        auto last=physical_offset(end);
        if (!last) return std::nullopt;
        const auto next=std::ranges::lower_bound(pieces_,end,{},&LeafSourcePiece::source_begin);
        if (next!=pieces_.end() && next->source_begin==end && next!=pieces_.begin())
            last=std::prev(next)->physical.end;
        return PhysicalSourceRange{*first,*last};
    }

};

// Recorded by a container frame, then resolved once against completed leaf
// ownership. The anchor is the existing ContainerPrefixAnchor; no new durable
// coordinate or body content belongs to the frame/session.
struct PendingContainerPrefix {
    std::size_t physical_line = 0;
    std::vector<PhysicalSourceRange> spelling;
    std::size_t content_physical_offset = 0;
    bool lazy = false;
    ContainerPrefixRole role = ContainerPrefixRole::Literal;
    // Binding to the final leaf does not replace per-frame syntax context.
    std::optional<std::size_t> binding_physical_offset;
};

struct OwnedSourcePiece {
    NodeId owner_id{};
    LeafSourcePiece piece;
};

// Source ownership is registered once after a real leaf is completed. Sealing
// the index overlays physical spans by original registration order (the same
// tie-break as bind_prefix_lines), without a per-character global map.
class SourceOwnershipIndex {
    struct Piece {
        PhysicalSourceRange physical;
        ContainerPrefixAnchor anchor;
        std::size_t order;
    };
    struct Segment {
        PhysicalSourceRange physical;
        ContainerPrefixAnchor anchor;
        std::size_t order;
    };
    struct EmptyPoint { std::size_t physical; ContainerPrefixAnchor anchor; std::size_t order; };
    struct SyntaxRange { PhysicalSourceRange physical; NodeId owner; std::size_t order; };
    struct RangeNode {
        std::size_t begin = 0, end = 0;
        std::size_t maximum_end = 0, minimum_width = 0, minimum_order = 0;
        std::size_t left = 0, right = 0;
    };
    std::vector<Piece> source_pieces_;
    std::vector<Piece> separator_pieces_;
    std::vector<Segment> sources_, separators_;
    std::vector<EmptyPoint> empty_points_;
    std::vector<SyntaxRange> syntax_ranges_;
    std::vector<RangeNode> range_nodes_;
    std::size_t next_order_ = 0;
    bool sealed_ = false;

    static std::vector<Segment> overlay(std::vector<Piece> const& pieces) {
        struct Event { std::size_t at, piece; bool begin; };
        std::vector<Event> events;
        events.reserve(pieces.size()*2);
        for (std::size_t i=0; i<pieces.size(); ++i) {
            events.push_back({pieces[i].physical.begin,i,true});
            events.push_back({pieces[i].physical.end,i,false});
        }
        std::ranges::sort(events, {}, &Event::at);
        std::vector<bool> active(pieces.size());
        auto later = [&](std::size_t a, std::size_t b) { return pieces[a].order > pieces[b].order; };
        std::priority_queue<std::size_t, std::vector<std::size_t>, decltype(later)> queue(later);
        std::vector<Segment> result;
        for (std::size_t index=0; index<events.size();) {
            const auto at=events[index].at;
            while (index<events.size() && events[index].at==at) {
                const auto event=events[index++];
                active[event.piece]=event.begin;
                if (event.begin) queue.push(event.piece);
            }
            while (!queue.empty() && !active[queue.top()]) queue.pop();
            if (index==events.size() || queue.empty() || events[index].at==at) continue;
            auto const& piece=pieces[queue.top()];
            auto anchor=piece.anchor;
            anchor.offset += at-piece.physical.begin;
            const auto end=events[index].at;
            if (!result.empty() && result.back().physical.end==at
                    && result.back().order==piece.order && result.back().anchor.owner_id==anchor.owner_id
                    && result.back().anchor.part==anchor.part
                    && result.back().anchor.offset+result.back().physical.size()==anchor.offset)
                result.back().physical.end=end;
            else result.push_back({{at,end},anchor,piece.order});
        }
        return result;
    }
    static std::optional<std::pair<ContainerPrefixAnchor,std::size_t>> source_candidate(
            std::vector<Segment> const& segments, std::size_t from, std::size_t line_end) {
        auto next=std::ranges::upper_bound(segments,from,{},[](Segment const& s){return s.physical.begin;});
        if (next!=segments.begin()) {
            auto previous=std::prev(next);
            if (from<previous->physical.end) next=previous;
        }
        if (next==segments.end()) return std::nullopt;
        const auto physical=(std::max)(from,next->physical.begin);
        if (physical>line_end) return std::nullopt;
        auto anchor=next->anchor; anchor.offset += physical-next->physical.begin;
        return std::pair{anchor,next->order};
    }
    std::size_t build_range_node(std::size_t begin, std::size_t end) {
        const auto id=range_nodes_.size(); range_nodes_.push_back({});
        RangeNode node; node.begin=begin; node.end=end;
        node.minimum_width=(std::numeric_limits<std::size_t>::max)();
        node.minimum_order=node.minimum_width;
        if (end-begin==1) {
            auto const& range=syntax_ranges_[begin];
            node.maximum_end=range.physical.end;
            node.minimum_width=range.physical.size(); node.minimum_order=range.order;
        } else {
            const auto middle=begin+(end-begin)/2;
            node.left=build_range_node(begin,middle); node.right=build_range_node(middle,end);
            auto const& a=range_nodes_[node.left]; auto const& b=range_nodes_[node.right];
            node.maximum_end=(std::max)(a.maximum_end,b.maximum_end);
            node.minimum_width=(std::min)(a.minimum_width,b.minimum_width);
            node.minimum_order=(std::min)(a.minimum_order,b.minimum_order);
        }
        range_nodes_[id]=node; return id;
    }
    std::optional<ContainerPrefixAnchor> syntax_candidate(std::size_t from,std::size_t line_end) const {
        if (range_nodes_.empty()) return std::nullopt;
        struct Candidate { std::size_t node, width, order; };
        auto later=[](Candidate const& a,Candidate const& b){return std::pair{a.width,a.order}>std::pair{b.width,b.order};};
        std::priority_queue<Candidate,std::vector<Candidate>,decltype(later)> pending(later);
        pending.push({0,range_nodes_[0].minimum_width,range_nodes_[0].minimum_order});
        std::optional<ContainerPrefixAnchor> best;
        auto best_key=std::pair{(std::numeric_limits<std::size_t>::max)(),(std::numeric_limits<std::size_t>::max)()};
        while (!pending.empty()) {
            const auto candidate=pending.top(); pending.pop();
            if (std::pair{candidate.width,candidate.order}>=best_key) continue;
            auto const& node=range_nodes_[candidate.node];
            if (node.maximum_end<from || syntax_ranges_[node.begin].physical.begin>line_end) continue;
            if (node.end-node.begin==1) {
                auto const& range=syntax_ranges_[node.begin];
                best_key={range.physical.size(),range.order};
                best=ContainerPrefixAnchor{range.owner,range.physical.begin>=from ? ContainerAnchorPart::Start : ContainerAnchorPart::End,0};
            } else {
                for (const auto child : {node.left,node.right}) {
                    auto const& entry=range_nodes_[child];
                    pending.push({child,entry.minimum_width,entry.minimum_order});
                }
            }
        }
        return best;
    }
    void require_open() const { if (sealed_) throw std::logic_error("source ownership index already sealed"); }
public:
    void add_source(NodeId owner,LeafSourcePiece const& piece) {
        require_open();
        if (piece.physical.empty()) {
            empty_points_.push_back({piece.physical.begin,{owner,ContainerAnchorPart::Source,piece.source_begin},next_order_++});
        } else source_pieces_.push_back({piece.physical,{owner,ContainerAnchorPart::Source,piece.source_begin},next_order_++});
    }
    void add_separator(NodeId owner,PhysicalSourceRange physical,std::size_t source_offset=0) {
        require_open();
        if (!physical.empty()) separator_pieces_.push_back({physical,{owner,ContainerAnchorPart::Separator,source_offset},next_order_++});
    }
    void add_syntax_range(NodeId owner,PhysicalSourceRange physical) {
        require_open(); syntax_ranges_.push_back({physical,owner,next_order_++});
    }
    void seal() {
        require_open();
        sources_=overlay(source_pieces_); separators_=overlay(separator_pieces_);
        std::ranges::sort(empty_points_,[](auto const& a,auto const& b){return std::pair{a.physical,a.order}<std::pair{b.physical,b.order};});
        std::ranges::sort(syntax_ranges_,{},[](auto const& r){return r.physical.begin;});
        if (!syntax_ranges_.empty()) build_range_node(0,syntax_ranges_.size());
        sealed_=true;
    }
    std::optional<ContainerPrefixAnchor> resolve(std::size_t from,std::size_t line_end) const {
        if (!sealed_) throw std::logic_error("source ownership index not sealed");
        auto source=source_candidate(sources_,from,line_end);
        const auto empty=std::ranges::lower_bound(empty_points_,from,{},&EmptyPoint::physical);
        if (empty!=empty_points_.end() && empty->physical<=line_end) {
            std::size_t source_physical=(std::numeric_limits<std::size_t>::max)();
            if (source) {
                auto interval=std::ranges::upper_bound(sources_,from,{},[](auto const& s){return s.physical.begin;});
                if (interval!=sources_.begin() && from<std::prev(interval)->physical.end) --interval;
                source_physical=(std::max)(from,interval->physical.begin);
            }
            if (!source || empty->physical<source_physical || (empty->physical==source_physical && empty->order<source->second))
                source=std::pair{empty->anchor,empty->order};
        }
        if (source) return source->first;
        // Unlike source anchors, a Separator anchor must contain the line's
        // content start; a later separator does not own that line.
        auto separator=std::ranges::upper_bound(separators_,from,{},[](auto const& s){return s.physical.begin;});
        if (separator!=separators_.begin()) {
            --separator;
            if (from<separator->physical.end) {
                auto anchor=separator->anchor; anchor.offset+=from-separator->physical.begin; return anchor;
            }
        }
        return syntax_candidate(from,line_end);
    }
    ContainerSource bind(RootPhysicalSource const& root,std::span<const PendingContainerPrefix> prefixes) const {
        ContainerSource result;
        result.lines.reserve(prefixes.size());
        for (auto const& prefix : prefixes) {
            const auto physical=prefix.binding_physical_offset.value_or(prefix.content_physical_offset);
            const auto anchor=resolve(physical,root.line(prefix.physical_line).content_end);
            if (!anchor) continue; // same unsupported/unbound-line behavior as the existing binder
            std::u32string spelling;
            for (const auto range : prefix.spelling) spelling.append(root.text(range));
            result.lines.push_back({*anchor,std::move(spelling),prefix.lazy,prefix.role});
        }
        return result;
    }
};

// Prefix exclusions are parse-time physical intervals. A sibling gap can be
// extracted without copying a decontainerized parent body or walking ancestry.
class ContainerPrefixExclusions {
    std::map<std::size_t,std::size_t> intervals_;
public:
    void add(PhysicalSourceRange range) {
        if (range.end<range.begin) throw std::out_of_range("prefix exclusion range");
        if (range.empty()) return;
        auto next=intervals_.lower_bound(range.begin);
        if (next!=intervals_.begin() && std::prev(next)->second>=range.begin) --next;
        while (next!=intervals_.end() && next->first<=range.end) {
            if (next->second<range.begin) { ++next; continue; }
            range.begin=(std::min)(range.begin,next->first);
            range.end=(std::max)(range.end,next->second);
            next=intervals_.erase(next);
        }
        intervals_.emplace(range.begin,range.end);
    }
    std::vector<PhysicalSourceRange> retained(PhysicalSourceRange range) const {
        if (range.end<range.begin) throw std::out_of_range("scope gap range");
        std::vector<PhysicalSourceRange> result;
        auto next=intervals_.upper_bound(range.begin);
        if (next!=intervals_.begin() && std::prev(next)->second>range.begin) --next;
        auto position=range.begin;
        for (;next!=intervals_.end() && next->first<range.end;++next) {
            if (next->second<=position) continue;
            if (next->first>position) result.push_back({position,(std::min)(range.end,next->first)});
            position=(std::max)(position,next->second);
            if (position>=range.end) break;
        }
        if (position<range.end) result.push_back({position,range.end});
        return result;
    }
};

}
