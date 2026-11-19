// Mermaid 12.2.0 Info parser; MIT; see ../LICENSE.
module folia.mermaid.info;
import std;

namespace folia::mermaid {
namespace {
std::size_t skip_space(std::u16string_view s, std::size_t p) {
    while (p < s.size() && (s[p] == u' ' || s[p] == u'\t')) ++p;
    return p;
}
void skip_newlines(std::u16string_view s, std::size_t& p) {
    while (p < s.size() && (s[p] == u'\r' || s[p] == u'\n')) {
        if (s[p++] == u'\r' && p < s.size() && s[p] == u'\n') ++p;
    }
}
bool word_at(std::u16string_view s, std::size_t p, std::u16string_view word) {
    return s.substr(p, word.size()) == word &&
        (p + word.size() == s.size() || s[p + word.size()] == u' ' || s[p + word.size()] == u'\t' ||
         s[p + word.size()] == u'\r' || s[p + word.size()] == u'\n');
}
}

std::expected<InfoDocument, InfoError> parse_info(std::u16string_view source) {
    InfoDocument result;
    std::size_t p = 0;
    p = skip_space(source, p);
    skip_newlines(source, p);
    p = skip_space(source, p);
    if (!word_at(source, p, u"info"))
        return std::unexpected(InfoError{p, u"Expected 'info'."});
    p += 4;
    skip_space(source, p);
    skip_newlines(source, p);
    p = skip_space(source, p);
    if (word_at(source, p, u"showInfo")) {
        result.show_info = true;
        p += 8;
    }
    while (p < source.size()) {
        skip_space(source, p);
        if (p == source.size()) break;
        if (source[p] == u'\r' || source[p] == u'\n') { skip_newlines(source, p); continue; }
        auto end = source.find_first_of(u"\r\n", p);
        if (end == std::u16string_view::npos) end = source.size();
        auto line = source.substr(p, end-p);
        auto assign = [&](std::u16string_view prefix, std::u16string& target) {
            if (line.starts_with(prefix) && (line.size()==prefix.size() || line[prefix.size()]==u' ' ||
                line[prefix.size()]==u'\t' || line[prefix.size()]==u':')) {
                auto k=prefix.size(); while(k<line.size()&&(line[k]==u' '||line[k]==u'\t'))++k;
                if(k<line.size()&&line[k]==u':')++k;
                while(k<line.size()&&(line[k]==u' '||line[k]==u'\t'))++k;
                target.assign(line.substr(k)); return true;
            }
            return false;
        };
        if (!assign(u"title", result.title) && !assign(u"accTitle", result.acc_title) &&
            !assign(u"accDescr", result.acc_description))
            return std::unexpected(InfoError{p, u"Unexpected content in Info input."});
        p = end;
    }
    return result;
}
}
