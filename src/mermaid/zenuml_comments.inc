// ZenUML comments preserve the source's display whitespace and use marked's
// inline code/emphasis semantics (src/svg/components/comment.ts, @zenuml/core
// 3.49.2). Flowchart labels deliberately collapse this whitespace instead.
#include "diagrams/flowchart/flowchart_markdown.inc"

std::vector<FlowchartLabelLine> zen_comment_runs(std::u16string_view source) {
    std::u16string prepared;
    const auto encoded = [&](std::u16string_view text) {
        for (const auto character : text) {
            if (character < 128) {
                const auto digits = std::to_string(static_cast<unsigned>(character));
                prepared += u"&#" + std::u16string(digits.begin(), digits.end()) + u";";
            } else
                prepared += character;
        }
    };
    const auto literal = [&](char16_t character) {
        if (character == u'&')
            prepared += u"&amp;";
        else if (character == u'<')
            prepared += u"&lt;";
        else if (character == u'>')
            prepared += u"&gt;";
        else
            prepared += character;
    };
    for (std::size_t i = 0; i < source.size();) {
        if (source[i] == u'\\' && i + 1 < source.size() &&
            std::u16string_view(u"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~").find(source[i + 1]) !=
                std::u16string_view::npos) {
            // renderComment retains the raw escape token, including its slash.
            // Protect it before the shared inline-emphasis parser consumes it.
            encoded(source.substr(i, 2));
            i += 2;
            continue;
        }
        if (source[i] != u'`') {
            literal(source[i++]);
            continue;
        }
        const auto start = i;
        while (i < source.size() && source[i] == u'`')
            ++i;
        const auto delimiter = source.substr(start, i - start);
        auto end = source.find(delimiter, i);
        while (end != source.npos &&
               ((end > i && source[end - 1] == u'`') ||
                (end + delimiter.size() < source.size() && source[end + delimiter.size()] == u'`'))) {
            end = source.find(delimiter, end + delimiter.size());
        }
        if (end == source.npos) {
            prepared += delimiter;
            continue;
        }
        std::u16string content(source.substr(i, end - i));
        std::ranges::replace(content, u'\n', u' ');
        if (content.size() > 1 && content.front() == u' ' && content.back() == u' ' &&
            std::ranges::any_of(content, [](char16_t character) { return character != u' '; })) {
            content = content.substr(1, content.size() - 2);
        }
        // Encode code-span punctuation before emphasis matching. Numeric
        // references are decoded once below, so asterisks/backticks inside code
        // remain literal without losing emphasis surrounding the code span.
        for (const auto character : content) {
            // marked has already escaped codespan.text when renderComment
            // passes it through esc again. Preserve that displayed entity text.
            switch (character) {
            case u'&':
                encoded(u"&amp;");
                break;
            case u'<':
                encoded(u"&lt;");
                break;
            case u'>':
                encoded(u"&gt;");
                break;
            case u'\"':
                encoded(u"&quot;");
                break;
            case u'\'':
                encoded(u"&#39;");
                break;
            default:
                encoded(std::u16string_view(&character, 1));
                break;
            }
        }
        i = end + delimiter.size();
    }
    const auto markdown = markdown_label_html(prepared);
    std::vector<FlowchartLabelLine> lines(1);
    unsigned strong{}, emphasis{};
    const auto append = [&](char16_t character) {
        if (character == u'\n') {
            lines.emplace_back();
            return;
        }
        auto &line = lines.back();
        if (line.empty() || line.back().strong != bool(strong) || line.back().emphasis != bool(emphasis)) {
            line.push_back({{}, bool(strong), bool(emphasis)});
        }
        line.back().text += character;
    };
    for (std::size_t i = 0; i < markdown.size();) {
        const auto rest = std::u16string_view(markdown).substr(i);
        if (rest.starts_with(u"<strong>")) {
            ++strong;
            i += 8;
        } else if (rest.starts_with(u"</strong>")) {
            --strong;
            i += 9;
        } else if (rest.starts_with(u"<em>")) {
            ++emphasis;
            i += 4;
        } else if (rest.starts_with(u"</em>")) {
            --emphasis;
            i += 5;
        } else if (rest.starts_with(u"&amp;")) {
            append(u'&');
            i += 5;
        } else if (rest.starts_with(u"&lt;")) {
            append(u'<');
            i += 4;
        } else if (rest.starts_with(u"&gt;")) {
            append(u'>');
            i += 4;
        } else if (rest.starts_with(u"&#")) {
            const auto end = rest.find(u';');
            if (end == rest.npos)
                throw std::runtime_error("Invalid ZenUML comment escape");
            const auto digits = rest.substr(2, end - 2);
            unsigned value = 0;
            for (const auto digit : digits) {
                if (digit < u'0' || digit > u'9' || value > 127) {
                    throw std::runtime_error("Invalid ZenUML comment escape");
                }
                value = value * 10 + digit - u'0';
            }
            if (value > 127)
                throw std::runtime_error("Invalid ZenUML comment escape");
            append(static_cast<char16_t>(value));
            i += end + 1;
        } else
            append(markdown[i++]);
    }
    return lines;
}
