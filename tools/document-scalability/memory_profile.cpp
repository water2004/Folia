// Production-core ownership and working-set probe. No font measurement is
// substituted for DirectWrite. An explicitly supplied file stays inside this
// process; only aggregate sizes, timings and operation counts are printed.
#if defined(__linux__)
#include <malloc.h>
#endif
import std;
import folia.core.editor;
import folia.core.render_builder;
import folia.core.render_model;
import folia.core.source_render;
import folia.core.theme;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

namespace {
using namespace folia;
using Clock = std::chrono::steady_clock;

struct Memory {
    std::size_t rss_kib = 0;
    std::size_t peak_kib = 0;
    std::size_t allocated_bytes = 0;
};

Memory memory() {
    Memory value;
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        auto read = [&](std::string_view prefix, std::size_t& result) {
            if (line.starts_with(prefix)) {
                std::istringstream input(line.substr(prefix.size()));
                input >> result;
            }
        };
        read("VmRSS:", value.rss_kib);
        read("VmHWM:", value.peak_kib);
    }
#endif
#if defined(__GLIBC__)
    const auto allocator = mallinfo2();
    value.allocated_bytes = allocator.uordblks + allocator.hblkhd;
#endif
    return value;
}

void emit(std::string_view phase, Clock::time_point start,
          RenderModel const* model = nullptr) {
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    const auto bytes = memory();
    const auto work = read_core_operation_counters();
    std::cout << "{\"phase\":\"" << phase << "\",\"milliseconds\":" << elapsed
              << ",\"rss_kib\":" << bytes.rss_kib
              << ",\"peak_rss_kib\":" << bytes.peak_kib
              << ",\"allocated_bytes\":" << bytes.allocated_bytes
              << ",\"full_parses\":" << work.full_document_parses
              << ",\"full_serializations\":" << work.full_document_serializations
              << ",\"full_index_scans\":" << work.full_document_block_index_scans;
    if (model) {
        std::cout << ",\"render_units\":" << model->blocks.size()
                  << ",\"materialized_units\":" << model->materialized_block_indices.size()
                  << ",\"render_block_capacity_bytes\":" << model->blocks.capacity() * sizeof(RenderBlock);
    }
    std::cout << "}\n" << std::flush;
}

template<class Map>
std::size_t offset_storage_bytes(Map const& map) {
    if constexpr (requires { map.allocated_bytes(); }) return map.allocated_bytes();
    else return map.capacity() * sizeof(typename Map::value_type);
}

void census(EditorDocument const& document) {
    std::size_t blocks = 0, raw_blocks = 0, offsets = 0, offset_bytes = 0;
    std::size_t inline_nodes = 0, semantic_payloads = 0;
    walk_blocks(document.root, [&](BlockNode const& block) {
        ++blocks;
        if (!block.block_source.empty()) {
            ++raw_blocks;
            const auto& map = block.block_source.tree().content_to_source;
            offsets += map.size();
            offset_bytes += offset_storage_bytes(map);
        }
        std::vector<InlineCstNode const*> pending;
        for (auto const& node : block.inline_content.tree.nodes) pending.push_back(&node);
        while (!pending.empty()) {
            const auto* node = pending.back();
            pending.pop_back();
            ++inline_nodes;
            semantic_payloads += node->semantic != nullptr;
            for (auto const& child : node->children) pending.push_back(&child);
        }
    });
    std::cout << "{\"phase\":\"census\",\"block_count\":" << blocks
              << ",\"raw_block_count\":" << raw_blocks
              << ",\"raw_content_offsets\":" << offsets
              << ",\"raw_offset_storage_bytes\":" << offset_bytes
              << ",\"inline_nodes\":" << inline_nodes
              << ",\"inline_semantic_payloads\":" << semantic_payloads
              << ",\"inline_semantic_object_bytes\":" << semantic_payloads * sizeof(InlineCstSemantic)
              << ",\"sizeof_block_node\":" << sizeof(BlockNode)
              << ",\"sizeof_render_block\":" << sizeof(RenderBlock)
              << "}\n" << std::flush;
}

std::string synthetic(std::size_t mebibytes, bool unique) {
    constexpr std::string_view section = R"markdown(
行内公式 $f(x)=\frac{x+1}{x-1}$ 与 $\int_0^1 x^n\,dx=\frac{1}{n+1}$。

$$
\begin{aligned}
\int_{-\infty}^{\infty} e^{-x^2}\,dx &= \sqrt{\pi} \\
\sum_{k=1}^{n}k^3 &= \left(\frac{n(n+1)}{2}\right)^2
\end{aligned}
$$

| 指标 | 符号 | 表达式 |
|:---|:---:|---:|
| 均值 | $\bar{x}$ | $\frac{1}{n}\sum_{i=1}^{n}x_i$ |
| 方差 | $s^2$ | $\frac{1}{n-1}\sum_{i=1}^n(x_i-\bar x)^2$ |

- 一级列表 $a^2+b^2=c^2$
  - 二级列表 $e^{i\pi}+1=0$
    - 三级列表 $\nabla\cdot\vec F=\rho/\varepsilon_0$

```python
import math
values = [math.sin(x / 100) ** 2 for x in range(1000)]
```

> 引用内容 $E=mc^2$，包含 Unicode 和组合文字 é。

)markdown";
    std::string result;
    const auto target = mebibytes * 1024u * 1024u;
    result.reserve(target + section.size() + 128);
    for (std::size_t index = 1; result.size() < target; ++index) {
        result += "## Memory sample " + std::to_string(index) + "\n";
        result += section;
        if (unique) result += "$$\nx_{" + std::to_string(index) + "}=" + std::to_string(index) + "\\pi\n$$\n\n";
    }
    return result;
}

void project_change(Editor& editor, RenderModel& model, ThemeProfile const& theme) {
    auto change = editor.take_last_document_change();
    if (!change) throw std::runtime_error("missing edit notification");
    RenderModelUpdate update;
    update.structural = change->structural;
    update.structural_locality_known = change->structural_locality_known;
    update.structural_anchors = change->structural_anchors;
    update.tree_deltas = change->tree_deltas;
    for (auto const& operation : change->text_operations)
        update.changed_owners.push_back((change->forward ? operation.forward : operation.inverse).container_id);
    model = build_render_model_incremental(editor.document(), editor.outline(), editor.symbols(),
                                          theme, std::move(model), update);
}

int run(int argc, char** argv) {
    std::string input;
    bool supplied = false;
    if (argc == 3 && std::string_view(argv[1]) == "--input") {
        std::ifstream stream(argv[2], std::ios::binary | std::ios::ate);
        if (!stream) throw std::runtime_error("cannot open explicitly supplied input");
        const auto length = stream.tellg();
        if (length < 0) throw std::runtime_error("cannot size input");
        input.resize(static_cast<std::size_t>(length));
        stream.seekg(0);
        if (!stream.read(input.data(), static_cast<std::streamsize>(input.size())))
            throw std::runtime_error("cannot read input");
        supplied = true;
    } else {
        const auto size = argc > 1 ? std::stoull(argv[1]) : 10;
        const bool unique = argc > 2 && std::string_view(argv[2]) == "unique";
        input = synthetic(size, unique);
    }
    std::cout << "{\"phase\":\"metadata\",\"input_bytes\":" << input.size()
              << ",\"supplied_input\":" << (supplied ? "true" : "false")
              << ",\"directwrite_measured\":false,\"math_engine_measured\":false}\n";
    reset_core_operation_counters();
    auto start = Clock::now();
    auto editor = std::make_unique<Editor>(std::move(input));
    emit("parse", start);
    census(editor->document());
    const auto theme = default_theme_profile();
    reset_core_operation_counters();
    start = Clock::now();
    auto model = build_virtualized_render_model(editor->document(), editor->outline(), editor->symbols(), theme);
    emit("virtual_projection", start, &model);

    // Exercise the same bounded projection lifetime used during prelayout and
    // viewport movement. This does not measure native shaping or formula SVGs.
    reset_core_operation_counters();
    start = Clock::now();
    for (std::size_t begin = 0; begin < model.blocks.size(); begin += 16) {
        const auto end = (std::min)(begin + 16, model.blocks.size());
        release_render_model_blocks_outside(model, editor->document(), theme, begin, end);
        materialize_render_model_range(model, editor->document(), editor->symbols(), theme, begin, end);
        if (model.materialized_block_indices.size() > 16) throw std::runtime_error("unbounded visual projection");
    }
    release_render_model_blocks_outside(model, editor->document(), theme, 0, 0);
    emit("projection_sweep", start, &model);

    std::vector<NodeId> paragraphs;
    walk_blocks(editor->document().root, [&](BlockNode const& block) {
        if (block.kind == BlockKind::Paragraph && !block.inline_content.source.empty()) paragraphs.push_back(block.id);
    });
    if (!paragraphs.empty()) {
        for (const auto offset : {std::size_t{0}, paragraphs.size() / 2, paragraphs.size() - 1}) {
            const auto owner = paragraphs[offset];
            const std::u32string original(*editor->editable_source(owner));
            const auto fragment = model.editable_fragment_index.at(owner.v);
            release_render_model_blocks_outside(model, editor->document(), theme, fragment, fragment + 1);
            materialize_render_model_range(model, editor->document(), editor->symbols(), theme, fragment, fragment + 1);
            editor->set_selection(TextSelection::caret({owner, 0, TextAffinity::Downstream}));
            reset_core_operation_counters();
            start = Clock::now();
            if (!editor->execute_document_insert_text(editor->selection(), U"X")) throw std::runtime_error("insertion rejected");
            project_change(*editor, model, theme);
            if (!editor->undo()) throw std::runtime_error("undo rejected");
            project_change(*editor, model, theme);
            emit("edit_undo_projection", start, &model);
            if (*editor->editable_source(owner) != original) throw std::runtime_error("undo changed source");
        }
    }
    model = {};
    reset_core_operation_counters();
    start = Clock::now();
    if (!editor->enter_source_mode()) throw std::runtime_error("source mode rejected");
    emit("enter_source_mode", start);
    reset_core_operation_counters();
    start = Clock::now();
    model = build_source_render_model(*editor->source_editor());
    emit("source_projection", start, &model);
    model = {};
    reset_core_operation_counters();
    start = Clock::now();
    if (!editor->exit_source_mode()) throw std::runtime_error("rendered mode rejected");
    emit("exit_source_mode", start);
    start = Clock::now();
    editor.reset();
    emit("close", start);
    return 0;
}
}

int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (std::exception const& error) { std::cerr << error.what() << '\n'; return 1; }
}
