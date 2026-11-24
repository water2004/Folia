#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_formatting_properties_tests = [] {

"cst_format_toggle_is_identical_across_recursive_editable_contexts"_test = [] {
    struct Case { std::string source; std::string expected; };
    const std::vector<Case> cases{
        {"__value__", "value"},
        {"# __value__", "# value"},
        {"- __value__", "- value"},
        {"> __value__", "> value"},
        {"| __value__ |\n| --- |", "| value |\n| --- |"},
        {"> [!NOTE] __value__\n> body", "> [!NOTE] value\n> body"},
        {"> [!NOTE] title\n> __value__", "> [!NOTE] title\n> value"},
        {"[^n]: __value__", "[^n]: value"},
    };

    for (const auto& test : cases) {
        Editor editor(test.source);
        const auto fragments = document_text_fragments(editor.document());
        const auto fragment = std::ranges::find_if(fragments, [](const auto& candidate) {
            return candidate.text == U"__value__";
        });
        expect(fatal(bool(fragment != fragments.end()))) << test.source;
        if (fragment == fragments.end()) continue;
        const auto before = TextSelection{
            {fragment->container_id, 2, TextAffinity::Downstream},
            {fragment->container_id, 7, TextAffinity::Downstream},
        };
        editor.set_selection(before);

        reset_core_operation_counters();
        const auto transaction = editor.execute_document_toggle_inline_format(
            editor.selection(),
            InlineFormat::Strong);
        expect(fatal(bool(transaction.has_value()))) << test.source;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << test.source;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << test.source;
        expect(fatal(bool(counters.inline_reparses == 1u))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == test.expected))) << test.source;
        expect(fatal(bool(*editor.editable_source(fragment->container_id) == U"value"))) << test.source;
        expect(fatal(bool(validate_document(editor.document()).empty()))) << test.source;

        expect(fatal(bool(editor.undo()))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == test.source))) << test.source;
        expect(fatal(bool(editor.selection() == before))) << test.source;
        expect(fatal(bool(editor.redo()))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == test.expected))) << test.source;
    }
};

"random_cst_format_toggles_are_lossless_and_exactly_reversible"_test = [] {
    std::mt19937_64 random{0xF04A7u};
    const std::u32string alphabet = U"abc *_~`[]()!$\\&;'\"😀";
    const std::vector<InlineFormat> formats{
        InlineFormat::Emphasis,
        InlineFormat::Strong,
        InlineFormat::Strikethrough,
        InlineFormat::Code,
        InlineFormat::Math,
    };
    const auto make_document = [](std::size_t context, std::u32string_view source) {
        const auto text = cps_to_utf8(source);
        switch (context) {
            case 0: return text;
            case 1: return "# " + text;
            case 2: return "- " + text;
            case 3: return "> " + text;
            case 4: return "| " + text + " |\n| --- |";
            case 5: return "> [!NOTE] " + text + "\n> body";
            case 6: return "> [!NOTE] title\n> " + text;
            default: return "[^n]: " + text;
        }
    };

    for (std::size_t iteration = 0; iteration < 240; ++iteration) {
        std::u32string original = U"p";
        const auto length = static_cast<std::size_t>(random() % 32);
        for (std::size_t index = 0; index < length; ++index) {
            original.push_back(alphabet[random() % alphabet.size()]);
        }
        original.push_back(U'q');

        Editor editor(make_document(iteration % 8, original));
        const auto fragments = document_text_fragments(editor.document());
        const auto fragment = std::ranges::find_if(fragments, [&](const auto& candidate) {
            return candidate.text == original;
        });
        expect(fatal(bool(fragment != fragments.end()))) << iteration;
        if (fragment == fragments.end()) continue;

        const auto first = static_cast<std::size_t>(random() % (original.size() + 1));
        const auto second = static_cast<std::size_t>(random() % (original.size() + 1));
        TextSelection before{
            {fragment->container_id, first, TextAffinity::Downstream},
            {fragment->container_id, second, TextAffinity::Upstream},
        };
        editor.set_selection(before);
        const auto before_markdown = editor.markdown_utf8();

        reset_core_operation_counters();
        const auto transaction = editor.execute_document_toggle_inline_format(
            editor.selection(),
            formats[random() % formats.size()]);
        expect(fatal(bool(transaction.has_value()))) << iteration;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << iteration;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << iteration;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << iteration;
        expect(fatal(bool(counters.inline_reparses == 1u))) << iteration;
        expect(fatal(bool(validate_document(editor.document()).empty()))) << iteration;

        const auto source = editor.editable_source(fragment->container_id);
        expect(fatal(bool(source.has_value()))) << iteration;
        if (!source) continue;
        expect(fatal(bool(editor.selection().anchor.source_offset <= source->size()))) << iteration;
        expect(fatal(bool(editor.selection().active.source_offset <= source->size()))) << iteration;
        const auto* block = find_block(editor.document().root, fragment->container_id);
        expect(fatal(bool(block != nullptr))) << iteration;
        if (!block) continue;
        const auto* inline_document = editable_inline_document(*block);
        expect(fatal(bool(inline_document != nullptr))) << iteration;
        if (!inline_document) continue;
        expect(fatal(bool(flatten_tokens(inline_document->tree, inline_document->source)
            == inline_document->source))) << iteration;

        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(fatal(bool(editor.undo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == before))) << iteration;
        expect(fatal(bool(editor.redo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == after_selection))) << iteration;
    }
};

};
