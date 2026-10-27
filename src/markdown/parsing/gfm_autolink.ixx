// folia.markdown.gfm_autolink — bounded recognizer for GFM extended autolinks.
export module folia.markdown.gfm_autolink;
import std;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.markdown.html_entities;

export namespace folia {
struct GfmAutolinkMatch {
    SourceRange range;
    SourceRange content_range;
    std::u32string href;
};

// Recognizes a GFM bare link beginning exactly at start. Offsets count UTF-32
// source characters. The source remains authoritative and is never changed.
export std::optional<GfmAutolinkMatch> scan_gfm_autolink(
    std::u32string_view source, std::size_t start, std::size_t limit,
    std::size_t scope_start = 0);
}

namespace folia::gfm_autolink_detail {
inline bool alpha(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }
inline bool digit(char32_t c) { return c >= U'0' && c <= U'9'; }
inline bool alnum(char32_t c) { return alpha(c) || digit(c); }
inline char32_t lower(char32_t c) { return c >= U'A' && c <= U'Z' ? c + (U'a' - U'A') : c; }
inline bool space(char32_t c) { return is_commonmark_whitespace(c); }
inline bool host_char(char32_t c) {
    // utf8proc's punctuation check used by cmark-gfm does not classify symbols
    // (including supplementary-plane emoji) as punctuation.
    const bool punctuation = c < 0x10000 && unicode_punctuation_or_symbol(c);
    return !space(c) && !punctuation;
}
inline bool email_local(char32_t c) { return alnum(c) || c == U'.' || c == U'+' || c == U'-' || c == U'_'; }

inline bool starts_with(std::u32string_view s, std::size_t at, std::u32string_view word, bool insensitive = false) {
    if (at > s.size() || word.size() > s.size() - at) return false;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (insensitive ? lower(s[at + i]) != lower(word[i]) : s[at + i] != word[i]) return false;
    }
    return true;
}

// Mirrors cmark-gfm check_domain: underscores are accepted in earlier labels,
// but rejected in the final two labels (except on unusually long domains).
inline std::size_t domain_length(std::u32string_view s, std::size_t at, std::size_t limit, bool allow_single_label) {
    if (at >= limit) return 0;
    std::size_t dots = 0, penultimate_underscores = 0, final_underscores = 0;
    std::size_t i = at + 1;
    for (; i < limit; ++i) {
        const auto c = s[i];
        if (c == U'\\' && i + 1 < limit) { ++i; continue; }
        if (c == U'_') ++final_underscores;
        else if (c == U'.') { penultimate_underscores = final_underscores; final_underscores = 0; ++dots; }
        else if (!host_char(c) && c != U'-') break;
    }
    if (dots <= 10 && (penultimate_underscores || final_underscores)) return 0;
    if (!allow_single_label && dots == 0) return 0;
    if (i <= at) return 0;
    return i - at;
}

// Trim GFM's trailing punctuation while retaining balanced closing parentheses.
inline std::size_t trim_end(std::u32string_view s, std::size_t begin, std::size_t end) {
    std::size_t opens = 0, closes = 0;
    for (auto i = begin; i < end; ++i) {
        if (s[i] == U'(') ++opens;
        else if (s[i] == U')') ++closes;
        else if (s[i] == U'<') { end = i; break; }
    }
    while (end > begin) {
        const auto c = s[end - 1];
        if (c == U')' && closes > opens) { --closes; --end; continue; }
        if (c == U';') {
            auto alpha_start = end - 1;
            while (alpha_start > begin && alpha(s[alpha_start - 1])) --alpha_start;
            if (alpha_start < end - 1 && alpha_start > begin && s[alpha_start - 1] == U'&') {
                const auto entity = s.substr(alpha_start - 1, end - alpha_start + 1);
                if (decode_html_character_reference(entity)) return end;
                end = alpha_start - 1;
                continue;
            }
        }
        if (c == U';' || c == U'?' || c == U'!' || c == U'.' || c == U',' || c == U':' || c == U'*' || c == U'_' || c == U'~' || c == U'\'' || c == U'"') {
            if (end >= begin + 2 && s[end - 2] == U'\\') end -= 2;
            else --end;
            continue;
        }
        break;
    }
    return end;
}

inline std::optional<GfmAutolinkMatch> result(
    std::size_t start, std::size_t end, std::u32string href) {
    if (end <= start) return std::nullopt;
    return GfmAutolinkMatch{{start, end}, {start, end}, std::move(href)};
}
}

namespace folia {
inline std::optional<GfmAutolinkMatch> scan_gfm_autolink(
    std::u32string_view s, std::size_t start, std::size_t limit, std::size_t scope_start) {
    using namespace gfm_autolink_detail;
    limit = (std::min)(limit, s.size());
    if (scope_start > start || start >= limit) return std::nullopt;

    std::size_t protocol_length = 0;
    std::u32string href_prefix;
    bool www = false;
    if (starts_with(s, start, U"www.")) { protocol_length = 4; href_prefix = U"http://"; www = true; }
    else if (starts_with(s, start, U"http://", true)) { protocol_length = 7; href_prefix.assign(s.substr(start, protocol_length)); }
    else if (starts_with(s, start, U"https://", true)) { protocol_length = 8; href_prefix.assign(s.substr(start, protocol_length)); }
    else if (starts_with(s, start, U"ftp://", true)) { protocol_length = 6; href_prefix.assign(s.substr(start, protocol_length)); }

    if (protocol_length) {
        if (www) {
            if (start > scope_start && !(space(s[start - 1]) || s[start - 1] == U'(' || s[start - 1] == U'*' || s[start - 1] == U'_' || s[start - 1] == U'~')) return std::nullopt;
        } else if (start && alnum(s[start - 1])) return std::nullopt;

        const auto host_start = start + protocol_length;
        if (!www && (host_start >= limit || !host_char(s[host_start]))) return std::nullopt;
        const auto domain_start = www ? start : host_start;
        const auto host_length = domain_length(s, domain_start, limit, !www);
        if (!host_length) return std::nullopt;
        auto end = domain_start + host_length;
        while (end < limit && !space(s[end]) && s[end] != U'<') ++end;
        end = trim_end(s, start, end);
        if (end <= host_start) return std::nullopt;
        std::u32string href = std::move(href_prefix);
        href.append(s.substr(www ? start : host_start, end - (www ? start : host_start)));
        return result(start, end, std::move(href));
    }

    const bool mailto = starts_with(s, start, U"mailto:");
    const bool xmpp = starts_with(s, start, U"xmpp:");
    if ((mailto || xmpp) && start && alnum(s[start - 1])) return std::nullopt;
    const auto body = start + (mailto ? 7u : xmpp ? 5u : 0u);
    std::size_t at = body;
    if (mailto || xmpp) {
        while (at < limit && s[at] != U'@' && !space(s[at]) && s[at] != U'<') ++at;
    } else {
        if (!email_local(s[start])) return std::nullopt;
        if (start && email_local(s[start - 1])) return std::nullopt;
        while (at < limit && email_local(s[at])) ++at;
    }
    if (at == body || at >= limit || s[at] != U'@' || !alnum(s[at - 1])) return std::nullopt;

    std::size_t end = at + 1;
    unsigned dots = 0;
    bool has_xmpp_path = false;
    while (end < limit) {
        const auto c = s[end];
        if (alnum(c)) { ++end; continue; }
        if (c == U'.' && end + 1 < limit && alnum(s[end + 1])) { ++dots; ++end; continue; }
        if (c == U'-' || c == U'_') { ++end; continue; }
        if (c == U'/' && xmpp && !has_xmpp_path) { has_xmpp_path = true; ++end; continue; }
        if (c == U'@' && xmpp && has_xmpp_path) { ++end; continue; }
        break;
    }
    if (!dots || end <= at + 1 || !(alpha(s[end - 1]) || s[end - 1] == U'.')) return std::nullopt;
    end = trim_end(s, start, end);
    if (end <= at + 1) return std::nullopt;

    std::u32string href;
    const auto shown = s.substr(start, end - start);
    if (mailto || xmpp) href.assign(shown);
    else { href = U"mailto:"; href.append(shown); }
    return result(start, end, std::move(href));
}
}
