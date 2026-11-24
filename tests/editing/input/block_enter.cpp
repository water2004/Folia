#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_history;
import folia.core.document_ids;
import folia.core.document_operation_apply;
import folia.markdown.document_symbols;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/document_edit_test_support.hpp"

suite editing_input_block_enter_tests = [] {

"typed_fence_enters_auto_closed_code_without_document_reparse"_test = [] {
    auto document = parse_document("");
    normalize_document(document);
    const auto initial = caret(first_editable(document));
    auto [opening, at_opening_end] = type_text(std::move(document), initial, U"```cpp");
    expect(fatal(bool(serialize_markdown(opening) == "```cpp")))
        << "opening=" << serialize_markdown(opening);
    expect(fatal(bool(at_opening_end.active.source_offset == 6u)));
    expect(fatal(bool(opening.root.children.front().kind == BlockKind::Paragraph)))
        << "opening-kind=" << static_cast<int>(opening.root.children.front().kind);
    auto entered = test_edit::document_enter(opening, at_opening_end);
    expect(fatal(bool(entered.has_value())));
    if (!entered) return;
    expect(fatal(bool(entered->after.root.children.front().kind == BlockKind::CodeBlock)));
    auto const& code = entered->after.root.children.front();
    expect(fatal(bool(code.block_source.source() == U"```cpp\n\n```")));
    expect(fatal(bool(code.block_source.tree().language == std::optional<std::string>{"cpp"})));
    expect(fatal(bool(code.block_source.tree().complete_opening)));
    expect(fatal(bool(code.block_source.tree().complete_closing)));
    expect(fatal(bool(block_source_tokens_partition(code.block_source))));
    expect(fatal(bool(flatten_block_source_tokens(code.block_source) == code.block_source.source())));
    expect(fatal(bool(entered->selection_after.active.container_id == entered->after.root.children.front().id)));
    expect(fatal(bool(entered->selection_after.active.source_offset == 7u)));

    auto second_enter = test_edit::document_enter(entered->after, entered->selection_after);
    expect(fatal(bool(second_enter.has_value())));
    if (!second_enter) return;
    expect(fatal(bool(second_enter->after.root.children.size() == 2u)));
    expect(fatal(bool(second_enter->after.root.children.front().block_source.source() == U"```cpp\n\n```")));
    expect(fatal(bool(second_enter->after.root.children.back().kind == BlockKind::Paragraph)));
    expect(fatal(bool(second_enter->selection_after.active.container_id
        == second_enter->after.root.children.back().id)));
    expect(fatal(bool(second_enter->selection_after.active.source_offset == 0u)));
    const auto serialized_after_exit = serialize_markdown(second_enter->after);
    expect(fatal(bool(serialized_after_exit == "```cpp\n\n```\n\n")))
        << "serialized=" << serialized_after_exit;
    expect_document_valid(second_enter->after);
};

"raw_block_source_projection_is_lossless_through_incomplete_character_edits"_test = [] {
    struct Case {
        BlockSourceKind kind;
        std::u32string source;
    };
    const std::vector<Case> cases{
        {BlockSourceKind::FencedCode, U"```cpp"},
        {BlockSourceKind::FencedCode, U"```cpp\n"},
        {BlockSourceKind::FencedCode, U"~~~js\nvalue\n~~"},
        {BlockSourceKind::DollarMath, U"$"},
        {BlockSourceKind::DollarMath, U"$$"},
        {BlockSourceKind::DollarMath, U"$$\n"},
        {BlockSourceKind::BracketMath, U"\\["},
        {BlockSourceKind::IndentedCode, U"    a\n  b\n"},
    };
    for (const auto& test_case : cases) {
        auto original = make_block_source(test_case.source, test_case.kind);
        expect(fatal(bool(block_source_tokens_partition(original))));
        expect(fatal(bool(flatten_block_source_tokens(original) == original.source())));
        for (std::size_t offset = 0; offset <= test_case.source.size(); ++offset) {
            auto inserted = original;
            inserted.source().insert(offset, 1, U'X');
            reparse_block_source(inserted);
            expect(fatal(bool(block_source_tokens_partition(inserted))));
            expect(fatal(bool(flatten_block_source_tokens(inserted) == inserted.source())));
        }
        for (std::size_t offset = 0; offset < test_case.source.size(); ++offset) {
            auto deleted = original;
            deleted.source().erase(offset, 1);
            reparse_block_source(deleted);
            expect(fatal(bool(block_source_tokens_partition(deleted))));
            expect(fatal(bool(flatten_block_source_tokens(deleted) == deleted.source())));
        }
    }
};

"enter_continues_and_exits_lists_by_tree_context"_test = [] {
    auto bullets = parse_document("- one");
    auto bullet_id = bullets.root.children.front().children.front().children.front().id;
    auto continued = test_edit::document_enter(bullets, TextSelection::caret({bullet_id, 3, TextAffinity::Downstream}));
    expect(fatal(bool(continued.has_value())));
    if (continued) {
        const auto& list = continued->after.root.children.front();
        expect(fatal(bool(list.children.size() == 2u)));
        expect(fatal(bool(list.children.back().item_special().marker == U"- ")));
        expect(fatal(bool(list.children.back().children.front().inline_content.source.empty())));
        expect(fatal(bool(continued->selection_after.active.container_id == list.children.back().children.front().id)));
    }

    auto ordered = parse_document("7) one");
    auto ordered_id = ordered.root.children.front().children.front().children.front().id;
    auto next_number = test_edit::document_enter(ordered, TextSelection::caret({ordered_id, 3, TextAffinity::Downstream}));
    expect(fatal(bool(next_number.has_value())));
    if (next_number) expect(fatal(bool(
        next_number->after.root.children.front().children.back().item_special().marker == U"8) ")));

    auto before_ordered = parse_document("7) one");
    auto before_ordered_id = before_ordered.root.children.front().children.front().children.front().id;
    auto inserted_before = test_edit::document_enter(before_ordered, TextSelection::caret({before_ordered_id, 0, TextAffinity::Downstream}));
    expect(fatal(bool(inserted_before.has_value())));
    if (inserted_before) {
        const auto& list = inserted_before->after.root.children.front();
        expect(fatal(bool(list.children.front().item_special().marker == U"7) ")));
        expect(fatal(bool(list.children.back().item_special().marker == U"8) ")));
        expect(fatal(bool(list.children.back().children.front().inline_content.source == U"one")));
    }

    auto tasks = parse_document("- [x] done");
    auto task_id = tasks.root.children.front().children.front().children.front().id;
    auto next_task = test_edit::document_enter(tasks, TextSelection::caret({task_id, 4, TextAffinity::Downstream}));
    expect(fatal(bool(next_task.has_value())));
    if (next_task) {
        const auto& item = next_task->after.root.children.front().children.back();
        expect(fatal(bool(item.kind == BlockKind::TaskListItem)));
        expect(fatal(bool(!item.item_special().checked)));
        expect(fatal(bool(item.item_special().marker == U"- [ ] ")));
    }

    auto empty = parse_document("- one\n- ");
    auto empty_id = empty.root.children.front().children.back().children.front().id;
    auto exited = test_edit::document_enter(empty, TextSelection::caret({empty_id, 0, TextAffinity::Downstream}));
    expect(fatal(bool(exited.has_value())));
    if (exited) {
        expect(fatal(bool(exited->after.root.children.size() == 2u)));
        expect(fatal(bool(exited->after.root.children.front().kind == BlockKind::List)));
        expect(fatal(bool(exited->after.root.children.front().children.size() == 1u)));
        expect(fatal(bool(exited->after.root.children.back().kind == BlockKind::Paragraph)));
        expect(fatal(bool(exited->selection_after.active.container_id == exited->after.root.children.back().id)));
    }
};

"enter_inside_nested_list_container_respects_nearest_owner"_test = [] {
    auto document = parse_document("- > quoted");
    auto& list = document.root.children.front();
    auto& quote = list.children.front().children.front();
    expect(fatal(bool(quote.kind == BlockKind::BlockQuote)));
    auto paragraph_id = quote.children.front().id;
    auto transaction = test_edit::document_enter(document, TextSelection::caret({paragraph_id, 6, TextAffinity::Downstream}));
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    const auto& updated_list = transaction->after.root.children.front();
    expect(fatal(bool(updated_list.children.size() == 1u)));
    const auto& updated_quote = updated_list.children.front().children.front();
    expect(fatal(bool(updated_quote.kind == BlockKind::BlockQuote)));
    expect(fatal(bool(updated_quote.children.size() == 2u)));
    expect_document_valid(transaction->after);
};

"enter_splits_source_without_serializing_the_document"_test = [] {
    auto document = parse_document("**alpha**");
    const auto original_id = first_editable(document).id;
    auto transaction = test_edit::document_enter(document, caret(first_editable(document), 5));
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(transaction->after.root.children.size() == 2u)));
    expect(fatal(bool(transaction->after.root.children[0].id == original_id)));
    expect(fatal(bool(transaction->after.root.children[0].inline_content.source == U"**alp")));
    expect(fatal(bool(transaction->after.root.children[1].inline_content.source == U"ha**")));
    expect(fatal(bool(transaction->selection_after.active.container_id == transaction->after.root.children[1].id)));
    expect_document_valid(transaction->after);
};

};
