// Reproducible production-core benchmark. Generates its input; never reads an
// attachment, and never substitutes an estimated font measurer for DirectWrite.
import std;
import folia.core.command;
import folia.core.editor;
import folia.core.render_builder;
import folia.core.render_model;
import folia.core.theme;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

namespace {
using Clock = std::chrono::steady_clock;
using namespace folia;

struct Memory {
    std::uint64_t resident_kib = 0;
    std::uint64_t peak_kib = 0;
};

Memory process_memory() {
    Memory result;
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        const auto read = [&](std::string_view prefix, std::uint64_t& destination) {
            if (!line.starts_with(prefix)) return;
            std::istringstream value(line.substr(prefix.size()));
            value >> destination;
        };
        read("VmRSS:", result.resident_kib);
        read("VmHWM:", result.peak_kib);
    }
#endif
    return result;
}

double elapsed_ms(Clock::time_point begin) {
    return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
}

template<class Counters>
void emit_work(Counters const& work) {
    std::cout << ",\"full_parses\":" << work.full_document_parses
              << ",\"full_serializations\":" << work.full_document_serializations
              << ",\"inline_reparses\":" << work.inline_reparses
              << ",\"inline_characters\":" << work.inline_scanned_characters
              << ",\"full_index_scans\":" << work.full_document_block_index_scans
              << ",\"index_repairs\":" << work.incremental_document_block_index_repairs
              << ",\"render_source_keys\":" << work.render_source_key_derivations;
    // Older revisions do not have these work counters. Null means unavailable,
    // rather than incorrectly reporting zero work for the baseline.
    std::cout << ",\"tree_lookup_nodes\":";
    if constexpr (requires { work.block_tree_lookup_nodes; })
        std::cout << work.block_tree_lookup_nodes;
    else std::cout << "null";
    std::cout << ",\"index_entries_updated\":";
    if constexpr (requires { work.block_index_entries_updated; })
        std::cout << work.block_index_entries_updated;
    else std::cout << "null";
    std::cout << ",\"fragment_nodes_planned\":";
    if constexpr (requires { work.render_fragment_nodes_planned; })
        std::cout << work.render_fragment_nodes_planned;
    else std::cout << "null";
    std::cout << ",\"navigation_entries_updated\":";
    if constexpr (requires { work.render_navigation_entries_updated; })
        std::cout << work.render_navigation_entries_updated;
    else std::cout << "null";
}

void emit(std::string_view phase, std::string_view position, std::size_t round,
          double milliseconds, RenderModel const* model = nullptr) {
    const auto memory = process_memory();
    const auto work = read_core_operation_counters();
    std::cout << "{\"phase\":\"" << phase << "\",\"position\":\"" << position
              << "\",\"round\":" << round << ",\"milliseconds\":" << milliseconds
              << ",\"rss_kib\":" << memory.resident_kib
              << ",\"peak_rss_kib\":" << memory.peak_kib;
    if (model) {
        std::cout << ",\"render_units\":" << model->blocks.size()
                  << ",\"materialized_units\":" << model->materialized_block_indices.size()
                  << ",\"rebuilt_units\":" << model->rebuilt_block_count;
    }
    emit_work(work);
    std::cout << "}\n" << std::flush;
}

template<class Update, class Change>
void copy_tree_deltas(Update& update, Change const& change) {
    if constexpr (requires { update.tree_deltas = change.tree_deltas; })
        update.tree_deltas = change.tree_deltas;
}

void update_projection(Editor& editor, RenderModel& model, ThemeProfile const& theme) {
    const auto change = editor.take_last_document_change();
    if (!change) throw std::runtime_error("edit did not report its document change");
    RenderModelUpdate update;
    update.structural = change->structural;
    update.structural_locality_known = change->structural_locality_known;
    update.structural_anchors = change->structural_anchors;
    copy_tree_deltas(update, *change);
    for (auto const& operation : change->text_operations)
        update.changed_owners.push_back(
            (change->forward ? operation.forward : operation.inverse).container_id);
    model = build_render_model_incremental(editor.document(), editor.outline(),
        editor.symbols(), theme, std::move(model), update);
}

template<class Model>
std::size_t physical_index(Model const& model, NodeId owner) {
    if constexpr (requires { model.editable_fragment_index; })
        return model.editable_fragment_index.at(owner.v);
    // The generated workload is deliberately a single top-level list in the
    // baseline, whose only render block contains every editable owner.
    return 0;
}

int run(std::size_t mebibytes, std::size_t rounds) {
    constexpr std::string_view line = "- Sample markdown list item for parser and upload testing.\n";
    const auto target = mebibytes * 1024u * 1024u;
    std::string source;
    source.reserve(target + line.size());
    while (source.size() < target) source += line;
    const auto bytes = source.size();
    std::cout << "{\"phase\":\"metadata\",\"requested_mib\":" << mebibytes
              << ",\"source_bytes\":" << bytes << ",\"rounds\":" << rounds
              << ",\"directwrite_measured\":false,\"synthetic_input\":true}\n";

    reset_core_operation_counters();
    auto begin = Clock::now();
    Editor editor(std::move(source));
    emit("parse", "all", 0, elapsed_ms(begin));
    const auto& list = editor.document().root.children.front();
    if (list.kind != BlockKind::List || editor.document().root.children.size() != 1)
        throw std::runtime_error("generated input must remain one semantic list");
    const auto items = list.children.size();
    const std::array<NodeId, 3> owners{
        list.children.front().children.front().id,
        list.children[items / 2].children.front().id,
        list.children.back().children.front().id};
    constexpr std::array<std::string_view, 3> positions{"first", "middle", "last"};
    auto theme = default_theme_profile();

    reset_core_operation_counters();
    begin = Clock::now();
    auto model = build_virtualized_render_model(editor.document(), editor.outline(), editor.symbols(), theme);
    emit("virtual_projection", "all", 0, elapsed_ms(begin), &model);

    for (std::size_t location = 0; location < owners.size(); ++location) {
        const auto owner = owners[location];
        const auto position = positions[location];
        const auto index = physical_index(model, owner);
        const auto window_begin = index > 8 ? index - 8 : 0;
        const auto window_end = (std::min)(window_begin + 32, model.blocks.size());
        reset_core_operation_counters();
        begin = Clock::now();
        release_render_model_blocks_outside(model, editor.document(), theme, window_begin, window_end);
        materialize_render_model_range(model, editor.document(), editor.symbols(), theme, window_begin, window_end);
        emit("viewport_projection", position, 0, elapsed_ms(begin), &model);

        const auto original = editor.editable_source(owner);
        if (!original || original->size() < 8) throw std::runtime_error("missing source owner");
        for (std::size_t round = 0; round < rounds; ++round) {
            reset_core_operation_counters();
            begin = Clock::now();
            editor.set_selection(TextSelection::caret({owner, 8, TextAffinity::Downstream}));
            emit("select", position, round, elapsed_ms(begin));

            reset_core_operation_counters();
            begin = Clock::now();
            if (!editor.execute_document_insert_text(editor.selection(), U"X"))
                throw std::runtime_error("insertion rejected");
            emit("insert", position, round, elapsed_ms(begin));

            reset_core_operation_counters();
            begin = Clock::now();
            update_projection(editor, model, theme);
            emit("insert_projection", position, round, elapsed_ms(begin), &model);

            reset_core_operation_counters();
            begin = Clock::now();
            if (!editor.undo()) throw std::runtime_error("undo rejected");
            emit("undo", position, round, elapsed_ms(begin));

            reset_core_operation_counters();
            begin = Clock::now();
            update_projection(editor, model, theme);
            emit("undo_projection", position, round, elapsed_ms(begin), &model);
            if (editor.editable_source(owner) != original)
                throw std::runtime_error("undo did not restore exact owner source");
        }

        // Structural edits are reported separately: changing the sequence of
        // list items has a different dependency/geometry cost from typing.
        editor.set_selection(TextSelection::caret({owner, 8, TextAffinity::Downstream}));
        reset_core_operation_counters();
        begin = Clock::now();
        if (!editor.execute_command(Command{.kind = CommandKind::InsertNewline}))
            throw std::runtime_error("list item split rejected");
        emit("enter", position, 0, elapsed_ms(begin));

        reset_core_operation_counters();
        begin = Clock::now();
        update_projection(editor, model, theme);
        emit("enter_projection", position, 0, elapsed_ms(begin), &model);

        reset_core_operation_counters();
        begin = Clock::now();
        if (!editor.undo()) throw std::runtime_error("split undo rejected");
        emit("enter_undo", position, 0, elapsed_ms(begin));

        reset_core_operation_counters();
        begin = Clock::now();
        update_projection(editor, model, theme);
        emit("enter_undo_projection", position, 0, elapsed_ms(begin), &model);
        if (editor.editable_source(owner) != original)
            throw std::runtime_error("split undo did not restore exact owner source");
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto mebibytes = argc > 1 ? std::stoull(argv[1]) : 10u;
        const auto rounds = argc > 2 ? std::stoull(argv[2]) : 3u;
        if (mebibytes == 0 || mebibytes > 1024 || rounds == 0 || rounds > 100)
            throw std::invalid_argument("usage: FoliaCoreScalability [1..1024 MiB] [1..100 rounds]");
        return run(mebibytes, rounds);
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
