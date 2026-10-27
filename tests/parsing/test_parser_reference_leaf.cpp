#include <string>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

namespace {
const BlockNode* find_link_definition(const std::vector<BlockNode>& blocks) {
    for (const auto& block : blocks) {
        if (block.kind == BlockKind::LinkDefinition) return &block;
        if (const auto* found = find_link_definition(block.children)) return found;
    }
    return nullptr;
}

const InlineCstNode* first_reference_link(const InlineDocument& document) {
    for (const auto& node : document.tree.nodes)
        if (node.kind == InlineCstKind::Link) return &node;
    return nullptr;
}
}

suite reference_leaf_tests = [] {

"forward_reference_resolves_after_leaf_definition_collection"_test = [] {
    const std::string source = "[multi label]\n\n[multi\n label]: /url \"two\n lines\"\n";
    const auto parsed = parse_text(1, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    expect(parsed.document.root.children.size() == 2u);
    if (parsed.document.root.children.size() != 2u) return;
    const auto& use = parsed.document.root.children.front();
    expect(use.kind == BlockKind::Paragraph);
    expect(use.inline_content.source == U"[multi label]");
    const auto* link = first_reference_link(use.inline_content);
    expect(fatal(link != nullptr));
    if (link) {
        expect(link->semantics().href == "/url");
    }
    const auto* definition = find_link_definition(parsed.document.root.children);
    expect(fatal(definition != nullptr));
    if (definition) expect(definition->atomic_special().raw == "[multi\n label]: /url \"two\n lines\"\n");
};

"physical_line_endings_survive_multiline_destination_and_title"_test = [] {
    const std::string source = "[ref]:\r\n <a&b>\r\n 'one\r\ntwo'\r\n\r\n[ref]";
    const auto parsed = parse_text(2, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    const auto* definition = find_link_definition(parsed.document.root.children);
    expect(fatal(definition != nullptr));
    if (definition) expect(definition->atomic_special().raw == "[ref]:\r\n <a&b>\r\n 'one\r\ntwo'\r\n");
};

"cr_only_source_keeps_definition_eol_and_resolves_reference"_test = [] {
    const std::string source = "[cr]: /one\r\r[cr]";
    const auto parsed = parse_text(6, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    const auto* definition = find_link_definition(parsed.document.root.children);
    expect(fatal(definition != nullptr));
    if (definition) expect(definition->atomic_special().raw == "[cr]: /one\r");
    const auto& use = parsed.document.root.children.back();
    const auto* link = first_reference_link(use.inline_content);
    expect(fatal(link != nullptr));
    if (link) expect(link->semantics().href == "/one");
};

"blockquote_definitions_use_deprefixed_lines_and_keep_container_source"_test = [] {
    const std::string source = "> [q]: /inside\r\n>\r\n> [q]";
    const auto parsed = parse_text(3, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    expect(parsed.document.root.children.size() == 1u);
    if (parsed.document.root.children.size() != 1u) return;
    const auto& quote = parsed.document.root.children.front();
    expect(quote.kind == BlockKind::BlockQuote);
    const auto* link = quote.children.empty() ? nullptr : first_reference_link(quote.children.back().inline_content);
    expect(fatal(link != nullptr));
    if (link) {
        expect(link->semantics().href == "/inside");
    }
};

"definition_without_final_eol_has_other_completion_and_exact_raw"_test = [] {
    const std::string source = "[empty]: <>";
    const auto parsed = parse_text(4, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    const auto* definition = find_link_definition(parsed.document.root.children);
    expect(fatal(definition != nullptr));
    if (definition) expect(definition->atomic_special().raw == source);
};

"invalid_blank_multiline_title_is_not_consumed_as_a_definition"_test = [] {
    const std::string source = "[ref]: /url \"bad\n\n title\"\n";
    const auto parsed = parse_text(5, source);
    expect(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source)));
    expect(find_link_definition(parsed.document.root.children) == nullptr);
};

};
