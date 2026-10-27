// Logical leaf lines and source accumulation over the shared physical
// source vocabulary. This file contains no Markdown recognition rules.
export module folia.markdown.parser_leaf_line_view;
import std;
import folia.markdown.parser_source_session;

export namespace folia::parser_source_detail {

// Parse-session identity only. This is neither a NodeId nor an editing or
// durable container coordinate; it lets a staged prefix name its frame even
// when a list frame has no physical prefix on the current line.
using ContainerFrameHandle = std::size_t;

struct StagedContainerPrefix {
    ContainerFrameHandle frame_handle = 0;
    PendingContainerPrefix prefix;
};

// One line as seen by a leaf after the active container frames have offered
// their prefixes. Content stays in root physical coordinates; an item task
// prefix or retained overpadding can make it discontiguous.
struct LeafLine {
    PhysicalLineCursor content_cursor;
    std::vector<PhysicalSourceRange> content_spans;
    PhysicalSourceRange ending;
    std::vector<StagedContainerPrefix> staged_prefixes;
    std::size_t active_container_frames = 0;
    std::size_t matched_container_frames = 0;
    bool lazy_candidate = false;
    bool at_logical_line_start = true;
    // Staged syntax context before lazy continuation strips indentation.
    // This is the same root physical cursor, never an editing coordinate.
    std::optional<PhysicalLineCursor> lazy_origin_cursor;

    std::size_t content_size() const {
        std::size_t size = 0;
        for (const auto span : content_spans) size += span.size();
        return size;
    }
};

// Read-only logical view of just this line. No line copy, subtree copy, or
// parent-view offset chain is created. Source offsets returned by physical()
// are root source code-point offsets.
class LeafLineText {
    const RootPhysicalSource* root_ = nullptr;
    const LeafLine* line_ = nullptr;
public:
    LeafLineText(const RootPhysicalSource& root, const LeafLine& line)
        : root_(&root), line_(&line) {}

    std::size_t size() const { return line_->content_size(); }

    char32_t at(std::size_t offset) const {
        for (const auto span : line_->content_spans) {
            if (offset < span.size()) return root_->source()[span.begin + offset];
            offset -= span.size();
        }
        return 0;
    }

    std::optional<std::size_t> physical(std::size_t offset) const {
        if (offset > size()) return std::nullopt;
        auto remaining = offset;
        for (std::size_t index = 0; index < line_->content_spans.size(); ++index) {
            const auto span = line_->content_spans[index];
            if (remaining < span.size()) return span.begin + remaining;
            if (remaining == span.size()) {
                // A boundary at a removed container marker maps to the next
                // content character, so later source facts start after it.
                if (index + 1 < line_->content_spans.size())
                    return line_->content_spans[index + 1].begin;
                return span.end;
            }
            remaining -= span.size();
        }
        return line_->content_spans.empty()
            ? std::optional<std::size_t>(line_->content_cursor.offset)
            : std::nullopt;
    }
};

// The leaf recognizer decides whether the physical line ending belongs to its
// source. This is important for paragraph hard breaks, code blocks, tables,
// and exact CR/LF preservation.
class LeafSourceAccumulator {
    LeafSourceView source_;
    std::vector<std::pair<std::size_t, std::size_t>> empty_anchors_;
public:
    explicit LeafSourceAccumulator(const RootPhysicalSource& root) : source_(root) {}

    void append_content(const LeafLine& line) {
        for (const auto span : line.content_spans) append(span);
    }

    void append(PhysicalSourceRange physical) {
        if (physical.empty()) empty_anchors_.emplace_back(source_.size(), physical.begin);
        source_.append(physical);
    }

    // Append only a logical subrange of a de-prefixed content line. It walks
    // the small span list (normally one or two spans), never a per-character
    // mapping across a frame or the unconsumed document.
    void append_content_range(
        const LeafLine& line, std::size_t begin, std::size_t end) {
        if (end < begin || end > line.content_size())
            throw std::out_of_range("leaf content range");
        auto skip = begin;
        auto remaining = end - begin;
        bool appended = false;
        for (const auto span : line.content_spans) {
            if (skip >= span.size()) {
                skip -= span.size();
                continue;
            }
            const auto available = span.size() - skip;
            const auto take = (std::min)(available, remaining);
            source_.append({span.begin + skip, span.begin + skip + take});
            appended = true;
            remaining -= take;
            skip = 0;
            if (remaining == 0) break;
        }
        if (begin == end || !appended) {
            auto remaining_offset = begin;
            std::optional<std::size_t> at;
            for (std::size_t index = 0; index < line.content_spans.size(); ++index) {
                const auto span = line.content_spans[index];
                if (remaining_offset < span.size()) {
                    at = span.begin + remaining_offset;
                    break;
                }
                remaining_offset -= span.size();
                if (remaining_offset == 0) {
                    at = index + 1 < line.content_spans.size()
                        ? line.content_spans[index + 1].begin : span.end;
                    break;
                }
            }
            if (!at && line.content_spans.empty()) at = line.content_cursor.offset;
            if (!at) throw std::out_of_range("leaf content boundary");
            append({*at, *at});
        }
        if (remaining != 0) throw std::logic_error("leaf content spans incomplete");
    }

    void append_ending(const LeafLine& line) { append(line.ending); }

    void append_line(const LeafLine& line, bool include_ending) {
        append_content(line);
        if (include_ending) append_ending(line);
    }

    std::size_t size() const { return source_.size(); }
    std::span<const LeafSourcePiece> pieces() const { return source_.pieces(); }
    std::optional<std::size_t> physical_offset(std::size_t offset) const {
        if (offset > source_.size()) return std::nullopt;
        // A later nonempty append at this local boundary defines the
        // downstream position and must beat an earlier empty-owner anchor.
        if (offset < source_.size()) return source_.physical_offset(offset);
        for (auto anchor = empty_anchors_.rbegin(); anchor != empty_anchors_.rend(); ++anchor) {
            if (anchor->first == offset) return anchor->second;
        }
        return source_.physical_offset(offset);
    }
    std::optional<PhysicalSourceRange> physical_range(
        std::size_t begin, std::size_t end) const {
        if (begin == end) {
            const auto point = physical_offset(begin);
            if (!point) return std::nullopt;
            return PhysicalSourceRange{*point, *point};
        }
        return source_.physical_range(begin, end);
    }
    std::u32string materialize() const { return source_.materialize(); }
};

// Engine protocol: peek constructs a LeafLine and stages prefixes without
// changing the line cursor or any frame. Existing Parser leaf recognition
// evaluates that candidate (including its established lazy/interrupt rules);
// only an accepted line is passed to accept, which commits staged prefixes.
// A rejected candidate is left available to close the appropriate frames or
// to dispatch as the next top-level block.
}
