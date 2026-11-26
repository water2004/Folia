export module folia.core.table_calculation;
import std;
import folia.markdown.ast;
import folia.markdown.document;
import folia.core.document_transaction;
import folia.markdown.table_metadata;

export namespace folia {

class TableCalculationCache {
public:
    struct Aggregate {
        long double sum = 0;
        std::size_t numeric_count = 0;
        std::size_t content_count = 0;
        std::map<double, std::size_t> ordered;
    };
    struct Table {
        std::uint64_t revision = 0;
        std::vector<Aggregate> columns;
    };

    Table& table(std::uint64_t id) { return tables_[id]; }
    void clear() { tables_.clear(); }
private:
    std::unordered_map<std::uint64_t, Table> tables_;
};

bool table_formula_is_valid(const BlockNode& table, std::u32string_view formula, TableRegion selection);
void refresh_table_calculations(EditorDocument& document, DocumentTransaction& transaction,
    TableCalculationCache& cache);

}
