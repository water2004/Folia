// folia.markdown.inline_parser — lossless editable inline CST parser.
//
// Parses a content node's `InlineDocument::source` into an `InlineCstTree` such
// that `flatten_tokens(tree, source) == source` character-for-character. Every
// source character belongs to exactly one top-level node (and top-level ranges
// are contiguous and exhaustive over `source`).
//
// Unlike a semantic Markdown parser, this one is *lossless and editable*:
//   - Unclosed constructs (`**`, `` ` ``, `[title](`, `$abc`, `~~abc`) are kept
//     as Incomplete nodes spanning their opener + trailing content, NOT dropped
//     or collapsed to plain text. The opener source is preserved verbatim.
//   - Ambiguous delimiter runs are kept as Delimiter/Error nodes.
//   - Markers, escapes, entities, whitespace, link URL spellings and quote
//     forms are all preserved as source ranges.
//
// The parser is block-local: it sees only one node's inline source and never
// crosses a block boundary. Soft/hard breaks are within-node '\n'.
export module folia.markdown.inline_parser;
import std;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.image_dimension;
import folia.markdown.utf;
import folia.markdown.html_entities;
import folia.markdown.link_label;
import folia.markdown.markdown_html;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.gfm_autolink;

export namespace folia {

namespace inline_parser_detail {

struct Parser {
    std::u32string_view src;
    std::size_t pos = 0;
    std::size_t scope_start = 0;
    std::size_t limit = 0;
    InlineParseContext ctx;
    std::uint64_t counter;
    bool suppress_bare_autolinks = false;

    explicit Parser(std::u32string_view s, const InlineParseContext& c)
        : src(s), limit(s.size()), ctx(c), counter(c.next_id.v) {}

    NodeId next_id() { return ctx.allocate_id ? ctx.allocate_id() : NodeId(counter++); }
    std::size_t size() const { return limit; }
    bool eof() const { return pos >= limit; }
    char32_t peek(std::size_t k = 0) const { return pos + k < limit ? src[pos + k] : char32_t{0}; }
    char32_t ch(std::size_t i) const { return i < limit ? src[i] : char32_t{0}; }
    void advance(std::size_t n = 1) { pos = (std::min)(pos + n, limit); }

    static bool is_line_ending(char32_t value) {
        return value == U'\r' || value == U'\n';
    }

    std::size_t line_ending_length(std::size_t at) const {
        if (at >= limit || !is_line_ending(src[at])) return 0;
        return src[at] == U'\r' && at + 1 < limit && src[at + 1] == U'\n' ? 2u : 1u;
    }

    static bool ascii_punct(char32_t c) {
        return (c >= U'!' && c <= U'/') || (c >= U':' && c <= U'@') || (c >= U'[' && c <= U'`') || (c >= U'{' && c <= U'~');
    }

    static bool delimiter_punct(char32_t c) { return unicode_punctuation_or_symbol(c); }

    struct DelimiterFlanking {
        bool left = false;
        bool right = false;
        bool previous_punctuation = false;
        bool next_punctuation = false;
    };

    DelimiterFlanking delimiter_flanking(std::size_t at, std::size_t length) const {
        const auto previous_boundary = at == scope_start;
        const auto next_boundary = at + length >= limit;
        const auto previous = previous_boundary ? char32_t{} : ch(at - 1);
        const auto next = next_boundary ? char32_t{} : ch(at + length);
        const auto previous_whitespace = previous_boundary || is_commonmark_whitespace(previous);
        const auto next_whitespace = next_boundary || is_commonmark_whitespace(next);
        const auto previous_punctuation = !previous_boundary && delimiter_punct(previous);
        const auto next_punctuation = !next_boundary && delimiter_punct(next);
        return {
            .left = !next_whitespace && (!next_punctuation || previous_whitespace || previous_punctuation),
            .right = !previous_whitespace && (!previous_punctuation || next_whitespace || next_punctuation),
            .previous_punctuation = previous_punctuation,
            .next_punctuation = next_punctuation,
        };
    }

    bool underscore_can_open(std::size_t at, std::size_t length) const {
        const auto flanking = delimiter_flanking(at, length);
        return flanking.left && (!flanking.right || flanking.previous_punctuation);
    }

    bool underscore_can_close(std::size_t at, std::size_t length) const {
        const auto flanking = delimiter_flanking(at, length);
        return flanking.right && (!flanking.left || flanking.next_punctuation);
    }

    struct EmphasisPair {
        std::size_t open_start = 0;
        std::size_t open_end = 0;
        std::size_t close_start = 0;
        std::size_t close_end = 0;
        InlineCstKind kind = InlineCstKind::Emphasis;
    };

    struct EmphasisRun {
        char32_t marker = U'\0';
        std::size_t start = 0;
        std::size_t end = 0;
        std::size_t length = 0;
        bool can_open = false;
        bool can_close = false;
        std::vector<std::size_t> link_context;
        std::size_t open_remaining = 0;
        std::size_t close_consumed = 0;
        bool active_opener = false;
    };

    struct LinkLabelRange { std::size_t open = 0; std::size_t close = 0; };

    std::optional<std::vector<EmphasisPair>> emphasis_pairs_cache;
    std::vector<std::size_t> emphasis_opener_starts_cache;
    std::optional<EmphasisPair> pending_emphasis;
    // Last independent construct in this parser scope. Suffix queries must not
    // rescan the source for every unmatched delimiter run.
    std::optional<std::vector<std::size_t>> separate_structure_positions_cache;
    mutable std::optional<MarkdownHtmlSpecialIndex> markdown_html_special_index_cache;
    struct CodeRun { std::size_t start = 0; std::size_t end = 0; };
    struct CodeRunIndex {
        std::vector<CodeRun> runs;
        std::unordered_map<std::size_t, std::vector<std::size_t>> starts_by_length;
    };
    mutable std::optional<CodeRunIndex> code_runs_cache;
    struct StrikePair { std::size_t open = 0; std::size_t close = 0; std::size_t length = 0; };
    std::optional<std::vector<StrikePair>> strikethrough_pairs_cache;
    mutable std::optional<std::vector<std::size_t>> bracket_close_index_cache;
    mutable std::size_t bracket_index_scope_start = npos;
    mutable std::size_t bracket_index_limit = npos;
    struct NestedLinkRange { std::size_t start = 0; std::size_t end = 0; };
    std::optional<std::vector<NestedLinkRange>> nested_link_ranges_cache;
    std::size_t nested_link_index_scope_start = npos;
    std::size_t nested_link_index_limit = npos;
    mutable std::optional<std::vector<std::size_t>> unescaped_close_parens_cache;
    mutable std::size_t close_parens_scope_start = npos;
    mutable std::size_t close_parens_limit = npos;
    mutable std::optional<std::vector<std::size_t>> angle_stoppers_cache;
    mutable std::size_t angle_index_scope_start = npos;
    mutable std::size_t angle_index_limit = npos;

    bool starts_with_at(std::size_t at, std::u32string_view value) const {
        return at + value.size() <= limit && src.substr(at, value.size()) == value;
    }

    const MarkdownHtmlSpecialIndex& markdown_html_special_index() const {
        if (!markdown_html_special_index_cache) {
            markdown_html_special_index_cache.emplace(src);
        }
        return *markdown_html_special_index_cache;
    }

    std::optional<MarkdownHtmlSpecial> html_special_at(std::size_t at, std::size_t end) const {
        if (at + 1 >= end || src[at] != U'<'
            || (src[at + 1] != U'!' && src[at + 1] != U'?')) return std::nullopt;
        return scan_markdown_html_special(src, at, end, &markdown_html_special_index());
    }

    const std::vector<CodeRun>& code_runs() const {
        if (!code_runs_cache) {
            CodeRunIndex index;
            for (std::size_t at = 0; at < src.size();) {
                if (src[at] != U'`') { ++at; continue; }
                const auto start = at++;
                while (at < src.size() && src[at] == U'`') ++at;
                const auto run_index = index.runs.size();
                index.runs.push_back({start, at});
                index.starts_by_length[at - start].push_back(run_index);
            }
            code_runs_cache.emplace(std::move(index));
        }
        return code_runs_cache->runs;
    }

    const CodeRun* code_run_covering(std::size_t at) const {
        const auto& runs = code_runs();
        const auto after = std::upper_bound(runs.begin(), runs.end(), at,
            [](std::size_t value, const CodeRun& run) { return value < run.start; });
        if (after == runs.begin()) return nullptr;
        const auto& run = *std::prev(after);
        return run.start <= at && at < run.end ? &run : nullptr;
    }

    const CodeRun* next_code_run(std::size_t at, std::size_t end) const {
        const auto& runs = code_runs();
        const auto found = std::lower_bound(runs.begin(), runs.end(), at,
            [](const CodeRun& run, std::size_t value) { return run.start < value; });
        return found != runs.end() && found->start < end ? &*found : nullptr;
    }

    std::size_t exact_code_run_end(std::size_t after, std::size_t end, std::size_t length) const {
        if (length == 0 || after >= end) return npos;
        (void)code_runs();
        const auto& runs = code_runs_cache->runs;
        auto match = npos;
        const auto found_length = code_runs_cache->starts_by_length.find(length);
        if (found_length != code_runs_cache->starts_by_length.end()) {
            const auto& starts = found_length->second;
            const auto found = std::lower_bound(starts.begin(), starts.end(), after,
                [&](std::size_t run_index, std::size_t value) { return runs[run_index].start < value; });
            if (found != starts.end()) {
                const auto& run = runs[*found];
                if (run.start < end && run.end <= end) match = run.end;
            }
        }

        // The old scanner measured a candidate run only up to the active
        // parse limit. A scope can end in the middle of a source-wide run, so
        // consider that single clipped tail separately from full indexed runs.
        if (end > after) {
            if (const auto* tail = code_run_covering(end - 1);
                tail && tail->start >= after && tail->end > end
                && end - tail->start == length
                && (match == npos || tail->start + length < match)) {
                match = end;
            }
        }
        return match;
    }

    // Return the end of an opaque inline construct for delimiter collection.
    // This only shields source that the normal parser handles separately; it
    // does not assign Markdown meaning or produce CST nodes.
    std::size_t skip_emphasis_opaque(std::size_t at) const {
        if (at >= limit) return at;
        if (src[at] == U'\\' && at + 1 < limit
            && (ascii_punct(src[at + 1]) || is_line_ending(src[at + 1]))) {
            return at + 2;
        }
        if (src[at] == U'`') {
            auto run_end = at;
            while (run_end < limit && src[run_end] == U'`') ++run_end;
            // Match the production code-span scanner: exact-length runs may
            // close across blank lines; unmatched openers remain ordinary
            // delimiters and must not hide later emphasis.
            const auto close_end = matching_code_span_end(at, limit);
            if (close_end != npos) return close_end;
            return run_end;
        }
        if (src[at] == U'<') {
            if (const auto special = html_special_at(at, limit)) {
                return special->range.end;
            }
            if (const auto tag = html_tag_at(at, limit)) {
                if (inline_syntax_allows_markdown(ctx.syntax_mode)) return tag->end;
                if (tag->closing || (tag->name != "img" && tag->name != "br"
                        && !safe_html_container(tag->name))) return tag->end;
                if (tag->name == "img" || tag->name == "br" || tag->self_closing) return tag->end;
                if (const auto closing = html_closing_tag(tag->name, tag->end)) return closing->second;
                // parse_html_element emits an incomplete container through
                // the current line when its matching close is missing.
                return find_line_or_doc_end(tag->end);
            }
            const auto autolink_end = valid_autolink_end(at, limit);
            if (autolink_end != npos) return autolink_end;
        }
        return at;
    }

    // Skip only syntax that is opaque to Markdown delimiters. Inline HTML
    // bodies remain scannable; only tags/attributes and HTML special tokens
    // are skipped here.
    std::size_t skip_strikethrough_opaque(std::size_t at) const {
        if (at >= limit) return at;
        if (src[at] == U'\\' && at + 1 < limit
            && (ascii_punct(src[at + 1]) || is_line_ending(src[at + 1]))) return at + 2;
        if (src[at] == U'`') {
            const auto end = matching_code_span_end(at, limit);
            if (end != npos) return end;
            auto run_end = at;
            while (run_end < limit && src[run_end] == U'`') ++run_end;
            return run_end;
        }
        if (src[at] == U'<') {
            if (const auto special = html_special_at(at, limit)) return special->range.end;
            const auto tag_end = html_tag_end(at, limit);
            if (tag_end != npos) return tag_end;
            const auto autolink_end = valid_autolink_end(at, limit);
            if (autolink_end != npos) return autolink_end;
        }
        return at;
    }

    const std::vector<StrikePair>& strikethrough_pairs() {
        if (strikethrough_pairs_cache) return *strikethrough_pairs_cache;
        struct Run { std::size_t start; std::size_t length; std::size_t previous_same; };
        std::vector<Run> runs;
        std::vector<std::size_t> delimiter_stack;
        std::array<std::size_t, 3> last_open{npos, npos, npos};
        std::vector<StrikePair> pairs;
        std::unordered_set<std::size_t> inline_link_closes;
        for (const auto& label : emphasis_link_labels()) {
            if (label.close + 1 < limit && src[label.close + 1] == U'(') {
                inline_link_closes.insert(label.close);
            }
        }
        for (auto scan = scope_start; scan + 1 < limit; ++scan) {
            if (src[scan] != U'!' || src[scan + 1] != U'[') continue;
            const auto close = find_link_label_close(scan + 1);
            if (close != npos && close + 1 < limit && src[close + 1] == U'(') {
                inline_link_closes.insert(close);
            }
        }
        for (std::size_t scan = scope_start; scan < limit;) {
            if (is_line_ending(src[scan])) {
                const auto next_line = scan + line_ending_length(scan);
                if (next_line < limit && is_line_ending(src[next_line])) break;
            }
            const auto skipped = skip_strikethrough_opaque(scan);
            if (skipped != scan) { scan = skipped; continue; }
            if (src[scan] == U']' && inline_link_closes.contains(scan)) {
                std::size_t end = npos;
                std::string href;
                std::optional<std::string> title;
                if (parse_inline_link_suffix(scan + 1, limit, end, href, title)) {
                    scan = end;
                    continue;
                }
            }
            if (src[scan] != U'~') { ++scan; continue; }
            const auto start = scan;
            while (scan < limit && src[scan] == U'~') ++scan;
            const auto length = scan - start;
            if (length != 1 && length != 2) continue;
            const auto flank = delimiter_flanking(start, length);
            if (!flank.left && !flank.right) continue;
            if (flank.right) {
                const auto opener = last_open[length];
                if (opener != npos) {
                    pairs.push_back({runs[opener].start, start, length});
                    while (!delimiter_stack.empty()) {
                        const auto popped = delimiter_stack.back();
                        delimiter_stack.pop_back();
                        const auto popped_length = runs[popped].length;
                        if (last_open[popped_length] == popped) {
                            last_open[popped_length] = runs[popped].previous_same;
                        }
                        if (popped == opener) break;
                    }
                }
            }
            if (flank.left) {
                const auto index = runs.size();
                runs.push_back({start, length, last_open[length]});
                last_open[length] = index;
                delimiter_stack.push_back(index);
            }
        }
        std::ranges::sort(pairs, {}, &StrikePair::open);
        strikethrough_pairs_cache = std::move(pairs);
        return *strikethrough_pairs_cache;
    }

    const std::vector<std::size_t>& bracket_close_index() const {
        if (bracket_close_index_cache
            && bracket_index_scope_start <= scope_start
            && bracket_index_limit >= limit) return *bracket_close_index_cache;

        std::vector<std::size_t> close_at;
        std::vector<std::size_t> openers;
        for (auto scan = scope_start; scan < limit;) {
            const auto skipped = skip_emphasis_opaque(scan);
            if (skipped != scan) { scan = skipped; continue; }
            if (src[scan] == U'[') {
                if (close_at.empty()) close_at.resize(limit - scope_start, npos);
                openers.push_back(scan);
            }
            else if (src[scan] == U']' && !openers.empty()) {
                close_at[openers.back() - scope_start] = scan - scope_start;
                openers.pop_back();
            }
            ++scan;
        }
        bracket_index_scope_start = scope_start;
        bracket_index_limit = limit;
        bracket_close_index_cache = std::move(close_at);
        return *bracket_close_index_cache;
    }

    std::size_t bracket_close_for(std::size_t open) const {
        if (open < scope_start || open >= limit) return npos;
        const auto& index = bracket_close_index();
        const auto relative_open = open - bracket_index_scope_start;
        if (relative_open >= index.size() || index[relative_open] == npos) return npos;
        const auto close = bracket_index_scope_start + index[relative_open];
        return close < limit ? close : npos;
    }

    std::size_t find_link_label_close(std::size_t open) const {
        if (open < scope_start || open >= limit || src[open] != U'[') return npos;
        const auto close = bracket_close_for(open);
        return close < limit ? close : npos;
    }

    std::vector<LinkLabelRange> emphasis_link_labels() const {
        std::vector<LinkLabelRange> ranges;
        for (auto scan = scope_start; scan < limit;) {
            const auto skipped = skip_emphasis_opaque(scan);
            if (skipped != scan) { scan = skipped; continue; }
            if (src[scan] == U'[' && (scan == scope_start || src[scan - 1] != U'!')) {
                const auto close = find_link_label_close(scan);
                if (close != npos) {
                    bool is_link_candidate = close + 1 < limit
                        && (src[close + 1] == U'(' || src[close + 1] == U'[');
                    if (!is_link_candidate && ctx.resolve_link_label) {
                        is_link_candidate = resolve_reference_label(
                            src.substr(scan + 1, close - scan - 1)).has_value();
                    }
                    if (is_link_candidate) ranges.push_back({scan, close});
                }
            }
            ++scan;
        }
        return ranges;
    }

    bool has_separate_inline_structure(std::size_t from) {
        if (!separate_structure_positions_cache) {
            const auto labels = emphasis_link_labels();
            std::vector<std::size_t> positions;
            for (auto scan = scope_start; scan < limit;) {
                const auto skipped = skip_emphasis_opaque(scan);
                if (skipped != scan) {
                    if ((src[scan] == U'\\' && scan + 1 < limit
                            && (ascii_punct(src[scan + 1]) || is_line_ending(src[scan + 1])))
                        || src[scan] == U'`' || src[scan] == U'<') positions.push_back(scan);
                    scan = skipped;
                    continue;
                }
                if (src[scan] == U'&') {
                    const auto entity = entity_end(src, scan);
                    if (entity != npos && entity <= limit) positions.push_back(scan);
                }
                if (src[scan] == U'[' && std::ranges::any_of(labels,
                        [&](LinkLabelRange const& range) { return range.open == scan; })) positions.push_back(scan);
                if (src[scan] == U'!' && scan + 1 < limit && src[scan + 1] == U'[') {
                    const auto close = find_link_label_close(scan + 1);
                    if (close != npos && close + 1 < limit
                        && (src[close + 1] == U'(' || src[close + 1] == U'[')) positions.push_back(scan);
                }
                ++scan;
            }
            separate_structure_positions_cache = std::move(positions);
        }
        const auto next = std::ranges::lower_bound(*separate_structure_positions_cache, from);
        return next != separate_structure_positions_cache->end() && *next < limit;
    }

    std::vector<EmphasisPair> resolve_emphasis_pairs() {
        std::vector<EmphasisRun> runs;
        const auto labels = emphasis_link_labels();
        for (auto scan = scope_start; scan < limit;) {
            const auto skipped = skip_emphasis_opaque(scan);
            if (skipped != scan) { scan = skipped; continue; }
            const auto marker = src[scan];
            if (marker != U'*' && marker != U'_') { ++scan; continue; }
            const auto start = scan;
            while (scan < limit && src[scan] == marker) ++scan;
            const auto length = scan - start;
            const auto flank = delimiter_flanking(start, length);
            const bool can_open = marker == U'_'
                ? flank.left && (!flank.right || flank.previous_punctuation)
                : flank.left;
            const bool can_close = marker == U'_'
                ? flank.right && (!flank.left || flank.next_punctuation)
                : flank.right;
            if (!can_open && !can_close) continue;
            EmphasisRun run;
            run.marker = marker;
            run.start = start;
            run.end = scan;
            run.length = length;
            run.can_open = can_open;
            run.can_close = can_close;
            for (auto const& label : labels) {
                if (start > label.open && start < label.close) run.link_context.push_back(label.open);
            }
            run.open_remaining = length;
            if (can_open) emphasis_opener_starts_cache.push_back(start);
            runs.push_back(std::move(run));
        }

        std::vector<EmphasisPair> pairs;
        // Eligible opener categories differ only by marker, original run
        // length modulo three, and whether the run can also close. Keep the
        // most recent active opener in each category for each link context.
        // Failed searches therefore inspect six heads, not all earlier runs.
        using OpenerHeads = std::array<std::vector<std::size_t>, 12>;
        std::map<std::vector<std::size_t>, OpenerHeads> heads_by_context;
        std::vector<std::size_t> active;
        auto category = [](EmphasisRun const& run) {
            return (run.marker == U'_' ? 6u : 0u)
                + (run.can_close ? 3u : 0u) + run.length % 3;
        };
        for (std::size_t closer_index = 0; closer_index < runs.size(); ++closer_index) {
            auto& closer = runs[closer_index];
            auto& heads = heads_by_context[closer.link_context];
            std::size_t closer_remaining = closer.length;
            while (closer.can_close && closer_remaining != 0) {
                std::size_t opener_index = npos;
                const auto base = closer.marker == U'_' ? 6u : 0u;
                for (std::size_t which = base; which < base + 6; ++which) {
                    auto& candidates = heads[which];
                    while (!candidates.empty() && !runs[candidates.back()].active_opener)
                        candidates.pop_back();
                    if (candidates.empty()) continue;
                    const auto index = candidates.back();
                    auto const& opener = runs[index];
                    const bool rule_of_three = (closer.can_open || opener.can_close)
                        && ((opener.length + closer.length) % 3 == 0)
                        && (opener.length % 3 != 0 || closer.length % 3 != 0);
                    if (!rule_of_three && (opener_index == npos || index > opener_index)) opener_index = index;
                }
                if (opener_index == npos) break;
                auto& opener = runs[opener_index];
                const auto use = opener.open_remaining >= 2 && closer_remaining >= 2 ? 2u : 1u;
                const auto open_start = opener.start + opener.open_remaining - use;
                const auto close_start = closer.start + closer.close_consumed;
                pairs.push_back({open_start, open_start + use, close_start, close_start + use,
                    use == 2 ? InlineCstKind::Strong : InlineCstKind::Emphasis});
                opener.open_remaining -= use;
                if (opener.open_remaining == 0) opener.active_opener = false;
                closer.close_consumed += use;
                closer_remaining -= use;
                // Each intervening opener is removed once across this scope.
                while (!active.empty() && active.back() > opener_index) {
                    runs[active.back()].active_opener = false;
                    active.pop_back();
                }
                if (!opener.active_opener && !active.empty() && active.back() == opener_index)
                    active.pop_back();
            }
            if (closer.can_open && closer_remaining != 0) {
                closer.open_remaining = closer_remaining;
                closer.active_opener = true;
                heads[category(closer)].push_back(closer_index);
                active.push_back(closer_index);
            }
        }
        std::ranges::sort(pairs, {}, &EmphasisPair::open_start);
        return pairs;
    }

    std::vector<EmphasisPair> const& emphasis_pairs() {
        if (!emphasis_pairs_cache) emphasis_pairs_cache = resolve_emphasis_pairs();
        return *emphasis_pairs_cache;
    }

    // Match a literal substring at `pos`; return true and advance if matched.
    bool match(std::u32string_view lit) {
        if (pos + lit.size() > limit) return false;
        for (std::size_t i = 0; i < lit.size(); ++i) if (src[pos + i] != lit[i]) return false;
        advance(lit.size());
        return true;
    }

    // Find the next occurrence of `lit` without crossing a physical line ending.
    static std::size_t find_on_line(std::u32string_view s, std::size_t from, std::u32string_view lit) {
        for (std::size_t i = from; i + lit.size() <= s.size(); ++i) {
            if (is_line_ending(s[i])) return npos;
            bool ok = true;
            for (std::size_t j = 0; j < lit.size(); ++j) if (s[i + j] != lit[j]) { ok = false; break; }
            if (ok) return i;
        }
        return npos;
    }
    static constexpr std::size_t npos = std::u32string::npos;

    static bool ascii_alpha(char32_t c) {
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
    }

    static bool ascii_digit(char32_t c) {
        return c >= U'0' && c <= U'9';
    }

    static bool ascii_alnum(char32_t c) {
        return ascii_alpha(c) || ascii_digit(c);
    }

    static bool markdown_whitespace(char32_t c) {
        return c == U' ' || c == U'\t' || c == U'\n' || c == U'\r'
            || c == U'\f' || c == U'\v';
    }

    std::size_t matching_code_span_end(std::size_t at, std::size_t end) const {
        if (at >= end || src[at] != U'`') return npos;
        const auto* opener = code_run_covering(at);
        if (!opener) return npos;
        const auto run_end = (std::min)(opener->end, end);
        const auto run_size = run_end - at;
        return exact_code_run_end(run_end, end, run_size);
    }

    std::size_t html_tag_end(std::size_t at, std::size_t end) const {
        const auto tag = scan_markdown_html_tag(src, at, end);
        return tag ? tag->range.end : npos;
    }

    std::size_t matching_bracket(std::size_t open, std::size_t end) const {
        end = (std::min)(end, limit);
        if (open < scope_start || open >= end || src[open] != U'[') return npos;
        const auto close = bracket_close_for(open);
        return close < end ? close : npos;
    }

    static std::u32string decode_link_escapes_and_entities(std::u32string_view raw) {
        std::u32string result;
        result.reserve(raw.size());
        for (std::size_t cursor = 0; cursor < raw.size();) {
            if (raw[cursor] == U'\\' && cursor + 1 < raw.size() && ascii_punct(raw[cursor + 1])) {
                result.push_back(raw[cursor + 1]);
                cursor += 2;
                continue;
            }
            if (raw[cursor] == U'&') {
                const auto entity_end = Parser::entity_end(raw, cursor);
                if (entity_end != npos) {
                    if (auto decoded = decode_html_character_reference(raw.substr(cursor, entity_end - cursor))) {
                        result += *decoded;
                    }
                    cursor = entity_end;
                    continue;
                }
            }
            result.push_back(raw[cursor++]);
        }
        return result;
    }

    static std::string normalize_link_destination(std::u32string_view raw) {
        return percent_encode_url(cps_to_utf8(decode_link_escapes_and_entities(raw)));
    }

    static std::string percent_encode_url(std::string_view decoded) {
        static constexpr std::string_view allowed = "~-_.+!*(),%#@?=;:/$&";
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(decoded.size());
        for (const unsigned char byte : decoded) {
            if (ascii_alnum(byte) || allowed.find(static_cast<char>(byte)) != std::string_view::npos) {
                result.push_back(static_cast<char>(byte));
            } else {
                result.push_back('%');
                result.push_back(hex[byte >> 4]);
                result.push_back(hex[byte & 0x0f]);
            }
        }
        return result;
    }

    static bool valid_email_autolink(std::u32string_view address) {
        const auto at = address.find(U'@');
        if (at == npos || at == 0 || at + 1 >= address.size()
            || address.find(U'@', at + 1) != npos) return false;
        const auto local = address.substr(0, at);
        auto is_atext = [](char32_t c) {
            return ascii_alnum(c) || c == U'!' || c == U'#' || c == U'$' || c == U'%'
                || c == U'&' || c == U'\'' || c == U'*' || c == U'+' || c == U'-'
                || c == U'/' || c == U'=' || c == U'?' || c == U'^' || c == U'_'
                || c == U'`' || c == U'{' || c == U'|' || c == U'}' || c == U'~';
        };
        bool previous_dot = true;
        for (const auto c : local) {
            if (c == U'.') {
                if (previous_dot) return false;
                previous_dot = true;
            } else {
                if (!is_atext(c)) return false;
                previous_dot = false;
            }
        }
        if (previous_dot) return false;

        const auto domain = address.substr(at + 1);
        bool saw_dot = false;
        bool label_start = true;
        char32_t previous = 0;
        for (const auto c : domain) {
            if (c == U'.') {
                if (label_start || previous == U'-') return false;
                saw_dot = true;
                label_start = true;
            } else if (ascii_alnum(c)) {
                label_start = false;
            } else if (c == U'-') {
                if (label_start) return false;
                label_start = false;
            } else {
                return false;
            }
            previous = c;
        }
        return saw_dot && !label_start && previous != U'-';
    }

    static bool valid_uri_autolink(std::u32string_view uri) {
        const auto colon = uri.find(U':');
        if (colon < 2 || colon > 32 || colon + 1 >= uri.size() || !ascii_alpha(uri.front())) return false;
        for (std::size_t i = 1; i < colon; ++i) {
            const auto c = uri[i];
            if (!ascii_alnum(c) && c != U'+' && c != U'.' && c != U'-') return false;
        }
        for (const auto c : uri.substr(colon + 1)) {
            if (c <= 0x20 || c == 0x7f || c == U'<' || c == U'>') return false;
        }
        return true;
    }

    const std::vector<std::size_t>& angle_stoppers() const {
        if (angle_stoppers_cache
            && angle_index_scope_start == scope_start
            && angle_index_limit == limit) return *angle_stoppers_cache;
        std::vector<std::size_t> positions;
        for (std::size_t scan = scope_start; scan < limit; ++scan) {
            // These are the first positions where the angle scanner must stop:
            // line endings terminate it, while another `<`, whitespace, or a
            // control character makes both URI and email candidates invalid.
            if (src[scan] == U'>' || src[scan] == U'<' || is_line_ending(src[scan])
                || src[scan] <= 0x20 || src[scan] == 0x7f) positions.push_back(scan);
        }
        angle_index_scope_start = scope_start;
        angle_index_limit = limit;
        angle_stoppers_cache = std::move(positions);
        return *angle_stoppers_cache;
    }

    std::size_t next_angle_stopper(std::size_t from, std::size_t end) const {
        end = (std::min)(end, limit);
        if (from < scope_start || from >= end) return npos;
        const auto& positions = angle_stoppers();
        const auto found = std::ranges::lower_bound(positions, from);
        return found != positions.end() && *found < end ? *found : npos;
    }

    std::size_t valid_autolink_end(std::size_t at, std::size_t end) const {
        if (at >= end || src[at] != U'<') return npos;
        const auto close = next_angle_stopper(at + 1, end);
        if (close == npos || src[close] != U'>' || close == at + 1) return npos;
        const auto contents = src.substr(at + 1, close - at - 1);
        return valid_uri_autolink(contents) || valid_email_autolink(contents) ? close + 1 : npos;
    }

    bool contains_code_span_or_html_tag(std::size_t begin, std::size_t end) const {
        for (auto cursor = begin; cursor < end;) {
            if (src[cursor] == U'\\' && cursor + 1 < end && ascii_punct(src[cursor + 1])) {
                cursor += 2;
                continue;
            }
            if (src[cursor] == U'`') {
                const auto code_end = matching_code_span_end(cursor, end);
                if (code_end != npos) return true;
                while (cursor < end && src[cursor] == U'`') ++cursor;
                continue;
            }
            if (src[cursor] == U'<') {
                if (html_tag_end(cursor, end) != npos) return true;
            }
            ++cursor;
        }
        return false;
    }

    static std::string normalize_link_title(std::u32string_view raw) {
        return cps_to_utf8(decode_link_escapes_and_entities(raw));
    }

    bool skip_link_whitespace(std::size_t& cursor, std::size_t end) const {
        unsigned line_breaks = 0;
        while (cursor < end && markdown_whitespace(src[cursor])) {
            if (src[cursor] == U'\r' || src[cursor] == U'\n') {
                if (src[cursor] == U'\r' && cursor + 1 < end && src[cursor + 1] == U'\n') ++cursor;
                if (++line_breaks > 1) return false;
            }
            ++cursor;
        }
        return true;
    }

    bool parse_inline_link_suffix(
        std::size_t suffix,
        std::size_t end,
        std::size_t& source_end,
        std::string& href,
        std::optional<std::string>& title) const {
        if (suffix >= end || src[suffix] != U'(') return false;
        // A complete inline link suffix must end in an unescaped `)`.
        // Reject impossible suffixes before title/destination scans walk a tail.
        if (!has_unescaped_close_paren_after(suffix, end)) return false;
        std::size_t cursor = suffix + 1;
        const auto after_open = cursor;
        if (!skip_link_whitespace(cursor, end)) return false;
        const bool leading_whitespace = cursor != after_open;

        std::size_t destination_start = cursor;
        std::size_t destination_end = cursor;
        const bool title_opener = cursor < end
            && (src[cursor] == U'"' || src[cursor] == U'\'' || src[cursor] == U'(');
        const bool empty_destination = cursor < end && (src[cursor] == U')'
            || (leading_whitespace && title_opener));
        if (empty_destination) {
            href.clear();
            destination_end = destination_start;
        } else if (cursor < end && src[cursor] == U'<') {
            destination_start = ++cursor;
            while (cursor < end) {
                if (src[cursor] == U'\\' && cursor + 1 < end && ascii_punct(src[cursor + 1])) {
                    cursor += 2;
                    continue;
                }
                if (is_line_ending(src[cursor]) || src[cursor] == U'<') return false;
                if (src[cursor] == U'>') break;
                ++cursor;
            }
            if (cursor >= end || src[cursor] != U'>') return false;
            destination_end = cursor;
            ++cursor;
        } else {
            std::size_t parenthesis_depth = 0;
            while (cursor < end) {
                const auto current = src[cursor];
                if (current == U'\\' && cursor + 1 < end && ascii_punct(src[cursor + 1])) {
                    cursor += 2;
                    continue;
                }
                if (markdown_whitespace(current)) break;
                if (current == U'<' || current == U'>' || current < 0x20 || current == 0x7f) return false;
                if (current == U'(') {
                    if (++parenthesis_depth > 32) return false;
                } else if (current == U')') {
                    if (parenthesis_depth == 0) break;
                    --parenthesis_depth;
                }
                ++cursor;
            }
            if (parenthesis_depth != 0) return false;
            destination_end = cursor;
            if (destination_start == destination_end && (cursor >= end || src[cursor] != U')')) return false;
        }
        if (!empty_destination) {
            href = normalize_link_destination(std::u32string_view(src).substr(
                destination_start, destination_end - destination_start));
        }

        const auto after_destination = cursor;
        if (!skip_link_whitespace(cursor, end)) return false;
        if (cursor < end && (src[cursor] == U'"' || src[cursor] == U'\'' || src[cursor] == U'(')) {
            if (cursor == after_destination && !leading_whitespace) return false;
            const auto opener = src[cursor++];
            const auto closer = opener == U'(' ? U')' : opener;
            const auto title_start = cursor;
            std::size_t nesting = opener == U'(' ? 1u : 0u;
            bool title_closed = false;
            while (cursor < end) {
                if (src[cursor] == U'\\' && cursor + 1 < end && ascii_punct(src[cursor + 1])) {
                    cursor += 2;
                    continue;
                }
                if (opener == U'(' && src[cursor] == U'(') ++nesting;
                else if (src[cursor] == closer) {
                    if (opener != U'(' || --nesting == 0) {
                        title = normalize_link_title(std::u32string_view(src).substr(title_start, cursor - title_start));
                        ++cursor;
                        title_closed = true;
                        break;
                    }
                }
                ++cursor;
            }
            if (!title_closed || !skip_link_whitespace(cursor, end)) return false;
        }
        if (cursor >= end || src[cursor] != U')') return false;
        source_end = cursor + 1;
        return true;
    }

    static bool contains_link_or_autolink(const InlineCstNodes& nodes) {
        for (const auto& node : nodes) {
            if (node.kind == InlineCstKind::Link || node.kind == InlineCstKind::Autolink) return true;
            if (contains_link_or_autolink(node.children)) return true;
        }
        return false;
    }

    static void append_image_alt(const InlineCstNodes& nodes, std::u32string_view source, std::u32string& output) {
        for (const auto& node : nodes) {
            const auto range = node.range;
            const auto raw = range.valid_for(source.size())
                ? source.substr(range.start, range.length()) : std::u32string_view{};
            switch (node.kind) {
                case InlineCstKind::Emphasis:
                case InlineCstKind::Strong:
                case InlineCstKind::Strikethrough:
                case InlineCstKind::Link:
                    append_image_alt(node.children, source, output);
                    break;
                case InlineCstKind::Image:
                    output += utf8_to_cps(node.semantics().alt);
                    break;
                case InlineCstKind::HtmlElement:
                    if (node.semantics().html_tag == "img") {
                        output += utf8_to_cps(node.semantics().alt);
                    } else if (node.semantics().html_tag == "br") {
                        output.push_back(U' ');
                    } else {
                        append_image_alt(node.children, source, output);
                    }
                    break;
                case InlineCstKind::Escape:
                    if (!raw.empty()) output.push_back(raw.back());
                    break;
                case InlineCstKind::Entity:
                    if (auto decoded = decode_html_character_reference(raw)) output += *decoded;
                    break;
                case InlineCstKind::SoftBreak:
                case InlineCstKind::HardBreak:
                    output.push_back(U' ');
                    break;
                case InlineCstKind::CodeSpan:
                    if (node.delimiter_ranges().content.valid_for(source.size())) {
                        auto code = std::u32string(source.substr(
                            node.delimiter_ranges().content.start,
                            node.delimiter_ranges().content.length()));
                        for (auto& character : code) {
                            if (character == U'\n' || character == U'\r') character = U' ';
                        }
                        const auto all_spaces = std::all_of(code.begin(), code.end(), [](char32_t c) { return c == U' '; });
                        if (code.size() >= 2 && code.front() == U' ' && code.back() == U' ' && !all_spaces) {
                            code.erase(code.begin());
                            code.pop_back();
                        }
                        output += code;
                    }
                    break;
                case InlineCstKind::Autolink:
                    if (node.delimiter_ranges().content.valid_for(source.size())) {
                        output += source.substr(
                            node.delimiter_ranges().content.start,
                            node.delimiter_ranges().content.length());
                    }
                    break;
                case InlineCstKind::HtmlComment:
                    break;
                default:
                    output += decode_link_escapes_and_entities(raw);
                    break;
            }
        }
    }

    InlineCstNodes parse_label_content(std::size_t begin, std::size_t end, bool suppress_bare_links = false) {
        Parser inner{src, ctx};
        inner.counter = counter;
        inner.pos = begin;
        inner.suppress_bare_autolinks = suppress_bare_autolinks || suppress_bare_links;
        auto children = inner.parse_until(end);
        counter = inner.counter;
        return children;
    }

    const std::vector<NestedLinkRange>& nested_link_ranges() {
        if (nested_link_ranges_cache
            && nested_link_index_scope_start == scope_start
            && nested_link_index_limit == limit) return *nested_link_ranges_cache;

        std::vector<NestedLinkRange> ranges;
        const auto add_range = [&](std::size_t start, std::size_t end) {
            if (ranges.empty() || ranges.back().start != start) ranges.push_back({start, end});
        };
        const auto local_char = [](char32_t c) {
            return ascii_alnum(c) || c == U'.' || c == U'+' || c == U'-' || c == U'_';
        };
        for (auto scan = scope_start; scan < limit;) {
            if (src[scan] == U'<') {
                if (const auto special = html_special_at(scan, limit)) {
                    scan = special->range.end;
                    continue;
                }
                if (const auto tag = html_tag_at(scan, limit)) {
                    scan = tag->end;
                    continue;
                }
                const auto auto_end = valid_autolink_end(scan, limit);
                if (auto_end != npos) {
                    add_range(scan, auto_end);
                    scan = auto_end;
                    continue;
                }
            }

            const auto skipped = skip_emphasis_opaque(scan);
            if (skipped != scan) { scan = skipped; continue; }

            if (src[scan] == U'[') {
                const auto close = bracket_close_for(scan);
                if (close < limit) {
                    const bool image = scan > scope_start && src[scan - 1] == U'!';
                    const auto after = close + 1;
                    bool is_link = false;
                    auto link_end = after;
                    if (after < limit && src[after] == U'(') {
                        std::string href;
                        std::optional<std::string> title;
                        is_link = parse_inline_link_suffix(after, limit, link_end, href, title);
                    } else if (after < limit && src[after] == U'[') {
                        const auto reference_close = bracket_close_for(after);
                        if (reference_close < limit) {
                            auto label = src.substr(after + 1, reference_close - after - 1);
                            if (label.empty()) label = src.substr(scan + 1, close - scan - 1);
                            is_link = resolve_reference_label(label).has_value();
                            link_end = reference_close + 1;
                        }
                    } else {
                        const auto label = src.substr(scan + 1, close - scan - 1);
                        is_link = resolve_reference_label(label).has_value();
                        link_end = close + 1;
                    }
                    if (is_link && !image) add_range(scan, link_end);
                }
            }

            if (!suppress_bare_autolinks && ctx.dialect.gfm.autolinks
                && inline_syntax_allows_markdown(ctx.syntax_mode)) {
                const bool prefix = starts_with_at(scan, U"www.")
                    || starts_with_at(scan, U"http://") || starts_with_at(scan, U"https://")
                    || starts_with_at(scan, U"ftp://") || starts_with_at(scan, U"mailto:")
                    || starts_with_at(scan, U"xmpp:");
                const bool email_start = local_char(src[scan])
                    && (scan == scope_start || !local_char(src[scan - 1]));
                if (prefix || email_start) {
                    const auto bare = scan_gfm_autolink(src, scan, limit, scope_start);
                    if (bare) {
                        add_range(scan, bare->range.end);
                        scan = bare->range.end;
                        continue;
                    }
                }
            }
            ++scan;
        }
        nested_link_index_scope_start = scope_start;
        nested_link_index_limit = limit;
        nested_link_ranges_cache = std::move(ranges);
        return *nested_link_ranges_cache;
    }

    bool nested_link_after(std::size_t begin, std::size_t end) {
        end = (std::min)(end, limit);
        if (begin >= end) return false;
        const auto& ranges = nested_link_ranges();
        const auto first = std::ranges::lower_bound(ranges, begin, {}, &NestedLinkRange::start);
        if (end == limit) return first != ranges.end();
        for (auto candidate = first; candidate != ranges.end() && candidate->start < end; ++candidate) {
            if (candidate->end <= end) return true;
        }
        return false;
    }

    bool emit_link_or_image(
        InlineCstNodes& out,
        std::size_t& run_start,
        std::size_t start,
        std::size_t text_start,
        std::size_t text_end,
        std::size_t source_end,
        bool is_image,
        std::string href,
        std::optional<std::string> title) {
        // GFM's post-processing skips links, but does process image children.
        // This mirrors that behavior while keeping source ownership in the label.
        auto children = parse_label_content(text_start, text_end, !is_image);
        if (!is_image && contains_link_or_autolink(children)) return false;
        flush_text_to(out, run_start, start);
        InlineCstNode node;
        node.id = next_id();
        node.kind = is_image ? InlineCstKind::Image : InlineCstKind::Link;
        node.status = ParseStatus::Complete;
        node.range = {start, source_end};
        node.ensure_delimiter_ranges() = {
            {start, source_end},
            {start, text_start},
            {text_start, text_end},
            SourceRange{text_end, source_end}};
        auto& semantic = node.ensure_semantics();
        semantic.href = std::move(href);
        semantic.title = std::move(title);
        if (is_image) {
            std::u32string alt;
            append_image_alt(children, src, alt);
            semantic.alt = cps_to_utf8(alt);
        }
        node.children = std::move(children);
        out.push_back(std::move(node));
        pos = source_end;
        run_start = pos;
        return true;
    }

    std::string normalized_reference_label(std::u32string_view raw) const {
        // Reference labels are normalized as raw label strings; CommonMark
        // does not parse escapes or character references inside them.
        // The reference-label limit bounds both normalization and lookup work.
        if (raw.size() > 999) return {};
        return normalize_markdown_link_label(raw);
    }

    auto resolve_reference_label(std::u32string_view raw) const {
        using Result = decltype(ctx.resolve_link_label(std::declval<const std::string&>()));
        if (!ctx.resolve_link_label || raw.empty() || raw.size() > 999) return Result{};
        return ctx.resolve_link_label(normalized_reference_label(raw));
    }

    // ---- entity (preserved verbatim as an Entity node) ----
    // Returns end offset (exclusive) of a recognized entity starting at `at`, else npos.
    static std::size_t entity_end(std::u32string_view s, std::size_t at) {
        if (at >= s.size() || s[at] != U'&') return npos;
        const auto max_length = (std::min)(std::size_t{34}, s.size() - at);
        for (std::size_t length = 3; length <= max_length; ++length) {
            const auto current = s[at + length - 1];
            if (is_line_ending(current)) return npos;
            if (current != U';') continue;
            const auto raw = s.substr(at, length);
            if (decode_html_character_reference(raw)) return at + length;
            return npos;
        }
        return npos;
    }

    // ---- leaf text run accumulator ----
    // Emit any pending literal run [run_start, pos) as a single Text node.
    void flush_text_to(InlineCstNodes& out, std::size_t& run_start, std::size_t end) {
        if (end <= run_start) return;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Text;
        node.range = {run_start, end};
        out.push_back(std::move(node));
        run_start = end;
    }

    void flush_text(InlineCstNodes& out, std::size_t& run_start) {
        flush_text_to(out, run_start, pos);
    }

    // ---- soft / hard physical line break ----
    // CRLF is one exact CST node and therefore one visual/caret unit.
    bool parse_break(
        InlineCstNodes& out,
        std::size_t& run_start,
        bool markdown_hard_breaks = true) {
        if (!is_line_ending(peek())) return false;
        const auto ending_length = line_ending_length(pos);
        // CommonMark discards trailing line whitespace. Two or more literal
        // spaces make a hard break; an odd run of backslashes makes one only
        // when its final backslash is not escaped by the preceding run.
        auto trailing_spaces = pos;
        while (trailing_spaces > run_start && ch(trailing_spaces - 1) == U' ') --trailing_spaces;
        const auto space_count = pos - trailing_spaces;
        auto trailing_whitespace = pos;
        while (trailing_whitespace > run_start
            && !is_line_ending(ch(trailing_whitespace - 1))
            && is_commonmark_whitespace(ch(trailing_whitespace - 1))) {
            --trailing_whitespace;
        }
        const auto soft_break_start = markdown_hard_breaks ? trailing_whitespace : pos;
        auto after_break = pos + ending_length;
        if (markdown_hard_breaks) {
            while (after_break < limit && (ch(after_break) == U' ' || ch(after_break) == U'\t')) ++after_break;
        }
        auto slash_count = std::size_t{0};
        while (pos > run_start + slash_count && ch(pos - slash_count - 1) == U'\\') ++slash_count;
        const bool hard_spaces = markdown_hard_breaks && space_count >= 2;
        const bool hard_slash = markdown_hard_breaks && !hard_spaces
            && (slash_count % 2 == 1) && after_break < limit;
        const bool hard = hard_spaces || hard_slash;
        const auto break_start = hard_spaces ? soft_break_start : hard_slash ? pos - 1 : soft_break_start;
        if (hard) {
            // The break owns the exact trailing marker/whitespace, line ending
            // and continuation indentation. None of those characters are
            // removed from source or from the CST's range partition.
            const auto text_end = break_start;
            // An incomplete construct immediately before this newline may
            // already own the trailing backslash/spaces. Top-level nodes must
            // not claim those characters twice.
            if (text_end > run_start) {
                InlineCstNode node;
                node.id = next_id();
                node.kind = InlineCstKind::Text;
                node.range = {run_start, text_end};
                out.push_back(std::move(node));
            }
            advance(after_break - pos);
            InlineCstNode hb;
            hb.id = next_id();
            hb.kind = InlineCstKind::HardBreak;
            hb.range = {break_start, pos};
            hb.ensure_delimiter_ranges() = {{break_start, pos}, {break_start, pos}, {break_start, pos}, std::nullopt};
            out.push_back(std::move(hb));
            run_start = pos;
        } else {
            // Strip a single trailing space (or a tab) and continuation
            // indentation from the break's semantic projection while keeping
            // every source code point inside this one CST node.
            flush_text_to(out, run_start, soft_break_start);
            advance(after_break - pos);
            InlineCstNode sb;
            sb.id = next_id();
            sb.kind = InlineCstKind::SoftBreak;
            sb.range = {soft_break_start, pos};
            sb.ensure_delimiter_ranges() = {{soft_break_start, pos}, {soft_break_start, pos}, {soft_break_start, pos}, std::nullopt};
            out.push_back(std::move(sb));
            run_start = pos;
        }
        return true;
    }

    // ---- escape `\punct` ----
    bool parse_escape(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'\\') return false;
        if (is_line_ending(peek(1))) return false; // handled as hard break
        if (!ascii_punct(peek(1))) return false;
        std::size_t start = pos;
        flush_text_to(out, run_start, start);
        advance(2);
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Escape;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, pos}, {start, pos}, std::nullopt};
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- entity &amp; ----
    bool parse_entity(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'&') return false;
        std::size_t end = entity_end(src, pos);
        if (end == npos) return false;
        std::size_t start = pos;
        flush_text_to(out, run_start, start);
        pos = end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Entity;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, pos}, {start, pos}, std::nullopt};
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- delimited inline (emphasis/strong/strike) ----
    // Tries to parse a `**...**`, `*...*`, `__...__`, `_..._`, `~~...~~`.
    // On a missing closer, emits an Incomplete node covering the opener + the
    // remainder of the line (still lossless — every char accounted for).
    bool parse_delimited(InlineCstNodes& out, std::size_t& run_start, std::u32string_view opener, InlineCstKind kind) {
        if (!starts_with(pos, opener)) return false;
        const auto underscore = !opener.empty() && opener.front() == U'_';
        // Other delimiters retain their existing editable/incomplete behavior;
        // underscore runs use CommonMark's stricter left/right-flanking rules
        // so identifier characters such as `trig_out` remain literal.
        std::size_t after = pos + opener.size();
        if (underscore && after < limit && !underscore_can_open(pos, opener.size())) return false;
        if (underscore && after >= limit && pos > 0
            && !is_cp_white(ch(pos - 1)) && !delimiter_punct(ch(pos - 1))) return false;
        if (after >= limit) {
            const auto start = pos;
            flush_text_to(out, run_start, start);
            pos = after;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Incomplete;
            node.status = ParseStatus::MissingCloser;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, pos}, {pos, pos}, std::nullopt};
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }
        char32_t next = ch(after);
        if (next == U' ' || next == U'\n' || next == U'\t') return false;
        if (opener == U"~~" && ch(after) == U' ') return false;

        std::size_t start = pos;
        std::size_t content_start = pos + opener.size();
        std::size_t closer = find_closer(content_start, opener);
        if (closer == npos) {
            // Keep the incomplete delimiter as the structural owner, but still
            // parse its content. Editing must be able to recognize semantic
            // children such as a table-cell <br> even while the surrounding
            // emphasis/strong/strike delimiter is temporarily unclosed.
            flush_text_to(out, run_start, start);
            pos = content_start;
            std::size_t end = find_line_or_doc_end(pos);
            if (end > pos) pos = end;
            Parser inner{src, ctx};
            inner.suppress_bare_autolinks = suppress_bare_autolinks;
            inner.counter = counter;
            inner.pos = content_start;
            auto children = inner.parse_until(pos);
            counter = inner.counter;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Incomplete;
            node.status = ParseStatus::MissingCloser;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, pos}, std::nullopt};
            node.children = std::move(children);
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }
        flush_text_to(out, run_start, start);
        std::size_t content_end = closer;
        std::size_t close_end = closer + opener.size();
        // Recurse into content for nested structure.
        Parser inner{src, ctx};
        inner.suppress_bare_autolinks = suppress_bare_autolinks;
        inner.counter = counter;
        inner.pos = content_start;
        InlineCstNodes children = inner.parse_until(content_end);
        counter = inner.counter;
        pos = close_end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = kind;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, close_end}};
        node.children = std::move(children);
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // GFM strike recognizes exact one- or two-tilde runs. Longer runs remain
    // literal source, including during editing.
    bool parse_strikethrough(InlineCstNodes& out, std::size_t& run_start) {
        if (!ctx.dialect.gfm.strikethrough || peek() != U'~') return false;
        if (pos > scope_start && ch(pos - 1) == U'~') return false;
        auto opener_end = pos;
        while (opener_end < limit && ch(opener_end) == U'~') ++opener_end;
        const auto delimiter_size = opener_end - pos;
        if (delimiter_size != 1 && delimiter_size != 2) return false;
        const auto opening_flank = delimiter_flanking(pos, delimiter_size);
        if (!opening_flank.left) return false;

        const auto start = pos;
        const auto content_start = opener_end;
        const auto& pairs = strikethrough_pairs();
        const auto pair = std::ranges::lower_bound(pairs, start, {}, &StrikePair::open);
        const auto closer = pair != pairs.end() && pair->open == start
            && pair->length == delimiter_size ? pair->close : npos;
        if (closer == npos) {
            flush_text_to(out, run_start, start);
            pos = content_start;
            const auto end = find_line_or_doc_end(pos);
            if (end > pos) pos = end;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Incomplete;
            node.status = ParseStatus::MissingCloser;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, pos}, std::nullopt};
            Parser inner{src, ctx};
            inner.suppress_bare_autolinks = suppress_bare_autolinks;
        inner.counter = counter;
            inner.pos = content_start;
            node.children = inner.parse_until(pos);
            counter = inner.counter;
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }

        flush_text_to(out, run_start, start);
        Parser inner{src, ctx};
        inner.suppress_bare_autolinks = suppress_bare_autolinks;
        inner.counter = counter;
        inner.pos = content_start;
        auto children = inner.parse_until(closer);
        counter = inner.counter;
        pos = closer + delimiter_size;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Strikethrough;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, closer}, SourceRange{closer, pos}};
        node.children = std::move(children);
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    bool parse_commonmark_emphasis(InlineCstNodes& out, std::size_t& run_start) {
        const auto marker = peek();
        if (marker != U'*' && marker != U'_') return false;

        auto run_start_source = pos;
        while (run_start_source > scope_start && src[run_start_source - 1] == marker) --run_start_source;
        auto run_end_source = pos;
        while (run_end_source < limit && src[run_end_source] == marker) ++run_end_source;

        const auto& pairs = emphasis_pairs();
        const auto first = std::ranges::lower_bound(pairs, pos, {}, &EmphasisPair::open_start);
        const EmphasisPair* match = first != pairs.end() && first->open_start < run_end_source
            && first->close_end <= limit ? &*first : nullptr;

        if (match && match->open_start > pos) {
            flush_text_to(out, run_start, pos);
            InlineCstNode literal_open;
            literal_open.id = next_id();
            literal_open.kind = InlineCstKind::Incomplete;
            literal_open.status = ParseStatus::MissingCloser;
            literal_open.range = {pos, match->open_start};
            literal_open.ensure_delimiter_ranges() = {
                {pos, match->open_start}, {pos, match->open_start},
                {match->open_start, match->open_start}, std::nullopt};
            out.push_back(std::move(literal_open));
            pos = match->open_start;
            run_start = pos;
            return true;
        }

        if (!match) {
            const auto length = run_end_source - run_start_source;
            const auto flank = delimiter_flanking(run_start_source, length);
            const bool can_open = marker == U'_'
                ? flank.left && (!flank.right || flank.previous_punctuation)
                : flank.left;
            flush_text_to(out, run_start, pos);

            const auto later_pair = std::ranges::lower_bound(
                pairs, run_end_source, {}, &EmphasisPair::open_start);
            if ((can_open && later_pair != pairs.end() && later_pair->open_start < limit)
                || has_separate_inline_structure(run_end_source)) {
                InlineCstNode incomplete;
                incomplete.id = next_id();
                incomplete.kind = InlineCstKind::Incomplete;
                incomplete.status = ParseStatus::MissingCloser;
                incomplete.range = {pos, run_end_source};
                incomplete.ensure_delimiter_ranges() = {
                    {pos, run_end_source}, {pos, run_end_source},
                    {run_end_source, run_end_source}, std::nullopt};
                out.push_back(std::move(incomplete));
                pos = run_end_source;
                run_start = pos;
                return true;
            }

            auto incomplete_end = run_end_source;
            if (can_open) {
                // A later opening run begins its own editable incomplete
                // construct. Do not recursively wrap unrelated unclosed runs
                // through the entire rest of the line.
                const auto next = std::ranges::lower_bound(emphasis_opener_starts_cache, run_end_source);
                const auto boundary = next == emphasis_opener_starts_cache.end() ? limit : (std::min)(limit, *next);
                incomplete_end = run_end_source;
                while (incomplete_end < boundary && !is_line_ending(src[incomplete_end])) ++incomplete_end;
            } else incomplete_end = run_end_source;
            Parser inner{src, ctx};
            inner.suppress_bare_autolinks = suppress_bare_autolinks;
            inner.counter = counter;
            inner.pos = run_end_source;
            auto children = inner.parse_until(incomplete_end);
            counter = inner.counter;
            InlineCstNode incomplete;
            incomplete.id = next_id();
            incomplete.kind = InlineCstKind::Incomplete;
            incomplete.status = ParseStatus::MissingCloser;
            incomplete.range = {pos, incomplete_end};
            incomplete.ensure_delimiter_ranges() = {
                {pos, incomplete_end}, {pos, run_end_source},
                {run_end_source, incomplete_end}, std::nullopt};
            incomplete.children = std::move(children);
            out.push_back(std::move(incomplete));
            pos = incomplete_end;
            run_start = pos;
            return true;
        }

        flush_text_to(out, run_start, match->open_start);
        pending_emphasis = *match;
        pos = match->open_end;
        run_start = pos;
        return true;
    }

    bool starts_with(std::size_t at, std::u32string_view lit) const {
        if (at + lit.size() > limit) return false;
        for (std::size_t i = 0; i < lit.size(); ++i) if (src[at + i] != lit[i]) return false;
        return true;
    }

    // Find a matching closer for `opener` starting at `from`, not crossing a
    // blank line (double newline) — emphasis cannot span a paragraph break.
    std::size_t find_closer(std::size_t from, std::u32string_view opener) {
        for (std::size_t i = from; i + opener.size() <= limit; ++i) {
            if (src[i] == U'\n' && i + 1 < limit && src[i + 1] == U'\n') return npos;
            if (starts_with(i, opener)) {
                // right-flanking: char before closer must not be whitespace.
                if (i == 0) return i;
                char32_t prev = ch(i - 1);
                if (prev == U' ' || prev == U'\n' || prev == U'\t') continue;
                if (!opener.empty() && opener.front() == U'_'
                    && !underscore_can_close(i, opener.size())) continue;
                return i;
            }
        }
        return npos;
    }

    std::size_t find_line_or_doc_end(std::size_t from) const {
        // For incomplete constructs, consume to end of this line (incl. newline)
        // or end of source.
        std::size_t i = from;
        while (i < limit && !is_line_ending(src[i])) ++i;
        if (i < limit) i += line_ending_length(i); // include CR, LF, or CRLF exactly
        return i;
    }

    // ---- inline code `...` ----
    bool parse_code_span(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'`') return false;
        if (pos > 0 && ch(pos - 1) == U'`') return false; // part of a longer run
        std::size_t start = pos;
        std::size_t count = 0;
        while (pos < limit && peek() == U'`') { ++count; advance(); }
        std::size_t content_start = pos;
        // CommonMark closes on an exact-length run and code spans can cross
        // blank lines. Keep the first other-length run available as a possible
        // opener if this delimiter remains unmatched.
        const auto closing_end = exact_code_run_end(pos, limit, count);
        const auto closing = closing_end == npos ? npos : closing_end - count;
        const auto* next_candidate = closing == npos ? next_code_run(pos, limit) : nullptr;
        if (closing == npos) {
            // Preserve an unmatched opener as an incomplete CST segment. If a
            // later, differently sized run can begin another valid span, stop
            // before it so that run can be parsed on the next iteration.
            flush_text_to(out, run_start, start);
            pos = next_candidate ? next_candidate->start : find_line_or_doc_end(pos);
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Incomplete;
            node.status = ParseStatus::MissingCloser;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, pos}, std::nullopt};
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }
        flush_text_to(out, run_start, start);
        std::size_t content_end = closing;
        std::size_t close_end = closing + count;
        pos = close_end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::CodeSpan;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, close_end}};
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- inline math $...$ or \(...\) ----
    bool parse_math(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() == U'$' && ctx.dialect.math.inline_dollar) {
            if (peek(1) == U'$') return false; // $$ belongs to block path
            std::size_t start = pos;
            advance();
            std::size_t content_start = pos;
            while (!eof() && peek() != U'$' && peek() != U'\n') advance();
            if (peek() != U'$') {
                flush_text_to(out, run_start, start);
                pos = find_line_or_doc_end(pos);
                InlineCstNode node;
                node.id = next_id();
                node.kind = InlineCstKind::Incomplete;
                node.status = ParseStatus::MissingCloser;
                node.range = {start, pos};
                node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, pos}, std::nullopt};
                out.push_back(std::move(node));
                run_start = pos;
                return true;
            }
            flush_text_to(out, run_start, start);
            std::size_t content_end = pos;
            advance();
            std::size_t close_end = pos;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::InlineMath;
            node.status = ParseStatus::Complete;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, close_end}};
            node.ensure_semantics().math_delim = MathDelimiter::InlineDollar;
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }
        if (peek() == U'\\' && peek(1) == U'(' && ctx.dialect.math.inline_paren) {
            std::size_t start = pos;
            advance(2);
            std::size_t content_start = pos;
            while (!eof() && !(peek() == U'\\' && peek(1) == U')') && peek() != U'\n') advance();
            if (!(peek() == U'\\' && peek(1) == U')')) {
                flush_text_to(out, run_start, start);
                pos = find_line_or_doc_end(pos);
                InlineCstNode node;
                node.id = next_id();
                node.kind = InlineCstKind::Incomplete;
                node.status = ParseStatus::MissingCloser;
                node.range = {start, pos};
                node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, pos}, std::nullopt};
                out.push_back(std::move(node));
                run_start = pos;
                return true;
            }
            flush_text_to(out, run_start, start);
            std::size_t content_end = pos;
            advance(2);
            std::size_t close_end = pos;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::InlineMath;
            node.status = ParseStatus::Complete;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, close_end}};
            node.ensure_semantics().math_delim = MathDelimiter::InlineParen;
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }
        return false;
    }

    const std::vector<std::size_t>& unescaped_close_parens() const {
        if (unescaped_close_parens_cache
            && close_parens_scope_start == scope_start
            && close_parens_limit == limit) return *unescaped_close_parens_cache;
        std::vector<std::size_t> closes;
        for (std::size_t scan = scope_start; scan < limit;) {
            if (src[scan] == U'\\' && scan + 1 < limit && ascii_punct(src[scan + 1])) {
                scan += 2;
                continue;
            }
            if (src[scan] == U')') closes.push_back(scan);
            ++scan;
        }
        close_parens_scope_start = scope_start;
        close_parens_limit = limit;
        unescaped_close_parens_cache = std::move(closes);
        return *unescaped_close_parens_cache;
    }

    bool has_unescaped_close_paren_after(std::size_t position, std::size_t end) const {
        end = (std::min)(end, limit);
        const auto& closes = unescaped_close_parens();
        const auto found = std::ranges::upper_bound(closes, position);
        return found != closes.end() && *found < end;
    }

    // ---- link / image [text](url) / ![alt](url) / [text][ref] ----
    bool parse_link_or_image(InlineCstNodes& out, std::size_t& run_start) {
        const auto start = pos;
        const bool is_image = peek() == U'!' && peek(1) == U'[';
        const auto open = start + (is_image ? 1u : 0u);
        if (open >= limit || src[open] != U'[') return false;

        const auto close = matching_bracket(open, limit);
        if (close == npos) {
            // An unmatched outer bracket must not hide a complete inner link.
            // Leave the input untouched in that case so the normal scanner can
            // recognize the inner construct on its next pass.
            if (nested_link_after(open + 1, limit)
                || contains_code_span_or_html_tag(open + 1, limit)) return false;
            return parse_incomplete_bracket(out, run_start, start);
        }

        const auto text_start = open + 1;
        const auto text_end = close;
        const auto after_text = close + 1;

        if (after_text < limit && src[after_text] == U'(') {
            std::size_t source_end = after_text;
            std::string href;
            std::optional<std::string> title;
            if (parse_inline_link_suffix(after_text, limit, source_end, href, title)) {
                if (emit_link_or_image(out, run_start, start, text_start, text_end,
                        source_end, is_image, std::move(href), std::move(title))) return true;
                return false; // CommonMark forbids a link nested inside a link.
            }
            // A partial destination remains an editable incomplete construct;
            // a closed but invalid form is ordinary literal Markdown.
            const bool has_close_paren = has_unescaped_close_paren_after(after_text, limit);
            if (!has_close_paren && !nested_link_after(text_start, text_end)) {
                return parse_incomplete_bracket(out, run_start, start);
            }
            if (has_close_paren && ctx.resolve_link_label) {
                if (auto definition = resolve_reference_label(src.substr(text_start, text_end - text_start))) {
                    auto href = percent_encode_url(definition->href);
                    auto title = definition->title
                        ? std::optional<std::string>(*definition->title)
                        : std::nullopt;
                    if (emit_link_or_image(out, run_start, start, text_start, text_end,
                            after_text, is_image, std::move(href), std::move(title))) return true;
                }
            }
            return false;
        }

        std::size_t source_end = after_text;
        std::u32string_view label = src.substr(text_start, text_end - text_start);
        if (after_text < limit && src[after_text] == U'[') {
            const auto ref_close = matching_bracket(after_text, limit);
            if (ref_close == npos) {
                if (nested_link_after(text_start, text_end)) return false;
                return parse_incomplete_bracket(out, run_start, start);
            }
            const auto ref_text = src.substr(after_text + 1, ref_close - after_text - 1);
            if (!ref_text.empty()) label = ref_text;
            source_end = ref_close + 1;
        }

        if (ctx.resolve_link_label) {
            auto definition = resolve_reference_label(label);
            if (definition) {
                // Link-definition parsing already decodes escapes and HTML
                // character references. Only URL-attribute escaping remains.
                auto href = percent_encode_url(definition->href);
                auto title = definition->title
                    ? std::optional<std::string>(*definition->title)
                    : std::nullopt;
                if (emit_link_or_image(out, run_start, start, text_start, text_end,
                        source_end, is_image, std::move(href), std::move(title))) return true;
                return false;
            }
        }

        // A syntactically explicit but unresolved reference is literal; it does
        // not fall back to shortcut-reference interpretation.
        if (after_text < limit && src[after_text] == U'[') return false;
        return false;
    }

    // Emit an Incomplete node for a bracket opener that didn't close into a
    // link/image. Consumes the `[` (and `!` if image) and the rest of the line.
    bool parse_incomplete_bracket(InlineCstNodes& out, std::size_t& run_start, std::size_t start) {
        flush_text_to(out, run_start, start);
        pos = start;
        std::size_t opener_end = start + 1;
        if (ch(start) == U'!') opener_end = start + 2;
        // Consume to end of line (don't swallow following lines).
        std::size_t end = start;
        while (end < limit && src[end] != U'\n') ++end;
        pos = end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Incomplete;
        node.status = ParseStatus::MissingCloser;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, opener_end}, {opener_end, pos}, std::nullopt};
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- autolink <url> / <email> ----
    bool parse_autolink(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'<') return false;
        const auto start = pos;
        const auto content_start = start + 1;
        const auto close = next_angle_stopper(content_start, limit);
        if (close == npos || src[close] != U'>' || close == content_start) return false;
        const auto value = src.substr(content_start, close - content_start);
        const bool is_email = valid_email_autolink(value);
        if (!is_email && !valid_uri_autolink(value)) return false;

        const auto text = cps_to_utf8(value);
        const auto href = is_email
            ? std::string("mailto:") + percent_encode_url(text)
            : percent_encode_url(text);
        pos = close + 1;
        flush_text_to(out, run_start, start);
        const auto close_end = pos;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Autolink;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, close_end}, {start, content_start}, {content_start, close_end - 1}, SourceRange{close_end - 1, close_end}};
        node.ensure_semantics().href = href;
        node.children.push_back(InlineCstNode{next_id(), InlineCstKind::Text, {content_start, close_end - 1}, ParseStatus::Complete});
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    bool parse_gfm_bare_autolink(InlineCstNodes& out, std::size_t& run_start) {
        if (suppress_bare_autolinks || !ctx.dialect.gfm.autolinks
            || !inline_syntax_allows_markdown(ctx.syntax_mode)) return false;
        auto match = scan_gfm_autolink(src, pos, limit, scope_start);
        if (!match) return false;

        // cmark-gfm runs this extension over inline text nodes after ordinary
        // Markdown parsing. Keep the same boundary: a bare URL cannot swallow
        // code spans, emphasis, math, strike, or an explicit link in its tail.
        const auto& emphasis = emphasis_pairs();
        const auto* strikes = ctx.dialect.gfm.strikethrough
            ? &strikethrough_pairs() : nullptr;
        auto syntax_start = match->range.end;
        for (auto cursor = pos + 1; cursor < syntax_start;) {
            if (src[cursor] == U'\\' && cursor + 1 < syntax_start
                && src[cursor + 1] == U'(' && ctx.dialect.math.inline_paren) {
                syntax_start = cursor;
                break;
            }
            if (src[cursor] == U'\\' && cursor + 1 < syntax_start && ascii_punct(src[cursor + 1])) {
                cursor += 2;
                continue;
            }
            if (src[cursor] == U'`') { syntax_start = cursor; break; }
            if (src[cursor] == U'$' && ctx.dialect.math.inline_dollar
                && !(cursor + 1 < syntax_start && src[cursor + 1] == U'$')) {
                syntax_start = cursor;
                break;
            }
            if (src[cursor] == U'*' || src[cursor] == U'_') {
                const auto pair = std::ranges::lower_bound(emphasis, cursor, {}, &EmphasisPair::open_start);
                if (pair != emphasis.end() && pair->open_start == cursor) {
                    syntax_start = cursor;
                    break;
                }
            }
            if (src[cursor] == U'~' && strikes) {
                const auto pair = std::ranges::lower_bound(*strikes, cursor, {}, &StrikePair::open);
                if (pair != strikes->end() && pair->open == cursor) {
                    syntax_start = cursor;
                    break;
                }
            }
            const auto bracket = src[cursor] == U'!' && cursor + 1 < syntax_start && src[cursor + 1] == U'['
                ? cursor + 1 : cursor;
            if (src[bracket] == U'[') {
                const auto close = matching_bracket(bracket, syntax_start);
                if (close != npos && close + 1 < syntax_start && src[close + 1] == U'(') {
                    auto source_end = close + 1;
                    std::string href;
                    std::optional<std::string> title;
                    if (parse_inline_link_suffix(close + 1, syntax_start, source_end, href, title)) {
                        syntax_start = cursor;
                        break;
                    }
                }
            }
            ++cursor;
        }
        if (syntax_start < match->range.end) {
            match = scan_gfm_autolink(src, pos, syntax_start, scope_start);
            if (!match) return false;
        }

        flush_text_to(out, run_start, pos);
        pos = match->range.end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::Autolink;
        node.status = ParseStatus::Complete;
        node.range = match->range;
        node.ensure_delimiter_ranges() = {
            match->range,
            SourceRange{match->range.start, match->range.start},
            match->content_range,
            std::nullopt};
        node.ensure_semantics().href = normalize_link_destination(match->href);
        node.children.push_back(InlineCstNode{
            next_id(), InlineCstKind::Text, match->content_range, ParseStatus::Complete});
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    struct HtmlTag {
        std::string name;
        std::unordered_map<std::string, std::string> attributes;
        std::size_t start = 0;
        std::size_t end = 0;
        bool closing = false;
        bool self_closing = false;
    };

    static std::string lower_ascii(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::optional<HtmlTag> html_tag_at(std::size_t at, std::size_t parse_limit) const {
        const auto scanned = scan_markdown_html_tag(src, at, parse_limit);
        if (!scanned) return std::nullopt;
        HtmlTag tag;
        tag.start = scanned->range.start;
        tag.end = scanned->range.end;
        tag.name = scanned->name;
        tag.closing = scanned->closing;
        tag.self_closing = scanned->self_closing;
        for (const auto& attribute_range : scanned->attributes) {
            auto attribute = lower_ascii(cps_to_utf8(src.substr(
                attribute_range.name.start, attribute_range.name.length())));
            std::string value;
            if (attribute_range.value) {
                const auto& range = *attribute_range.value;
                value = cps_to_utf8(src.substr(range.start, range.length()));
            }
            // Event handlers never enter derived presentation state.  Safe
            // CSS declarations are filtered later by the HTML presentation
            // projector; exact spelling remains preserved by source ranges.
            if (!attribute.starts_with("on")) {
                tag.attributes[std::move(attribute)] = std::move(value);
            }
        }
        return tag;
    }

    std::optional<std::pair<std::size_t, std::size_t>> html_closing_tag(
        std::string_view name,
        std::size_t from) const {
        std::size_t depth = 1;
        for (auto cursor = from; cursor < limit; ++cursor) {
            if (src[cursor] != U'<') continue;
            const auto tag = html_tag_at(cursor, limit);
            if (!tag || tag->name != name) continue;
            if (tag->closing) {
                if (--depth == 0) return std::pair{cursor, tag->end};
            } else if (!tag->self_closing) {
                ++depth;
            }
            cursor = tag->end - 1;
        }
        return std::nullopt;
    }

    static bool safe_html_target(std::string_view value, bool image) {
        std::size_t start = 0;
        std::size_t end = value.size();
        while (start < end && static_cast<unsigned char>(value[start]) <= 0x20) ++start;
        while (end > start && static_cast<unsigned char>(value[end - 1]) <= 0x20) --end;
        auto lower = lower_ascii(std::string(value.substr(start, end - start)));
        const auto colon = lower.find(':');
        const auto boundary = lower.find_first_of("/?#");
        if (colon != std::string::npos && (boundary == std::string::npos || colon < boundary)) {
            const auto scheme = lower.substr(0, colon);
            if (scheme == "http" || scheme == "https" || (!image && scheme == "mailto")) return true;
            return image && scheme == "data" && lower.starts_with("data:image/");
        }
        return true;
    }

    static std::optional<ImageDimension> html_dimension(const HtmlTag& tag, std::string_view name) {
        const auto found = tag.attributes.find(std::string(name));
        if (found == tag.attributes.end()) return std::nullopt;
        return parse_html_image_dimension(found->second);
    }

    static bool safe_html_container(std::string_view name) {
        return name == "strong" || name == "b" || name == "em" || name == "i"
            || name == "cite" || name == "del" || name == "s" || name == "strike"
            || name == "ins" || name == "code" || name == "tt"
            || name == "a" || name == "span" || name == "abbr" || name == "small"
            || name == "sub" || name == "sup" || name == "mark" || name == "kbd"
            || name == "q" || name == "time" || name == "u" || name == "var"
            || name == "samp";
    }

    bool parse_html_special(InlineCstNodes& out, std::size_t& run_start) {
        const auto special = html_special_at(pos, limit);
        // Unfinished markup stays visible in the normal editable text run.
        if (!special) return false;
        flush_text_to(out, run_start, pos);
        pos = special->range.end;
        InlineCstNode node;
        node.id = next_id();
        node.kind = special->kind == MarkdownHtmlSpecialKind::Comment
            ? InlineCstKind::HtmlComment : InlineCstKind::Raw;
        node.range = special->range;
        node.ensure_delimiter_ranges() = {special->range, special->opening,
            special->content, special->closing};
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    bool parse_html_element(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'<') return false;
        const auto start = pos;
        const auto tag = html_tag_at(start, limit);
        if (!tag) return false;

        // Complete tags remain opaque syntax even when they have no derived
        // presentation. In particular, brackets and delimiters in attributes
        // cannot become Markdown links or emphasis.
        if (tag->closing || (tag->name != "img" && tag->name != "br"
                && !safe_html_container(tag->name))) {
            flush_text_to(out, run_start, start);
            pos = tag->end;
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Raw;
            node.range = {start, pos};
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }

        flush_text_to(out, run_start, start);
        if (tag->name == "img" || tag->name == "br") {
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::HtmlElement;
            node.range = {start, tag->end};
            node.ensure_delimiter_ranges() = {{start, tag->end}, {start, tag->end}, {tag->end, tag->end}, std::nullopt};
            auto& semantic = node.ensure_semantics();
            semantic.html_tag = tag->name;
            semantic.html_attributes = tag->attributes;
            if (tag->name == "img") {
                if (const auto found = tag->attributes.find("src"); found != tag->attributes.end() && safe_html_target(found->second, true)) semantic.href = found->second;
                if (const auto found = tag->attributes.find("alt"); found != tag->attributes.end()) semantic.alt = found->second;
                if (const auto found = tag->attributes.find("title"); found != tag->attributes.end()) semantic.title = found->second;
                semantic.image_width = html_dimension(*tag, "width");
                semantic.image_height = html_dimension(*tag, "height");
            }
            pos = tag->end;
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }

        const auto closing = tag->self_closing
            ? std::optional<std::pair<std::size_t, std::size_t>>(std::pair{tag->end, tag->end})
            : html_closing_tag(tag->name, tag->end);
        if (!closing) {
            pos = find_line_or_doc_end(tag->end);
            InlineCstNode node;
            node.id = next_id();
            node.kind = InlineCstKind::Incomplete;
            node.status = ParseStatus::MissingCloser;
            node.range = {start, pos};
            node.ensure_delimiter_ranges() = {{start, pos}, {start, tag->end}, {tag->end, pos}, std::nullopt};
            auto& semantic = node.ensure_semantics();
            semantic.html_tag = tag->name;
            semantic.html_attributes = tag->attributes;
            if (inline_syntax_allows_markdown(ctx.syntax_mode)) {
                Parser inner{src, ctx};
                inner.suppress_bare_autolinks = suppress_bare_autolinks || tag->name == "a";
                inner.counter = counter;
                inner.pos = tag->end;
                node.children = inner.parse_until(pos);
                counter = inner.counter;
            }
            out.push_back(std::move(node));
            run_start = pos;
            return true;
        }

        const auto content_start = tag->end;
        const auto content_end = closing->first;
        const auto source_end = closing->second;
        InlineCstNodes children;
        auto html_context = ctx;
        // Inline HTML does not switch off Markdown in its body. HtmlText is
        // selected by the block parser only for an actual raw HTML block.
        html_context.syntax_mode = ctx.syntax_mode;
        Parser inner{src, html_context};
        inner.suppress_bare_autolinks = suppress_bare_autolinks || tag->name == "a";
        inner.counter = counter;
        inner.pos = content_start;
        children = inner.parse_until(content_end);
        counter = inner.counter;

        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::HtmlElement;
        node.range = {start, source_end};
        node.ensure_delimiter_ranges() = {{start, source_end}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, source_end}};
        node.children = std::move(children);
        auto& semantic = node.ensure_semantics();
        semantic.html_tag = tag->name;
        semantic.html_attributes = tag->attributes;
        if (tag->name == "a") {
            if (const auto found = tag->attributes.find("href"); found != tag->attributes.end() && safe_html_target(found->second, false)) semantic.href = found->second;
            if (const auto found = tag->attributes.find("title"); found != tag->attributes.end()) semantic.title = found->second;
        }
        pos = source_end;
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- footnote ref [^label] ----
    bool parse_footnote_ref(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'[' || peek(1) != U'^') return false;
        std::size_t start = pos;
        advance(2);
        std::size_t label_start = pos;
        while (!eof() && peek() != U']' && peek() != U'\n') advance();
        if (peek() != U']') { pos = start; return parse_incomplete_bracket(out, run_start, start); }
        std::size_t label_end = pos;
        advance();
        flush_text_to(out, run_start, start);
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::FootnoteRef;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, label_start}, {label_start, label_end}, SourceRange{label_end, pos}};
        node.ensure_semantics().label = cps_to_utf8(src.substr(label_start, label_end - label_start));
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    // ---- wiki link [[target|alias]] / [[target]] ----
    bool parse_wiki_link(InlineCstNodes& out, std::size_t& run_start) {
        if (peek() != U'[' || peek(1) != U'[') return false;
        std::size_t start = pos;
        advance(2);
        std::size_t content_start = pos;
        std::u32string body;
        while (!eof() && !(peek() == U']' && peek(1) == U']') && peek() != U'\n') body.push_back(peek()), advance();
        if (!(peek() == U']' && peek(1) == U']')) { pos = start; return parse_incomplete_bracket(out, run_start, start); }
        std::size_t content_end = pos;
        advance(2);
        flush_text_to(out, run_start, start);
        std::string target;
        std::optional<std::string> alias;
        auto bar = body.find(U'|');
        if (bar != std::u32string::npos) {
            target = cps_to_utf8(body.substr(0, bar));
            alias = cps_to_utf8(body.substr(bar + 1));
        } else {
            target = cps_to_utf8(body);
        }
        InlineCstNode node;
        node.id = next_id();
        node.kind = InlineCstKind::WikiLink;
        node.status = ParseStatus::Complete;
        node.range = {start, pos};
        node.ensure_delimiter_ranges() = {{start, pos}, {start, content_start}, {content_start, content_end}, SourceRange{content_end, pos}};
        auto& semantic = node.ensure_semantics();
        semantic.target = std::move(target);
        semantic.alias = std::move(alias);
        out.push_back(std::move(node));
        run_start = pos;
        return true;
    }

    static bool delimiter_char(char32_t c) {
        switch (c) {
            case U'*': case U'_': case U'~': case U'`': case U'$':
            case U'[': case U']': case U'(': case U')': case U'!':
            case U'<': case U'>': case U'"': case U'\'':
                return true;
            default:
                return false;
        }
    }

    // Produce the lexical leaf stream independently of the structural parse.
    // Ranges are exhaustive and never overlap, so concatenating token source
    // slices is a real leaf-token flatten, not a top-level node shortcut.
    std::vector<InlineToken> tokenize() {
        std::vector<InlineToken> tokens;
        std::size_t cursor = 0;
        auto emit = [&](TokenKind kind, std::size_t, std::size_t end) {
            tokens.emplace_back(kind, end);
        };

        while (cursor < limit) {
            const auto start = cursor;
            const auto c = src[cursor];

            if (is_line_ending(c)) {
                std::size_t slash_count = 0;
                while (cursor > slash_count && src[cursor - slash_count - 1] == U'\\') ++slash_count;
                auto after_break = cursor + line_ending_length(cursor);
                if (inline_syntax_allows_markdown(ctx.syntax_mode)) {
                    while (after_break < limit && (src[after_break] == U' ' || src[after_break] == U'\t')) ++after_break;
                }
                const bool hard = inline_syntax_allows_markdown(ctx.syntax_mode)
                    && ((cursor >= 2 && src[cursor - 1] == U' ' && src[cursor - 2] == U' ')
                        || (slash_count % 2 == 1 && after_break < limit));
                cursor += line_ending_length(cursor);
                emit(hard ? TokenKind::HardBreak : TokenKind::SoftBreak, start, cursor);
                continue;
            }
            if (c == U' ' || c == U'\t') {
                while (cursor < limit && (src[cursor] == U' ' || src[cursor] == U'\t')) ++cursor;
                emit(TokenKind::Whitespace, start, cursor);
                continue;
            }
            if (c == U'\\' && cursor + 1 < limit && ascii_punct(src[cursor + 1])) {
                cursor += 2;
                emit(TokenKind::Escape, start, cursor);
                continue;
            }
            if (c == U'&') {
                const auto end = entity_end(src.substr(0, limit), cursor);
                if (end != npos) {
                    cursor = end;
                    emit(TokenKind::Entity, start, cursor);
                    continue;
                }
            }
            if (delimiter_char(c)) {
                while (cursor < limit && delimiter_char(src[cursor])) ++cursor;
                emit(TokenKind::Delimiter, start, cursor);
                continue;
            }

            ++cursor;
            while (cursor < limit) {
                const auto next = src[cursor];
                if (is_line_ending(next) || next == U' ' || next == U'\t' || delimiter_char(next)) break;
                if (next == U'\\' && cursor + 1 < limit && ascii_punct(src[cursor + 1])) break;
                if (next == U'&' && entity_end(src.substr(0, limit), cursor) != npos) break;
                ++cursor;
            }
            emit(TokenKind::Text, start, cursor);
        }
        return tokens;
    }

    // One dispatch engine for the source scope and its matched emphasis
    // views. Other constructs retain their existing independent Parser scopes.
    InlineCstNodes parse_scope() {
        struct EmphasisFrame {
            EmphasisPair pair;
            InlineCstNodes parent;
            std::size_t parent_scope_start;
            std::size_t parent_limit;
            std::optional<std::vector<StrikePair>> strikes;
            std::optional<std::vector<NestedLinkRange>> links;
            std::size_t link_scope_start;
            std::size_t link_limit;
        };
        std::vector<EmphasisFrame> frames;
        InlineCstNodes out;
        std::size_t run_start = pos;
        while (true) {
            if (eof()) {
                flush_text(out, run_start);
                if (frames.empty()) break;
                auto frame = std::move(frames.back());
                frames.pop_back();
                InlineCstNode node;
                node.id = next_id();
                node.kind = frame.pair.kind;
                node.status = ParseStatus::Complete;
                node.range = {frame.pair.open_start, frame.pair.close_end};
                node.ensure_delimiter_ranges() = {
                    node.range,
                    {frame.pair.open_start, frame.pair.open_end},
                    {frame.pair.open_end, frame.pair.close_start},
                    SourceRange{frame.pair.close_start, frame.pair.close_end}};
                node.children = std::move(out);
                out = std::move(frame.parent);
                scope_start = frame.parent_scope_start;
                limit = frame.parent_limit;
                strikethrough_pairs_cache = std::move(frame.strikes);
                nested_link_ranges_cache = std::move(frame.links);
                nested_link_index_scope_start = frame.link_scope_start;
                nested_link_index_limit = frame.link_limit;
                out.push_back(std::move(node));
                pos = frame.pair.close_end;
                run_start = pos;
                continue;
            }
            if (parse_break(out, run_start, inline_syntax_allows_markdown(ctx.syntax_mode))) continue;
            if (ctx.syntax_mode == InlineSyntaxMode::HtmlText) {
                if (parse_entity(out, run_start)) continue;
                if (peek() == U'<' && parse_html_special(out, run_start)) continue;
                if (peek() == U'<' && parse_html_element(out, run_start)) continue;
                advance();
                continue;
            }
            if (peek() == U'$' || (peek() == U'\\' && peek(1) == U'(')) { if (parse_math(out, run_start)) continue; }
            if (parse_escape(out, run_start)) continue;
            if (parse_entity(out, run_start)) continue;
            if (peek() == U'*' || peek() == U'_') {
                if (parse_commonmark_emphasis(out, run_start)) {
                    if (pending_emphasis) {
                        // Build source-scope indexes once before the first
                        // emphasis view narrows its bounds. Child views only
                        // filter these immutable matches.
                        (void)bracket_close_index();
                        (void)has_separate_inline_structure(scope_start);
                        const auto pair = *pending_emphasis;
                        pending_emphasis.reset();
                        frames.push_back({pair, std::move(out), scope_start, limit,
                            std::move(strikethrough_pairs_cache), std::move(nested_link_ranges_cache),
                            nested_link_index_scope_start, nested_link_index_limit});
                        strikethrough_pairs_cache.reset();
                        nested_link_ranges_cache.reset();
                        out = {};
                        scope_start = pair.open_end;
                        limit = pair.close_start;
                        run_start = pos;
                    }
                    continue;
                }
            }
            if (peek() == U'~' && parse_strikethrough(out, run_start)) continue;
            if (peek() == U'`') { if (parse_code_span(out, run_start)) continue; }
            if (peek() == U'!' && peek(1) == U'[') { if (parse_link_or_image(out, run_start)) continue; }
            if (peek() == U'[') {
                if (peek(1) == U'^') { if (parse_footnote_ref(out, run_start)) continue; }
                if (ctx.dialect.wiki_links && peek(1) == U'[') { if (parse_wiki_link(out, run_start)) continue; }
                if (parse_link_or_image(out, run_start)) continue;
            }
            if (peek() == U'<') {
                if (parse_html_special(out, run_start)) continue;
                if (parse_autolink(out, run_start)) continue;
                if (parse_html_element(out, run_start)) continue;
            }
            if (parse_gfm_bare_autolink(out, run_start)) continue;
            advance();
        }
        return out;
    }

    InlineCstTree parse_all() {
        auto nodes = parse_scope();
        return InlineCstTree{std::move(nodes), tokenize()};
    }

    InlineCstNodes parse_until(std::size_t requested_limit) {
        limit = (std::min)(limit, requested_limit);
        const auto previous_scope_start = scope_start;
        scope_start = pos;
        auto nodes = parse_scope();
        scope_start = previous_scope_start;
        return nodes;
    }

};

} // namespace inline_parser_detail

inline InlineCstTree parse_inline(std::u32string_view source, const InlineParseContext& ctx) {
    record_inline_reparse();
    inline_parser_detail::Parser parser{source, ctx};
    return parser.parse_all();
}

inline void reparse_inline_document(InlineDocument& document, const InlineParseContext& ctx) {
    auto local = ctx;
    local.syntax_mode = document.syntax_mode;
    document.tree = parse_inline(document.source, local);
}

} // namespace folia
