#include "support/folia_test.hpp"
import folia.core.document_edit;
import folia.core.source_projection;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void callout_source_roundtrip(std::u32string_view source) {
    const auto bytes = cps_to_utf8(source);
    const auto parsed = parse_text(1, bytes);
    const auto projection = serialize_markdown_projection(parsed.document);
    if (projection.text != source)
        std::cerr << "callout source before " << std::quoted(bytes)
            << " after " << std::quoted(cps_to_utf8(projection.text)) << '\n';
    expect(projection.text == source);
    expect(serialize_markdown(parsed.document) == bytes);
    expect(validate_document(parsed.document).empty());
    const auto reloaded = parse_text(2, serialize_markdown(parsed.document));
    expect(serialize_markdown(reloaded.document) == bytes);

    SourceProjectionIndex index;
    index.reset(parsed.document, projection);
    walk_blocks(parsed.document.root, [&](const BlockNode& block) {
        const auto* content = editable_inline_document(block);
        if (!content) return;
        expect(flatten_tokens(content->tree, content->source) == content->source);
        const auto mapped = index.offset(TextPosition{block.id, 0});
        expect(mapped.has_value());
        if (mapped) expect(*mapped <= source.size());
        if (content->source == U"body") {
            const auto inside = index.offset(TextPosition{block.id, 2});
            expect(inside == std::optional<std::size_t>{source.find(U"body") + 2});
        }
    });
}

std::u32string nest_callout(std::u32string_view source, bool list) {
    std::u32string result;
    std::size_t begin = 0;
    do {
        result += list ? (begin == 0 ? U"- " : U"  ") : U"> ";
        auto end = source.find_first_of(U"\r\n", begin);
        if (end == std::u32string::npos) { result += source.substr(begin); break; }
        ++end;
        if (source[end - 1] == U'\r' && end < source.size() && source[end] == U'\n') ++end;
        result += source.substr(begin, end - begin);
        begin = end;
    } while (begin < source.size());
    return result;
}

}

suite callout_source_roundtrip_tests = [] {

"callout_source_roundtrip preserves physical endings blank markers and empty source owners"_test = [] {
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto title : {std::u32string{}, std::u32string{U" title"}})
    for (const auto indent : {std::u32string{}, std::u32string{U"  "}}) {
        const auto header = indent + U"> [!NOTE]" + title;
        const auto quote = indent + U">";
        for (const auto& tail : std::vector<std::u32string>{
            U"", ending, ending + quote, ending + quote + U" ", ending + quote + U"\t",
            ending + quote + U" body", ending + quote + ending + quote + U" body",
            ending + quote + ending + quote + ending + quote + U" body" + ending + quote,
        }) callout_source_roundtrip(header + tail);
    }
};

"callout_source_roundtrip preserves separators inside enclosing quotes and list items"_test = [] {
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto title : {std::u32string{}, std::u32string{U" title"}})
    for (const auto list : {false, true}) {
        const auto header = U"> [!NOTE]" + title;
        for (const auto& tail : std::vector<std::u32string>{
            U"", ending + U"> ", ending + U"> body", ending + U">" + ending + U"> body" + ending + U">",
        }) callout_source_roundtrip(nest_callout(header + tail, list));
    }
};

};
