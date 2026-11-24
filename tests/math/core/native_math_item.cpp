#include "support/folia_test.hpp"
import folia.math.core.math_item;

namespace {
using namespace boost::ut;
namespace core = folia::math::core;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;

scene::Scene makeScene(double width = 3) {
    scene::Group group;
    group.metadata.source_tag = "native-math-item-output";
    return scene::Scene(std::move(group), scene::Metrics{width, .75, .25},
                        scene::FontMetrics{16, 8, .5, .75});
}

struct RecordingInput final : core::MathInputJaxService {
    int calls{};
    mml::MmlNode* selected{};
    mml::MmlDocument* lastDocument{};
    core::MathOutputJaxService* lastOutput{};
    bool fail{};
    std::optional<std::int64_t> captureCounter;
    std::vector<core::MathState> states;
    std::vector<std::optional<core::MathRecompile>> recompiles;

    mml::MmlNode& compile(core::MathItem& item, mml::MmlDocument& document,
                          core::CompileServices services = {}) override {
        ++calls;
        lastDocument = &document;
        lastOutput = services.output;
        states.push_back(item.state());
        recompiles.push_back(item.inputData.recompile);
        if (captureCounter) item.inputData.recompile = core::MathRecompile{item.state(), *captureCounter};
        if (fail) throw std::runtime_error("recorded input failure");
        return selected ? *selected : document.make(mml::NodeKind::math);
    }
};

struct RecordingOutput final : core::MathOutputJaxService {
    int typesets{}, escapes{}, measures{};
    bool fail{};
    core::MathItem* lastItem{};
    mml::MmlDocument* lastDocument{};
    core::MathState seenState{core::STATE.UNPROCESSED};

    scene::Scene typeset(core::MathItem& item, mml::MmlDocument& document) override {
        ++typesets;
        record(item, document);
        if (fail) throw std::runtime_error("recorded output failure");
        return makeScene();
    }
    scene::Scene escaped(core::MathItem& item, mml::MmlDocument& document) override {
        ++escapes;
        record(item, document);
        if (fail) throw std::runtime_error("recorded escaped failure");
        return makeScene(7);
    }
    folia::math::BBox getBBox(core::MathItem& item, mml::MmlDocument& document) override {
        ++measures;
        record(item, document);
        return folia::math::BBox(folia::math::BBoxData{.w = 1.25, .h = .75, .d = .25});
    }
    void record(core::MathItem& item, mml::MmlDocument& document) {
        lastItem = &item;
        lastDocument = &document;
        seenState = item.state();
    }
};

struct ActionCall {
    bool convert;
    core::MathState boundary;
    core::MathState seenState;
    bool hadInput;
    bool hadOutput;
    core::MathItem* item;
    mml::MmlDocument* document;
};
struct RecordingActions final : core::MathRenderActionsService {
    std::vector<ActionCall> calls;
    void renderMath(core::MathItem& item, mml::MmlDocument& document, core::MathState start) override {
        record(false, item, document, start);
    }
    void renderConvert(core::MathItem& item, mml::MmlDocument& document, core::MathState end) override {
        record(true, item, document, end);
    }
    void record(bool convert, core::MathItem& item, mml::MmlDocument& document, core::MathState boundary) {
        calls.push_back({convert, boundary, item.state(), item.inputData.recompile.has_value(),
                         item.outputData.mtext_family.has_value(), &item, &document});
    }
};

struct RemovalCall {
    bool restore;
    core::MathState state;
    bool hadInput;
    bool hadOutput;
};
struct ObservedItem final : core::AbstractMathItem {
    using AbstractMathItem::AbstractMathItem;
    std::vector<std::string> events;
    std::vector<RemovalCall> removals;
    std::optional<core::MathState> stateAfterClear;
    void removeFromDocument(bool restore) override {
        events.push_back("remove");
        removals.push_back({restore, state(), inputData.recompile.has_value(), outputData.mtext_family.has_value()});
        AbstractMathItem::removeFromDocument(restore);
    }
    void clear() override {
        events.push_back("clear");
        if (stateAfterClear) state_ = *stateAfterClear;
    }
};

suite native_math_item = [] {
    "math_item_initial_values_and_single_display_state"_test = [] {
        core::AbstractMathItem item(u"x+😀", nullptr);
        expect(item.math == u"x+😀");
        expect(item.inputJax == nullptr);
        expect(item.isDisplay());
        expect(!item.isEscaped());
        expect(item.state() == core::STATE.UNPROCESSED);
        expect(item.root == nullptr);
        expect(!item.typesetRoot.has_value());
        expect(!item.metrics.has_value());
        expect(!item.inputData.recompile.has_value());
        expect(!item.outputData.mtext_family.has_value());

        item.setDisplay(false);
        expect(item.display == core::MathDisplay::inline_math);
        expect(!item.isDisplay());
        expect(!item.isEscaped());
        item.display = core::MathDisplay::escaped;
        expect(item.isEscaped());
        expect(!item.isDisplay());
        item.setDisplay(true);
        expect(item.isDisplay());
        expect(!item.isEscaped());
        item.setMetrics(19, 7.5, 412, 1.25);
        expect(item.metrics.has_value());
        expect(item.metrics->em == 19.0_d);
        expect(item.metrics->ex == 7.5_d);
        expect(item.metrics->containerWidth == 412.0_d);
        expect(item.metrics->scale == 1.25_d);
    };

    "math_item_compile_gate_borrows_real_node_and_rejects_foreign_document"_test = [] {
        mml::MmlDocument document;
        mml::MmlDocument other;
        RecordingInput input;
        core::AbstractMathItem item(u"x", &input, core::MathDisplay::inline_math);
        item.compile(document);
        auto* first = item.root;
        expect(first != nullptr);
        expect(&first->document() == &document);
        expect(input.lastDocument == &document);
        expect(input.states == std::vector<core::MathState>{core::STATE.UNPROCESSED});
        expect(item.state() == core::STATE.COMPILED);
        item.compile(document);
        expect(input.calls == 1_i);
        expect(item.root == first);
        expect(document.size() == 1_u);

        item.state(static_cast<core::MathState>(19));
        input.selected = &other.make(mml::NodeKind::math);
        bool rejected = false;
        try { item.compile(document); }
        catch (std::invalid_argument const&) { rejected = true; }
        expect(rejected);
        expect(item.root == first);
        expect(item.state() == static_cast<core::MathState>(19));
        expect(document.size() == 1_u);
        expect(other.size() == 1_u);

        input.selected = nullptr;
        input.fail = true;
        bool failed = false;
        try { item.compile(document); }
        catch (std::runtime_error const&) { failed = true; }
        expect(failed);
        expect(item.root == first);
        expect(item.state() == static_cast<core::MathState>(19));
    };

    "math_item_compile_borrows_output_for_only_the_current_compilation"_test = [] {
        mml::MmlDocument document;
        RecordingInput input;
        RecordingOutput first, second;
        core::AbstractMathItem item(u"x", &input);
        core::MathItem& expression = item;
        expression.compile(document, {&first});
        expect(input.lastOutput == &first);
        expect(input.calls == 1_i);
        expression.compile(document, {&second});
        expect(input.calls == 1_i) << "Completed compile is still state-gated";
        expect(input.lastOutput == &first);

        expression.state(static_cast<core::MathState>(19));
        input.fail = true;
        expect(throws<std::runtime_error>([&] { expression.compile(document, {&second}); }));
        expect(input.lastOutput == &second);
        expect(expression.state() == static_cast<core::MathState>(19));
        input.fail = false;
        expression.compile(document);
        expect(input.lastOutput == nullptr) << "A new call never inherits the previous borrow";
        expect(expression.state() == core::STATE.COMPILED);
        expect(first.measures == 0_i);
        expect(second.measures == 0_i);
    };

    "math_item_null_input_is_measurement_only_without_dummy_compile"_test = [] {
        mml::MmlDocument document;
        auto& root = document.make(mml::NodeKind::math);
        auto& child = document.make(mml::NodeKind::mi);
        document.append(root, child);
        document.setRoot(root);
        core::AbstractMathItem measured(u"", nullptr, core::MathDisplay::inline_math);
        measured.root = &child;
        RecordingOutput output;
        auto box = output.getBBox(measured, document);
        expect(box.w == 1.25_d);
        expect(output.lastItem == &measured);
        expect(output.lastDocument == &document);
        expect(document.root() == &root);
        expect(child.parent() == &root);
        expect(!measured.typesetRoot.has_value());

        bool rejected = false;
        try { measured.compile(document); }
        catch (std::logic_error const&) { rejected = true; }
        expect(rejected);
        expect(measured.root == &child);
        expect(measured.state() == core::STATE.UNPROCESSED);
        expect(document.size() == 2_u);
    };

    "math_item_typeset_and_escaped_gates_commit_only_completed_scene"_test = [] {
        mml::MmlDocument document;
        RecordingInput input;
        RecordingOutput output;
        core::AbstractMathItem item(u"x", &input);
        item.compile(document);
        item.typeset(output, document);
        expect(output.typesets == 1_i);
        expect(output.escapes == 0_i);
        expect(output.seenState == core::STATE.COMPILED);
        expect(item.typesetRoot.has_value());
        expect(item.typesetRoot->metrics().width == 3.0_d);
        expect(item.state() == core::STATE.TYPESET);
        item.typeset(output, document);
        expect(output.typesets == 1_i);

        item.state(static_cast<core::MathState>(124));
        output.fail = true;
        bool failed = false;
        try { item.typeset(output, document); }
        catch (std::runtime_error const&) { failed = true; }
        expect(failed);
        expect(item.state() == static_cast<core::MathState>(124));
        expect(item.typesetRoot->metrics().width == 3.0_d);

        output.fail = false;
        core::AbstractMathItem escaped(u"$", nullptr, core::MathDisplay::escaped);
        escaped.typeset(output, document);
        expect(output.escapes == 1_i);
        expect(escaped.typesetRoot->metrics().width == 7.0_d);
        expect(escaped.state() == core::STATE.TYPESET);
        escaped.typeset(output, document);
        expect(output.escapes == 1_i);
    };

    "math_item_rollback_removal_precedes_data_clears_and_keeps_roots_metrics"_test = [] {
        mml::MmlDocument document;
        ObservedItem item(u"x", nullptr);
        item.root = &document.make(mml::NodeKind::math);
        auto* root = item.root;
        item.typesetRoot = makeScene();
        item.setMetrics(16, 8, 400, 1);
        item.inputData.recompile = core::MathRecompile{core::STATE.UNPROCESSED, 7};
        item.outputData.mtext_family = u"serif";
        item.state(core::STATE.INSERTED);
        item.state(core::STATE.UNPROCESSED, true);
        expect(item.events == std::vector<std::string>({"remove", "clear"}));
        expect(item.removals.size() == 1_u);
        expect(item.removals[0].restore);
        expect(item.removals[0].state == core::STATE.INSERTED);
        expect(item.removals[0].hadInput);
        expect(item.removals[0].hadOutput);
        expect(!item.inputData.recompile.has_value());
        expect(!item.outputData.mtext_family.has_value());
        expect(item.root == root);
        expect(item.typesetRoot->metrics().width == 3.0_d);
        expect(item.metrics->containerWidth == 400.0_d);
        expect(item.state() == core::STATE.UNPROCESSED);

        item.state(core::STATE.INSERTED);
        item.reset();
        expect(item.removals.size() == 2_u);
        expect(!item.removals.back().restore);
    };

    "math_item_state_rechecks_current_state_after_virtual_clear"_test = [] {
        ObservedItem item(u"x", nullptr);
        item.inputData.recompile = core::MathRecompile{core::STATE.UNPROCESSED, 7};
        item.outputData.mtext_family = u"retained-after-removal";
        item.state(core::STATE.INSERTED);
        // Source state() reevaluates _state after removeFromDocument, rather than caching it.
        item.stateAfterClear = core::STATE.COMPILED;
        item.state(core::STATE.UNPROCESSED);
        expect(item.events == std::vector<std::string>({"remove", "clear"}));
        expect(item.outputData.mtext_family == std::optional<std::u16string>(u"retained-after-removal"));
        expect(!item.inputData.recompile.has_value());
        expect(item.state() == core::STATE.UNPROCESSED);
    };

    "math_item_exact_thresholds_clear_only_crossed_data"_test = [] {
        core::AbstractMathItem item(u"x", nullptr);
        item.state(core::STATE.TYPESET);
        item.inputData.recompile = core::MathRecompile{core::STATE.UNPROCESSED, 7};
        item.outputData.merror_family = u"serif";
        item.state(core::STATE.TYPESET);
        expect(item.outputData.merror_family.has_value());
        item.state(core::STATE.COMPILED);
        expect(!item.outputData.merror_family.has_value());
        expect(item.inputData.recompile.has_value());
        item.state(static_cast<core::MathState>(19));
        expect(!item.inputData.recompile.has_value());
        // A record installed below COMPILED is not erased by another below-threshold transition.
        item.inputData.recompile = core::MathRecompile{static_cast<core::MathState>(19), 0};
        item.reset(true);
        expect(item.inputData.recompile.has_value());
        expect(item.inputData.recompile->counter == 0_i);
    };

    "math_item_renderer_delegation_and_rerender_predecessor_124"_test = [] {
        mml::MmlDocument document;
        core::AbstractMathItem item(u"x", nullptr);
        RecordingActions actions;
        item.render(actions, document);
        expect(!actions.calls[0].convert);
        expect(actions.calls[0].boundary == core::STATE.UNPROCESSED);
        expect(actions.calls[0].item == &item);
        expect(actions.calls[0].document == &document);

        item.state(core::STATE.TYPESET);
        item.inputData.recompile = core::MathRecompile{core::STATE.UNPROCESSED, 7};
        item.outputData.mtext_family = u"serif";
        item.rerender(actions, document);
        expect(actions.calls[1].boundary == core::STATE.RERENDER);
        expect(actions.calls[1].seenState == static_cast<core::MathState>(124));
        expect(actions.calls[1].hadInput);
        expect(!actions.calls[1].hadOutput);
        expect(item.state() == static_cast<core::MathState>(124));

        item.state(static_cast<core::MathState>(19));
        item.rerender(actions, document, core::STATE.COMPILED);
        expect(actions.calls[2].seenState == static_cast<core::MathState>(19));
        expect(actions.calls[2].boundary == core::STATE.COMPILED);
        item.convert(actions, document);
        expect(actions.calls[3].convert);
        expect(actions.calls[3].boundary == core::STATE.LAST);
        item.convert(actions, document, core::STATE.COMPILED);
        expect(actions.calls[4].boundary == core::STATE.COMPILED);
        expect(item.state() == static_cast<core::MathState>(19));
    };

    "math_item_recompile_captures_precompiled_state_and_restores_record_after_rollback"_test = [] {
        mml::MmlDocument document;
        RecordingInput input;
        input.captureCounter = 7;
        core::AbstractMathItem item(u"x", &input);
        item.compile(document);
        expect(item.state() == core::STATE.COMPILED);
        expect(item.inputData.recompile.has_value());
        auto const saved = *item.inputData.recompile;
        expect(saved == core::MathRecompile{core::STATE.UNPROCESSED, 7});
        auto* firstRoot = item.root;

        item.state(saved.state);
        expect(!item.inputData.recompile.has_value());
        expect(item.root == firstRoot);
        item.inputData.recompile = saved;
        input.captureCounter.reset();
        item.compile(document);
        expect(input.calls == 2_i);
        expect(input.states == std::vector<core::MathState>({core::STATE.UNPROCESSED, core::STATE.UNPROCESSED}));
        expect(input.recompiles[1].has_value());
        expect(*input.recompiles[1] == saved);
        expect(*item.inputData.recompile == saved);
        expect(item.state() == core::STATE.COMPILED);
        expect(item.root != firstRoot);
        expect(&item.root->document() == &document);
    };

    "math_item_source_default_clear_keeps_values_and_scene_outlives_job"_test = [] {
        std::optional<scene::Scene> result;
        {
            mml::MmlDocument document;
            RecordingInput input;
            RecordingOutput output;
            core::AbstractMathItem item(u"x", &input);
            item.compile(document);
            item.typeset(output, document);
            auto* root = item.root;
            item.inputData.recompile = core::MathRecompile{core::STATE.UNPROCESSED, 7};
            item.outputData.mtext_family = u"serif";
            item.clear();
            item.updateDocument();
            item.removeFromDocument(true);
            expect(item.root == root);
            expect(item.typesetRoot.has_value());
            expect(item.inputData.recompile.has_value());
            expect(item.outputData.mtext_family.has_value());
            expect(item.state() == core::STATE.TYPESET);
            result = std::move(item.typesetRoot);
        }
        expect(result.has_value());
        expect(result->metrics() == scene::Metrics{3, .75, .25});
        expect(result->root().metadata.source_tag == "native-math-item-output");
    };
};
}
