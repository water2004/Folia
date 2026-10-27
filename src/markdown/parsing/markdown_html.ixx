// Shared CommonMark 0.31.2 raw HTML open/closing-tag scanner.
export module folia.markdown.markdown_html;
import std;
import folia.markdown.text_edit;

export namespace folia {

struct MarkdownHtmlAttribute {
    SourceRange name;
    std::optional<SourceRange> value;
};

struct MarkdownHtmlTag {
    SourceRange range;
    std::string name;
    bool closing = false;
    bool self_closing = false;
    std::vector<MarkdownHtmlAttribute> attributes;
};

enum class MarkdownHtmlBlockKind {
    RawText,
    Comment,
    ProcessingInstruction,
    Declaration,
    Cdata,
    BlockTag,
    OtherTag,
};

struct MarkdownHtmlBlockStart {
    MarkdownHtmlBlockKind kind = MarkdownHtmlBlockKind::OtherTag;
    // Source range of the start construct (or recognized tag prefix).
    // The source remains owned by its original document/node.
    SourceRange tag_start;
};

enum class MarkdownHtmlSpecialKind { Comment, ProcessingInstruction, Declaration, Cdata };

struct MarkdownHtmlSpecial {
    MarkdownHtmlSpecialKind kind;
    SourceRange range;
    SourceRange opening;
    SourceRange content;
    SourceRange closing;
};

// A source-local index for repeated special-construct queries. Inline parsing
// can encounter many openers with no terminator; indexing each possible close
// once avoids rescanning the remaining source from every opener.
class MarkdownHtmlSpecialIndex {
    std::vector<std::size_t> comments_;
    std::vector<std::size_t> processing_instructions_;
    std::vector<std::size_t> cdata_;
    std::vector<std::size_t> declarations_;

public:
    explicit MarkdownHtmlSpecialIndex(std::u32string_view source) {
        for (std::size_t at = 0; at < source.size(); ++at) {
            if (at + 3 <= source.size() && source.substr(at, 3) == U"-->") comments_.push_back(at);
            if (at + 2 <= source.size() && source.substr(at, 2) == U"?>") processing_instructions_.push_back(at);
            if (at + 3 <= source.size() && source.substr(at, 3) == U"]]>") cdata_.push_back(at);
            if (source[at] == U'>') declarations_.push_back(at);
        }
    }

    std::size_t first_after(MarkdownHtmlSpecialKind kind, std::size_t at, std::size_t limit) const {
        const std::vector<std::size_t>* positions = nullptr;
        std::size_t terminator_length = 0;
        switch (kind) {
            case MarkdownHtmlSpecialKind::Comment: positions = &comments_; terminator_length = 3; break;
            case MarkdownHtmlSpecialKind::ProcessingInstruction: positions = &processing_instructions_; terminator_length = 2; break;
            case MarkdownHtmlSpecialKind::Cdata: positions = &cdata_; terminator_length = 3; break;
            case MarkdownHtmlSpecialKind::Declaration: positions = &declarations_; terminator_length = 1; break;
        }
        const auto found = std::lower_bound(positions->begin(), positions->end(), at);
        return found != positions->end() && *found <= limit
                && terminator_length <= limit - *found
            ? *found : std::u32string_view::npos;
    }
};

namespace markdown_html_detail {

inline bool ascii_letter(char32_t value) {
    return (value >= U'A' && value <= U'Z') || (value >= U'a' && value <= U'z');
}

inline bool tag_name_character(char32_t value) {
    return ascii_letter(value)
        || (value >= U'0' && value <= U'9')
        || value == U'-';
}

inline bool attribute_name_start(char32_t value) {
    return ascii_letter(value) || value == U'_' || value == U':';
}

inline bool attribute_name_character(char32_t value) {
    return attribute_name_start(value)
        || (value >= U'0' && value <= U'9')
        || value == U'.' || value == U'-';
}

inline bool horizontal_space(char32_t value) {
    return value == U' ' || value == U'\t';
}

inline bool line_ending(char32_t value) {
    return value == U'\r' || value == U'\n';
}

struct WhitespaceScan {
    std::size_t end = 0;
    bool present = false;
    bool valid = true;
};

inline WhitespaceScan scan_whitespace(
    std::u32string_view source,
    std::size_t start,
    std::size_t limit) {
    WhitespaceScan result{start, false, true};
    std::size_t line_endings = 0;
    while (result.end < limit) {
        const auto value = source[result.end];
        if (horizontal_space(value)) {
            result.present = true;
            ++result.end;
        } else if (line_ending(value)) {
            result.present = true;
            if (++line_endings > 1) {
                result.valid = false;
                return result;
            }
            if (value == U'\r' && result.end + 1 < limit
                && source[result.end + 1] == U'\n') {
                result.end += 2;
            } else {
                ++result.end;
            }
        } else {
            break;
        }
    }
    return result;
}

inline bool unquoted_value_character(char32_t value) {
    return !horizontal_space(value) && !line_ending(value)
        && value != U'"' && value != U'\'' && value != U'='
        && value != U'<' && value != U'>' && value != U'`';
}

inline std::optional<MarkdownHtmlTag> scan_tag(
    std::u32string_view source,
    std::size_t start,
    std::size_t limit) {
    limit = (std::min)(limit, source.size());
    if (start >= limit || source[start] != U'<') return std::nullopt;

    auto cursor = start + 1;
    bool closing = false;
    if (cursor < limit && source[cursor] == U'/') {
        closing = true;
        ++cursor;
    }

    if (cursor >= limit || !ascii_letter(source[cursor])) return std::nullopt;
    const auto name_start = cursor++;
    while (cursor < limit && tag_name_character(source[cursor])) ++cursor;
    const auto name_end = cursor;
    std::string name;
    name.reserve(name_end - name_start);
    for (auto index = name_start; index < name_end; ++index) {
        auto value = source[index];
        if (value >= U'A' && value <= U'Z') value += U'a' - U'A';
        name.push_back(static_cast<char>(value));
    }

    MarkdownHtmlTag tag;
    tag.name = std::move(name);
    tag.closing = closing;

    if (closing) {
        const auto whitespace = scan_whitespace(source, cursor, limit);
        if (!whitespace.valid) return std::nullopt;
        cursor = whitespace.end;
        if (cursor >= limit || source[cursor] != U'>') return std::nullopt;
        tag.range = {start, cursor + 1};
        return tag;
    }

    bool separator_already_consumed = false;
    while (cursor < limit) {
        if (source[cursor] == U'>') {
            tag.range = {start, cursor + 1};
            return tag;
        }
        if (source[cursor] == U'/' && cursor + 1 < limit && source[cursor + 1] == U'>') {
            tag.self_closing = true;
            tag.range = {start, cursor + 2};
            return tag;
        }

        if (!separator_already_consumed) {
            const auto whitespace = scan_whitespace(source, cursor, limit);
            if (!whitespace.valid || !whitespace.present) return std::nullopt;
            cursor = whitespace.end;
            if (cursor >= limit) return std::nullopt;
            if (source[cursor] == U'>') {
                tag.range = {start, cursor + 1};
                return tag;
            }
            if (source[cursor] == U'/' && cursor + 1 < limit && source[cursor + 1] == U'>') {
                tag.self_closing = true;
                tag.range = {start, cursor + 2};
                return tag;
            }
        }
        separator_already_consumed = false;

        if (cursor >= limit || !attribute_name_start(source[cursor])) return std::nullopt;
        const auto attribute_name_start_offset = cursor++;
        while (cursor < limit && attribute_name_character(source[cursor])) ++cursor;
        const auto attribute_name_end_offset = cursor;
        const auto name_whitespace = scan_whitespace(source, cursor, limit);
        if (!name_whitespace.valid) return std::nullopt;
        cursor = name_whitespace.end;

        MarkdownHtmlAttribute attribute;
        attribute.name = {attribute_name_start_offset, attribute_name_end_offset};
        if (cursor < limit && source[cursor] == U'=') {
            ++cursor;
            const auto value_whitespace = scan_whitespace(source, cursor, limit);
            if (!value_whitespace.valid) return std::nullopt;
            cursor = value_whitespace.end;
            if (cursor >= limit) return std::nullopt;

            const auto quote = source[cursor];
            if (quote == U'\'' || quote == U'"') {
                const auto value_start = ++cursor;
                while (cursor < limit && source[cursor] != quote) ++cursor;
                if (cursor >= limit) return std::nullopt;
                attribute.value = SourceRange{value_start, cursor};
                ++cursor;
                tag.attributes.push_back(attribute);
                continue;
            }

            const auto value_start = cursor;
            while (cursor < limit && unquoted_value_character(source[cursor])) ++cursor;
            if (cursor == value_start) return std::nullopt;
            attribute.value = SourceRange{value_start, cursor};
            tag.attributes.push_back(attribute);
            continue;
        }

        tag.attributes.push_back(attribute);
        if (name_whitespace.present) {
            separator_already_consumed = true;
        }
    }
    return std::nullopt;
}

inline bool ascii_digit(char32_t value) {
    return value >= U'0' && value <= U'9';
}

inline bool ascii_alnum(char32_t value) {
    return ascii_letter(value) || ascii_digit(value);
}

inline bool ascii_equal_fold(char32_t left, char32_t right) {
    if (left >= U'A' && left <= U'Z') left += U'a' - U'A';
    if (right >= U'A' && right <= U'Z') right += U'a' - U'A';
    return left == right;
}

inline bool starts_ascii_case_insensitive(
    std::u32string_view source,
    std::size_t at,
    std::size_t limit,
    std::u32string_view value) {
    if (at > limit || value.size() > limit - at) return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (!ascii_equal_fold(source[at + index], value[index])) return false;
    }
    return true;
}

inline bool block_tag_name(std::u32string_view name) {
    // CommonMark 0.31.2 type-6 tag set. This is syntax recognition, not the
    // application's broader HTML presentation/block-element policy.
    static constexpr std::u32string_view names[] = {
        U"address", U"article", U"aside", U"base", U"basefont",
        U"blockquote", U"body", U"caption", U"center", U"col",
        U"colgroup", U"dd", U"details", U"dialog", U"dir", U"div",
        U"dl", U"dt", U"fieldset", U"figcaption", U"figure", U"footer",
        U"form", U"frame", U"frameset", U"h1", U"h2", U"h3", U"h4",
        U"h5", U"h6", U"head", U"header", U"hr", U"html", U"iframe",
        U"legend", U"li", U"link", U"main", U"menu", U"menuitem", U"nav",
        U"noframes", U"ol", U"optgroup", U"option", U"p", U"param",
        U"search", U"section", U"summary", U"table", U"tbody", U"td",
        U"tfoot", U"th", U"thead", U"title", U"tr", U"track", U"ul",
    };
    return std::ranges::any_of(names, [name](std::u32string_view candidate) {
        if (candidate.size() != name.size()) return false;
        for (std::size_t index = 0; index < name.size(); ++index) {
            if (!ascii_equal_fold(candidate[index], name[index])) return false;
        }
        return true;
    });
}

inline std::optional<MarkdownHtmlBlockStart> scan_block_start(
    std::u32string_view source,
    std::size_t line_start,
    std::size_t line_end) {
    if (line_start > line_end || line_end > source.size()) return std::nullopt;
    auto tag_start = line_start;
    std::size_t indentation = 0;
    while (tag_start < line_end && source[tag_start] == U' ' && indentation < 4) {
        ++tag_start;
        ++indentation;
    }
    if (indentation > 3 || tag_start >= line_end || source[tag_start] != U'<') {
        return std::nullopt;
    }

    auto result = [&](MarkdownHtmlBlockKind kind, std::size_t end) {
        return std::optional<MarkdownHtmlBlockStart>{
            MarkdownHtmlBlockStart{kind, {tag_start, end}}};
    };

    // Types 1-5 use their specified prefixes; later validity and termination
    // are independent from whether an HTML tag CST can be built.
    bool excluded_self_closing_raw_tag = false;
    static constexpr std::u32string_view raw_text_names[] = {U"pre", U"script", U"style", U"textarea"};
    for (const auto name : raw_text_names) {
        const auto name_start = tag_start + 1;
        if (!starts_ascii_case_insensitive(source, name_start, line_end, name)) continue;
        const auto after = name_start + name.size();
        if (after == line_end || horizontal_space(source[after])
            || line_ending(source[after]) || source[after] == U'>') {
            return result(MarkdownHtmlBlockKind::RawText, after);
        }
        // md4c/CommonMark explicitly excludes the self-closing form here.
        if (source[after] == U'/' && after + 1 < line_end
            && source[after + 1] == U'>') {
            excluded_self_closing_raw_tag = true;
            break;
        }
    }
    if (excluded_self_closing_raw_tag) return std::nullopt;
    if (tag_start + 4 <= line_end
        && source.substr(tag_start, 4) == U"<!--") {
        return result(MarkdownHtmlBlockKind::Comment, tag_start + 4);
    }
    if (tag_start + 2 <= line_end && source.substr(tag_start, 2) == U"<?") {
        return result(MarkdownHtmlBlockKind::ProcessingInstruction, tag_start + 2);
    }
    if (tag_start + 2 < line_end && source[tag_start] == U'<'
        && source[tag_start + 1] == U'!' && ascii_letter(source[tag_start + 2])) {
        return result(MarkdownHtmlBlockKind::Declaration, tag_start + 3);
    }
    constexpr std::u32string_view cdata = U"<![CDATA[";
    if (tag_start + cdata.size() <= line_end
        && source.substr(tag_start, cdata.size()) == cdata) {
        return result(MarkdownHtmlBlockKind::Cdata, tag_start + cdata.size());
    }

    // Type 6 permits either opening or closing tags and only the exact
    // CommonMark tag set. A valid tag body is not required after the prefix.
    auto cursor = tag_start + 1;
    if (cursor < line_end && source[cursor] == U'/') {
        ++cursor;
    }
    const auto name_start = cursor;
    if (cursor < line_end && ascii_letter(source[cursor])) ++cursor;
    while (cursor < line_end && (ascii_alnum(source[cursor]) || source[cursor] == U'-')) ++cursor;
    if (cursor > name_start) {
        auto name = source.substr(name_start, cursor - name_start);
        if (block_tag_name(name)) {
            const bool boundary = cursor == line_end || horizontal_space(source[cursor])
                || line_ending(source[cursor]) || source[cursor] == U'>'
                || (source[cursor] == U'/' && cursor + 1 < line_end
                    && source[cursor + 1] == U'>');
            if (boundary) return result(MarkdownHtmlBlockKind::BlockTag, cursor);
        }
    }

    // Type 7: one complete tag on this physical line, followed only by
    // CommonMark/HTML whitespace. Its paragraph-interruption exception is
    // expressed separately by markdown_html_block_can_interrupt().
    const auto tag = scan_tag(source, tag_start, line_end);
    if (tag) {
        bool single_physical_line = true;
        for (auto at = tag_start; at < tag->range.end; ++at) {
            if (line_ending(source[at])) {
                single_physical_line = false;
                break;
            }
        }
        auto after = tag->range.end;
        while (after < line_end && horizontal_space(source[after])) ++after;
        if (single_physical_line && after == line_end) {
            if (!tag->closing
                && (tag->name == "pre" || tag->name == "script"
                    || tag->name == "style" || tag->name == "textarea")) {
                return std::nullopt;
            }
            return result(MarkdownHtmlBlockKind::OtherTag, tag->range.end);
        }
    }
    return std::nullopt;
}

inline bool block_ends_on_line(
    MarkdownHtmlBlockKind kind,
    std::u32string_view source,
    std::size_t line_start,
    std::size_t line_end) {
    if (line_start > line_end || line_end > source.size()) return false;
    std::u32string_view terminator;
    switch (kind) {
        case MarkdownHtmlBlockKind::RawText: {
            constexpr std::u32string_view names[] = {U"</pre>", U"</script>", U"</style>", U"</textarea>"};
            for (const auto name : names) {
                for (auto at = line_start; at + name.size() <= line_end; ++at) {
                    if (starts_ascii_case_insensitive(source, at, line_end, name)) return true;
                }
            }
            return false;
        }
        case MarkdownHtmlBlockKind::Comment: terminator = U"-->"; break;
        case MarkdownHtmlBlockKind::ProcessingInstruction: terminator = U"?>"; break;
        case MarkdownHtmlBlockKind::Declaration: terminator = U">"; break;
        case MarkdownHtmlBlockKind::Cdata: terminator = U"]]>"; break;
        case MarkdownHtmlBlockKind::BlockTag:
        case MarkdownHtmlBlockKind::OtherTag:
            // Types 6 and 7 end at blank lines, handled by the block parser.
            return false;
    }
    if (terminator.size() > line_end - line_start) return false;
    return source.substr(line_start, line_end - line_start).find(terminator)
        != std::u32string_view::npos;
}

} // namespace markdown_html_detail

inline std::optional<MarkdownHtmlTag> scan_markdown_html_tag(
    std::u32string_view source,
    std::size_t start,
    std::size_t limit) {
    return markdown_html_detail::scan_tag(source, start, limit);
}

inline std::optional<MarkdownHtmlBlockStart> scan_markdown_html_block_start(
    std::u32string_view source,
    std::size_t line_start,
    std::size_t line_end) {
    return markdown_html_detail::scan_block_start(source, line_start, line_end);
}

inline bool markdown_html_block_can_interrupt(MarkdownHtmlBlockKind kind) {
    return kind != MarkdownHtmlBlockKind::OtherTag;
}

inline bool markdown_html_block_ends_on_line(
    MarkdownHtmlBlockKind kind,
    std::u32string_view source,
    std::size_t line_start,
    std::size_t line_end) {
    return markdown_html_detail::block_ends_on_line(kind, source, line_start, line_end);
}

// Complete non-element raw HTML constructs, with no decoding or body copies.
inline std::optional<MarkdownHtmlSpecial> scan_markdown_html_special(
    std::u32string_view source, std::size_t start, std::size_t limit,
    const MarkdownHtmlSpecialIndex* index = nullptr) {
    limit = (std::min)(limit, source.size());
    if (start >= limit || source[start] != U'<') return std::nullopt;
    const auto input = source.substr(start, limit - start);
    MarkdownHtmlSpecialKind kind;
    std::size_t opening_length = 0;
    std::u32string_view terminator;
    if (input.starts_with(U"<!--")) {
        kind = MarkdownHtmlSpecialKind::Comment;
        opening_length = 4;
        terminator = U"-->";
        if (input.starts_with(U"<!-->") || input.starts_with(U"<!--->")) {
            const auto end = start + (input.starts_with(U"<!-->") ? 5u : 6u);
            return MarkdownHtmlSpecial{kind, {start, end}, {start, start + 4},
                {start + 4, start + 4}, {start + 4, end}};
        }
    } else if (input.starts_with(U"<?")) {
        kind = MarkdownHtmlSpecialKind::ProcessingInstruction;
        opening_length = 2;
        terminator = U"?>";
    } else if (input.starts_with(U"<![CDATA[")) {
        kind = MarkdownHtmlSpecialKind::Cdata;
        opening_length = 9;
        terminator = U"]]>";
    } else if (input.size() > 2 && input.starts_with(U"<!")
            && markdown_html_detail::ascii_letter(input[2])) {
        kind = MarkdownHtmlSpecialKind::Declaration;
        opening_length = 2;
        terminator = U">";
    } else {
        return std::nullopt;
    }
    const auto close_at = index
        ? index->first_after(kind, start + opening_length, limit)
        : [&] {
            const auto found = input.find(terminator, opening_length);
            return found == std::u32string_view::npos ? found : start + found;
        }();
    if (close_at == std::u32string_view::npos) return std::nullopt;
    const auto close = close_at - start;
    const auto end = start + close + terminator.size();
    return MarkdownHtmlSpecial{kind, {start, end}, {start, start + opening_length},
        {start + opening_length, start + close}, {start + close, end}};
}

} // namespace folia
