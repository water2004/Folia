// folia.markdown.instrumentation — testable counters for architectural hot-path gates.
export module folia.markdown.instrumentation;
import std;

export namespace folia {

struct CoreOperationCounters {
    std::uint64_t full_document_parses = 0;
    std::uint64_t block_fragment_parses = 0;
    std::uint64_t full_document_serializations = 0;
    std::uint64_t full_tree_transaction_diffs = 0;
    std::uint64_t full_document_symbol_derivations = 0;
    std::uint64_t full_document_outline_derivations = 0;
    std::uint64_t local_symbol_derivations = 0;
    std::uint64_t render_source_key_derivations = 0;
    std::uint64_t render_source_nodes_hashed = 0;
    std::uint64_t render_source_characters_hashed = 0;
    std::uint64_t render_fragment_nodes_planned = 0;
    std::uint64_t render_navigation_entries_updated = 0;
    std::uint64_t full_document_block_index_scans = 0;
    std::uint64_t incremental_document_block_index_repairs = 0;
    std::uint64_t full_document_text_projections = 0;
    std::uint64_t inline_reparses = 0;
    std::uint64_t inline_scanned_characters = 0;
    // Count work, not just calls to high-level rebuild helpers. Direct
    // block_path() searches used to escape the full-index-scan gate entirely.
    std::uint64_t block_tree_lookup_nodes = 0;
    std::uint64_t block_index_entries_updated = 0;
    std::uint64_t editable_order_entries_updated = 0;
    std::uint64_t container_prefix_records_examined = 0;
    std::uint64_t container_prefix_records_copied = 0;
};

inline thread_local CoreOperationCounters core_operation_counters;

inline void reset_core_operation_counters() { core_operation_counters = {}; }
inline CoreOperationCounters read_core_operation_counters() { return core_operation_counters; }
inline void record_full_document_parse() { ++core_operation_counters.full_document_parses; }
inline void record_block_fragment_parse() { ++core_operation_counters.block_fragment_parses; }
inline void record_full_document_serialization() { ++core_operation_counters.full_document_serializations; }
inline void record_full_tree_transaction_diff() { ++core_operation_counters.full_tree_transaction_diffs; }
inline void record_full_document_symbol_derivation() {
    ++core_operation_counters.full_document_symbol_derivations;
}
inline void record_full_document_outline_derivation() {
    ++core_operation_counters.full_document_outline_derivations;
}
inline void record_local_symbol_derivation() { ++core_operation_counters.local_symbol_derivations; }
inline void record_render_source_key_derivation() {
    ++core_operation_counters.render_source_key_derivations;
}
inline void record_render_source_node(std::size_t characters) {
    ++core_operation_counters.render_source_nodes_hashed;
    core_operation_counters.render_source_characters_hashed += characters;
}
inline void record_render_fragment_plan_node() {
    ++core_operation_counters.render_fragment_nodes_planned;
}
inline void record_render_navigation_entry_update() {
    ++core_operation_counters.render_navigation_entries_updated;
}
inline void record_full_document_block_index_scan() {
    ++core_operation_counters.full_document_block_index_scans;
}
inline void record_incremental_document_block_index_repair() {
    ++core_operation_counters.incremental_document_block_index_repairs;
}
inline void record_full_document_text_projection() {
    ++core_operation_counters.full_document_text_projections;
}
inline void record_inline_reparse() { ++core_operation_counters.inline_reparses; }
inline void record_inline_scanned_characters(std::size_t count) {
    core_operation_counters.inline_scanned_characters += count;
}

inline void record_block_tree_lookup_node() { ++core_operation_counters.block_tree_lookup_nodes; }
inline void record_block_index_entry_update() { ++core_operation_counters.block_index_entries_updated; }
inline void record_editable_order_entry_update() { ++core_operation_counters.editable_order_entries_updated; }
inline void record_container_prefix_probe() { ++core_operation_counters.container_prefix_records_examined; }
inline void record_container_prefix_copy(std::size_t count) {
    core_operation_counters.container_prefix_records_copied += count;
}

} // namespace folia
