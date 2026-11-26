module folia.core.table_calculation;
import std;
import folia.core.table_values;
import folia.core.table_selection;
import folia.core.document_edit_primitives;
import folia.core.document_block_edit;
import folia.markdown.inline_document;
import folia.markdown.text_edit;
import folia.markdown.ids;
import folia.markdown.block_tree;

namespace folia {
namespace {

enum class Op { Value, Reference, Add, Subtract, Multiply, Divide, Negate, Sum, Average, Min, Max, Round };
struct Instruction {
    Op op;
    double literal = 0;
    std::size_t argument = 0;
};
using Formula = std::vector<Instruction>;
using Headers = std::map<std::u32string_view, std::size_t>;

Headers header_names(const BlockNode& table) {
    Headers headers;
    if (table.children.empty()) return headers;
    const auto& cells = table.children.front().children;
    for (std::size_t column = 0; column < cells.size(); ++column)
        headers.try_emplace(trim_table_value(cells[column].inline_content().source), column);
    return headers;
}

class FormulaParser {
public:
    FormulaParser(std::u32string_view text, const Headers& headers)
        : text_(trim_table_value(text)), headers_(headers) {}
    std::optional<Formula> compile() {
        take(U'=');
        if (text_.size() > 8192 || !expression(0) || !trim_table_value(text_).empty()) return std::nullopt;
        return std::move(code_);
    }
private:
    bool take(char32_t symbol) {
        text_ = trim_table_value(text_);
        if (text_.empty() || text_.front() != symbol) return false;
        text_.remove_prefix(1);
        return true;
    }
    bool expression(unsigned depth) {
        if (!term(depth)) return false;
        for (;;) {
            auto op = take(U'+') ? std::optional{Op::Add} : (take(U'-') || take(U'−'))
                ? std::optional{Op::Subtract} : std::nullopt;
            if (!op) return true;
            if (!term(depth)) return false;
            code_.push_back({*op});
        }
    }
    bool term(unsigned depth) {
        if (!atom(depth)) return false;
        for (;;) {
            auto op = (take(U'*') || take(U'×')) ? std::optional{Op::Multiply}
                : (take(U'/') || take(U'÷')) ? std::optional{Op::Divide} : std::nullopt;
            if (!op) return true;
            if (!atom(depth)) return false;
            code_.push_back({*op});
        }
    }
    bool atom(unsigned depth) {
        if (depth > 128 || text_.empty()) return false;
        if (take(U'+')) return atom(depth + 1);
        if (take(U'-') || take(U'−')) {
            if (!atom(depth + 1)) return false;
            code_.push_back({Op::Negate});
            return true;
        }
        if (take(U'(')) return expression(depth + 1) && take(U')');
        if (take(U'{')) {
            const auto closing = text_.find(U'}');
            if (closing == text_.npos) return false;
            const auto found = headers_.find(trim_table_value(text_.substr(0, closing)));
            if (found == headers_.end()) return false;
            code_.push_back({Op::Reference, 0, found->second});
            text_.remove_prefix(closing + 1);
            return true;
        }
        auto number_size = text_.find_first_not_of(U"0123456789.");
        if (number_size == text_.npos) number_size = text_.size();
        if (number_size) {
            if (auto number = table_number(text_.substr(0, number_size))) {
                code_.push_back({Op::Value, *number});
                text_.remove_prefix(number_size);
                return true;
            }
            return false;
        }
        std::u32string function;
        while (!text_.empty()) {
            auto name = text_.front();
            if (name >= U'a' && name <= U'z') name -= U'a' - U'A';
            if (name < U'A' || name > U'Z') break;
            function += name;
            text_.remove_prefix(1);
        }
        constexpr std::array<std::u32string_view, 5> names{U"SUM", U"AVG", U"MIN", U"MAX", U"ROUND"};
        const auto found = std::ranges::find(names, function);
        if (found == names.end() || !take(U'(')) return false;
        const auto op = static_cast<Op>(static_cast<int>(Op::Sum) + (found - names.begin()));
        std::size_t count = 0;
        do {
            if (!expression(depth + 1)) return false;
            ++count;
        } while (take(U','));
        if (!take(U')') || (op == Op::Round && count > 2)) return false;
        code_.push_back({op, 0, count});
        return true;
    }
    std::u32string_view text_;
    const Headers& headers_;
    Formula code_;
};

std::optional<double> evaluate(const Formula& formula, const std::function<std::optional<double>(std::size_t)>& column) {
    std::vector<double> stack;
    for (auto instruction : formula) {
        if (instruction.op == Op::Value) { stack.push_back(instruction.literal); continue; }
        if (instruction.op == Op::Reference) {
            const auto value = column(instruction.argument);
            if (!value) return std::nullopt;
            stack.push_back(*value);
            continue;
        }
        if (instruction.op == Op::Negate) { stack.back() = -stack.back(); continue; }
        const auto function = instruction.op >= Op::Sum;
        const auto count = function ? instruction.argument : 2;
        const auto begin = stack.size() - count;
        auto operands = std::span<const double>{stack}.subspan(begin);
        double result = 0;
        switch (instruction.op) {
            case Op::Add: result = operands[0] + operands[1]; break;
            case Op::Subtract: result = operands[0] - operands[1]; break;
            case Op::Multiply: result = operands[0] * operands[1]; break;
            case Op::Divide:
                if (operands[1] == 0) return std::nullopt;
                result = operands[0] / operands[1];
                break;
            case Op::Sum: result = std::accumulate(operands.begin(), operands.end(), 0.0); break;
            case Op::Average: result = std::accumulate(operands.begin(), operands.end(), 0.0) / count; break;
            case Op::Min: result = *std::ranges::min_element(operands); break;
            case Op::Max: result = *std::ranges::max_element(operands); break;
            case Op::Round: {
                const auto precision = count == 2 ? (std::clamp)(std::trunc(operands[1]), -12.0, 12.0) : 0.0;
                const auto scale = std::pow(10.0, precision);
                result = std::round(operands[0] * scale) / scale;
                break;
            }
            default: return std::nullopt;
        }
        if (!std::isfinite(result)) return std::nullopt;
        stack.resize(begin);
        stack.push_back(result);
    }
    return stack.size() == 1 ? std::optional{stack.back()} : std::nullopt;
}

bool calculated(const TableMetadata* metadata) {
    return metadata && (table_has_totals(metadata)
        || std::ranges::any_of(metadata->formulas, [](auto const& text) { return !text.empty(); }));
}

void write_cell(EditorDocument& document, DocumentTransaction& transaction,
    BlockNode& cell, std::u32string source) {
    auto const& before = cell.inline_content().source;
    if (before == source) return;
    document_edit_detail::NodeAllocator allocator(document);
    auto edit = document_edit_detail::edit_block_source(document, cell, {0, before.size()}, std::move(source), allocator);
    if (edit) document_edit_detail::append_source_operation(transaction.operations, std::move(*edit));
}

void revise_count(std::size_t& count, bool add) {
    if (add) ++count;
    else if (count > 0) --count;
}

void accumulate_cell(TableCalculationCache::Aggregate& aggregate, TableTotal type,
    std::u32string_view text, bool add) {
    if (!trim_table_value(text).empty()) revise_count(aggregate.content_count, add);
    auto value = table_number(text, true);
    if (!value) return;
    revise_count(aggregate.numeric_count, add);
    aggregate.sum += add ? *value : -*value;
    if (type == TableTotal::Minimum || type == TableTotal::Maximum) {
        if (add) ++aggregate.ordered[*value];
        else if (auto found = aggregate.ordered.find(*value); found != aggregate.ordered.end()) {
            if (--found->second == 0) aggregate.ordered.erase(found);
        }
    }
}

std::optional<double> aggregate_value(const TableCalculationCache::Aggregate& aggregate, TableTotal type) {
    switch (type) {
        case TableTotal::Sum: return static_cast<double>(aggregate.sum);
        case TableTotal::Average: return aggregate.numeric_count
            ? std::optional{static_cast<double>(aggregate.sum / aggregate.numeric_count)} : std::nullopt;
        case TableTotal::Count: return static_cast<double>(aggregate.content_count);
        case TableTotal::Minimum: return aggregate.ordered.empty() ? std::nullopt : std::optional{aggregate.ordered.begin()->first};
        case TableTotal::Maximum: return aggregate.ordered.empty() ? std::nullopt : std::optional{aggregate.ordered.rbegin()->first};
        default: return std::nullopt;
    }
}

struct Work {
    bool structural = false;
    std::set<std::size_t> rows;
    std::map<std::size_t, std::vector<std::size_t>> header_edits;
};

void update_table(EditorDocument& document, DocumentTransaction& transaction, BlockNode& table,
    const Work& work, TableCalculationCache& cache) {
    auto metadata = table.table_special().metadata;
    if (!calculated(metadata.get()) || table.children.empty()) return;
    if (!work.header_edits.empty() && !metadata->formulas.empty()) {
        std::map<std::u32string, std::u32string, std::less<>> renamed;
        for (const auto& [column, edits] : work.header_edits) {
            const auto& source = table.children.front().children[column].inline_content().source;
            auto before = source;
            for (auto index : edits | std::views::reverse) {
                const auto& inverse = std::get<DocumentTextOperation>(transaction.operations[index]).inverse;
                if (inverse.range.valid_for(before.size()))
                    before.replace(inverse.range.start, inverse.range.length(), inverse.replacement);
            }
            const auto old_name = trim_table_value(before);
            const auto new_name = trim_table_value(source);
            if (old_name != new_name && !new_name.contains(U'}'))
                renamed.try_emplace(std::u32string(old_name), new_name);
        }
        auto next = rename_table_formula_references(metadata, renamed);
        if (next != metadata) {
            DocumentTreeEdit edit;
            edit.kind = DocumentTreeEditKind::UpdatePayload;
            edit.before = document_transaction_detail::payload_shell(table);
            table.ensure_table_special().metadata = next;
            edit.after = document_transaction_detail::payload_shell(table);
            transaction.operations.emplace_back(std::move(edit));
            metadata = std::move(next);
        }
    }
    const auto headers = (std::min)(table.children.size(), metadata->header_rows);
    const auto columns = table.children.front().children.size();
    const auto has_total = table_has_totals(metadata.get());
    const auto data_end = table.children.size() - has_total;
    const auto all_rows = work.structural || std::ranges::any_of(work.rows, [&](auto row) { return row < headers; });
    auto names = header_names(table);
    std::vector<std::optional<Formula>> formulas;
    formulas.reserve((std::min)(metadata->formulas.size(), columns));
    for (auto const& text : metadata->formulas | std::views::take(columns))
        formulas.push_back(text.empty() ? std::nullopt : FormulaParser(text, names).compile());

    auto& totals = cache.table(table.id.v);
    const bool incremental = has_total && !all_rows && totals.revision == transaction.revision_before
        && totals.columns.size() == metadata->totals.size();
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> changed_cells;
    if (incremental) {
        for (std::size_t index = 0; index < transaction.operations.size(); ++index)
            if (const auto* edit = std::get_if<DocumentTextOperation>(&transaction.operations[index]))
                changed_cells[edit->inverse.container_id.v].push_back(index);
    }
    auto previous_cell = [&](const BlockNode& cell) {
        auto source = cell.inline_content().source;
        if (auto edits = changed_cells.find(cell.id.v); edits != changed_cells.end())
            for (auto index : edits->second | std::views::reverse) {
                auto const& inverse = std::get<DocumentTextOperation>(transaction.operations[index]).inverse;
                if (inverse.range.valid_for(source.size())) source.replace(inverse.range.start, inverse.range.length(), inverse.replacement);
            }
        return source;
    };
    auto update_row = [&](std::size_t row) {
        auto& cells = table.children[row].children;
        std::vector<std::u32string> before;
        if (incremental) {
            before.resize((std::min)(metadata->totals.size(), columns));
            for (std::size_t column = 0; column < before.size(); ++column)
                if (metadata->totals[column] != TableTotal::None) before[column] = previous_cell(cells[column]);
        }
        std::vector<std::optional<double>> values(columns);
        enum class Mark { Unknown, Evaluating, Done };
        std::vector<Mark> marks(columns, Mark::Unknown);
        auto scalar = [&](auto& self, std::size_t column, unsigned depth) -> std::optional<double> {
            if (depth > 128 || marks[column] == Mark::Evaluating) return std::nullopt;
            if (marks[column] == Mark::Done) return values[column];
            marks[column] = Mark::Evaluating;
            if (column < formulas.size() && !metadata->formulas[column].empty()) {
                if (formulas[column]) values[column] = evaluate(*formulas[column], [&](auto dependency) {
                    return self(self, dependency, depth + 1);
                });
            } else values[column] = table_number(cells[column].inline_content().source, true);
            marks[column] = Mark::Done;
            return values[column];
        };
        for (std::size_t column = 0; column < formulas.size(); ++column) {
            if (metadata->formulas[column].empty()) continue;
            auto value = scalar(scalar, column, 0);
            auto precision = column < metadata->formula_decimals.size()
                ? metadata->formula_decimals[column] : std::nullopt;
            write_cell(document, transaction, cells[column], value
                ? table_decimal(*value, precision.value_or(8), precision.has_value()) : std::u32string{});
        }
        for (std::size_t column = 0; column < before.size(); ++column) {
            const auto type = metadata->totals[column];
            if (type == TableTotal::None) continue;
            accumulate_cell(totals.columns[column], type, before[column], false);
            accumulate_cell(totals.columns[column], type, cells[column].inline_content().source, true);
        }
    };
    if (all_rows) {
        if (!formulas.empty()) for (auto row = headers; row < data_end; ++row) update_row(row);
    } else for (auto row : work.rows) if (row >= headers && row < data_end) update_row(row);
    if (!has_total || data_end < headers) return;
    if (!incremental) {
        totals.columns.assign(metadata->totals.size(), {});
        for (auto row = headers; row < data_end; ++row)
            for (std::size_t column = 0; column < columns && column < metadata->totals.size(); ++column) {
                const auto kind = metadata->totals[column];
                if (kind != TableTotal::None)
                    accumulate_cell(totals.columns[column], kind, table.children[row].children[column].inline_content().source, true);
            }
    }
    totals.revision = transaction.revision_after;
    for (std::size_t column = 0; column < columns; ++column) {
        const auto kind = column < metadata->totals.size() ? metadata->totals[column] : TableTotal::None;
        auto value = kind == TableTotal::None ? std::nullopt : aggregate_value(totals.columns[column], kind);
        auto source = value ? table_decimal(*value, 8)
            : column == 0 && kind == TableTotal::None ? std::u32string{U"Total"} : std::u32string{};
        write_cell(document, transaction, table.children.back().children[column], std::move(source));
    }
}

} // namespace

bool table_formula_is_valid(const BlockNode& table, std::u32string_view formula, TableRegion selection) {
    if (trim_table_value(formula).empty()) return true;
    const auto headers = header_names(table);
    const auto compiled = FormulaParser(formula, headers).compile();
    if (!compiled || table.children.empty()) return false;
    const auto columns = table.children.front().children.size();
    const auto* metadata = table.table_special().metadata.get();
    std::vector<std::optional<Formula>> formulas(columns);
    for (std::size_t column = 0; column < columns; ++column) {
        if (selection.first.column <= column && column <= selection.last.column) formulas[column] = compiled;
        else if (metadata && column < metadata->formulas.size() && !metadata->formulas[column].empty())
            formulas[column] = FormulaParser(metadata->formulas[column], headers).compile();
    }
    for (auto column = selection.first.column; column <= selection.last.column && column < columns; ++column) {
        std::vector<std::size_t> pending{column};
        std::vector<bool> visited(columns, false);
        while (!pending.empty()) {
            const auto at = pending.back();
            pending.pop_back();
            if (visited[at] || !formulas[at]) continue;
            visited[at] = true;
            for (const auto& instruction : *formulas[at]) {
                if (instruction.op != Op::Reference) continue;
                if (instruction.argument == column) return false;
                pending.push_back(instruction.argument);
            }
        }
    }
    return true;
}

void refresh_table_calculations(EditorDocument& document, DocumentTransaction& transaction, TableCalculationCache& cache) {
    std::unordered_map<std::uint64_t, Work> changes;
    // The new structure has not reached the derived indexes yet. Consult
    // existing owner paths only. Inserts under a table are reported by their
    // parent operation, so there is no reason to search the document.
    auto known_block = [&](NodeId id) -> const BlockNode* {
        auto path = cached_document_block_path(document, id);
        const auto* block = path ? block_at_path(document.root, *path) : nullptr;
        return block && block->id == id ? block : nullptr;
    };
    for (std::size_t index = 0; index < transaction.operations.size(); ++index) {
        auto const& operation = transaction.operations[index];
        if (auto text = std::get_if<DocumentTextOperation>(&operation)) {
            const auto* edited = known_block(text->forward.container_id);
            if (!edited || edited->kind != BlockKind::TableCell) continue;
            if (auto cell = document_table_cell_address(document, edited->id)) {
                auto const* table = find_document_block(document, cell->table_id);
                if (table && calculated(table->table_special().metadata.get())) {
                    auto& work = changes[cell->table_id.v];
                    work.rows.insert(cell->row);
                    if (cell->row == 0) work.header_edits[cell->column].push_back(index);
                }
            }
        } else if (auto tree = std::get_if<DocumentTreeEdit>(&operation)) {
            auto owner = tree->kind == DocumentTreeEditKind::UpdatePayload ? tree->after.id : tree->parent_id;
            if (tree->kind == DocumentTreeEditKind::UpdatePayload && tree->after.kind != BlockKind::Table) continue;
            const auto* block = known_block(owner);
            if (!block) continue;
            if (block->kind == BlockKind::TableRow && !block->children.empty()) {
                auto parent = document.cached_block_locators.find(block->id.v);
                block = parent != document.cached_block_locators.end() ? known_block(parent->second.parent_id) : nullptr;
            }
            if (!block || block->kind != BlockKind::Table || !calculated(block->table_special().metadata.get())) continue;
            if (tree->kind == DocumentTreeEditKind::UpdatePayload) {
                auto const* before = tree->before.table_special().metadata.get();
                auto const* after = tree->after.table_special().metadata.get();
                if (before && after && before->formulas == after->formulas && before->totals == after->totals
                    && before->formula_decimals == after->formula_decimals) continue;
            }
            changes[block->id.v].structural = true;
        }
    }
    for (const auto& [id, work] : changes)
        if (auto* table = find_document_block(document, NodeId{id}))
            update_table(document, transaction, *table, work, cache);
}

}
