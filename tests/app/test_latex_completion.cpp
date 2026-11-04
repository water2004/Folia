#include "pch.h"
#include "support/folia_test.hpp"
#include "settings/LatexCommandStore.h"
#include "editor/interaction/EditorKeyboardController.h"
#include "media/NativeMathSession.h"
import folia.core.snippet_template;
import folia.markdown.instrumentation;
import folia.markdown.utf;

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
auto Catalog() { return LatexCommandStore{}.Load().builtIn; }
auto const& Find(std::vector<folia::LatexCommandDefinition> const& catalog, std::u32string_view name) {
    auto it = std::ranges::find(catalog, name, &folia::LatexCommandDefinition::trigger);
    if (it == catalog.end()) throw std::runtime_error("missing completion " + folia::cps_to_utf8(name));
    return *it;
}
std::optional<folia::TextSpan> Formula(folia::RenderBlock const& block) {
    if (block.kind == folia::RenderBlockKind::Math) return block.content_span;
    for (auto const& child : block.child_blocks) if (auto found = Formula(child)) return found;
    for (auto const& item : block.inline_items)
        if (item.kind == folia::InlineRenderItem::Kind::Math && item.special().semantic().content_range)
            return folia::TextSpan{item.source_span.container_id, *item.special().semantic().content_range};
    for (auto const& cell : block.special().table_cells) for (auto const& item : cell)
        if (item.kind == folia::InlineRenderItem::Kind::Math && item.special().semantic().content_range)
            return folia::TextSpan{item.source_span.container_id, *item.special().semantic().content_range};
    return std::nullopt;
}
folia::TextSpan Formula(EditorSession& session) {
    auto frame = session.RenderFrame();
    if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
    for (auto const& block : frame.renderModel.blocks) if (auto found = Formula(block)) return *found;
    throw std::runtime_error("test formula missing");
}
suite latex_completion_app = [] {
    "shipped math catalog parses and every trigger is reachable with its exact spelling"_test = [] {
        const auto catalog = Catalog();
        expect(fatal(catalog.size() > 1700_u));
        for (auto const& command : catalog) {
            const auto parsed = folia::parse_snippet_template(command.snippet);
            expect(!parsed.text.empty()) << command.id;
            expect(fatal(!parsed.tab_stops.empty())) << command.id;
            expect(parsed.tab_stops.back().index == 0_u) << command.id;
            expect(parsed.tab_stops.back().range == folia::SourceRange{parsed.text.size(), parsed.text.size()}) << command.id;
            for (auto const& stop : parsed.tab_stops) expect(stop.range.valid_for(parsed.text.size())) << command.id;
            folia::platform::editor::EditorSnippetSession navigation;
            const auto owner = folia::NodeId{41};
            auto selected = navigation.Start(owner, 5, parsed.tab_stops);
            for (auto const& stop : parsed.tab_stops) {
                expect(fatal(selected.has_value()));
                expect(selected->anchor.source_offset == stop.range.start + 5) << command.id;
                expect(selected->active.source_offset == stop.range.end + 5) << command.id;
                selected = navigation.Navigate(*selected, false).selection;
            }
            expect(!navigation.Active());
            auto typed = U"\\" + command.trigger;
            auto query = folia::query_latex_commands_at(catalog, typed, typed.size(), {}, 0, 1);
            expect(fatal(query.has_value())) << command.id;
            expect(fatal(query->candidates.size() == 1_u)) << command.id;
            expect(query->candidates.front().command.id == command.id) << command.id;
            expect(query->candidates.front().exact_match);
        }
        expect(folia::parse_snippet_template(Find(catalog, U"$").snippet).text == U"\\$");
        expect(folia::parse_snippet_template(Find(catalog, U"sub").snippet).text == U"\\sub");
        expect(folia::parse_snippet_template(Find(catalog, U"sup").snippet).text == U"\\sup");
    };
    "paired environment completion preserves multiline source and argument selection in nested blocks"_test = [] {
        const auto catalog = Catalog();
        for (auto markdown : {L"$$\n\\begin{arr\n$$", L"> $$\n> \\begin{arr\n> $$",
            L"- [ ] before\n\n  $$\n  \\begin{arr\n  $$"}) {
            EditorSession session; session.SetText(markdown);
            const auto span = Formula(session);
            const auto original = *session.EditableSource(span.container_id);
            const auto base = original.find(U"\\begin{arr");
            auto caret = folia::TextPosition{span.container_id, base + 10};
            session.SetSelection(caret, caret);
            const auto before = session.Selection();
            auto context = session.LatexCompletionSourceAtCaret();
            expect(fatal(context.has_value()));
            expect(context->container == span.container_id && context->caret == caret.source_offset);
            auto query = folia::query_latex_commands_at(catalog, original, caret.source_offset, {}, 0);
            expect(fatal(query.has_value()));
            EditorSurfaceRenderer renderer; EditorTextInputController textInput; EditorKeyboardController keyboard;
            keyboard.Attach(session, renderer, textInput,
                [&](auto const& command) { return session.ExecuteCommand(command); }, {}, {}, {}, {}, {}, {});
            folia::reset_core_operation_counters();
            expect(fatal(keyboard.InsertSnippetReplacing(span.container_id, query->replacement,
                Find(catalog, U"begin{array}").snippet)));
            auto expected = original; expected.replace(base, 10, U"\\begin{array}{cc}\n\n\\end{array}");
            expect(session.EditableSource(span.container_id) == std::optional{expected});
            const auto after = session.Selection();
            expect(session.SelectedSource() == U"cc");
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
            expect(session.Selection() == before);
            expect(session.EditableSource(span.container_id) == std::optional{original});
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Redo})));
            expect(session.Selection() == after);
            expect(session.EditableSource(span.container_id) == std::optional{expected});
            const auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            keyboard.Detach();
        }
    };
    "math completion replaces source atomically and restores both selections through history"_test = [] {
        const auto catalog = Catalog();
        for (auto markdown : {L"$\\fr$", L"> $\\fr$", L"- $\\fr$", L"- [ ] $\\fr$",
            L"| $\\fr$ |\n| --- |", L"> [!NOTE]\n> $\\fr$", L"# $\\fr$",
            L"$$\n\\fr\n$$", L"> $$\n> \\fr\n> $$", L"- before\n\n  $$\n  \\fr\n  $$"}) {
            EditorSession session; session.SetText(markdown);
            const auto span = Formula(session);
            const auto original = *session.EditableSource(span.container_id);
            auto caret = folia::TextPosition{span.container_id, span.source_range.start + 3, folia::TextAffinity::Upstream};
            session.SetSelection(caret, caret);
            const auto before = session.Selection();
            auto query = folia::query_latex_commands_at(catalog, original, caret.source_offset, {}, 0);
            expect(fatal(query.has_value()));
            EditorSurfaceRenderer renderer; EditorTextInputController textInput; EditorKeyboardController keyboard;
            keyboard.Attach(session, renderer, textInput,
                [&](auto const& command) { return session.ExecuteCommand(command); }, {}, {}, {}, {}, {}, {});
            folia::reset_core_operation_counters();
            const auto& snippet = Find(catalog, U"frac").snippet;
            expect(fatal(keyboard.InsertSnippetReplacing(span.container_id, query->replacement, snippet)));
            const auto after = session.Selection();
            auto expected = original; expected.replace(query->replacement.start, query->replacement.length(), U"\\frac{}{}");
            expect(session.EditableSource(span.container_id) == std::optional{expected});
            expect(after.active.source_offset == query->replacement.start + 6);
            expect(keyboard.HasSnippetSession());
            expect(keyboard.SnippetPlaceholders().size() == 2_u);
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
            expect(session.EditableSource(span.container_id) == std::optional{original});
            expect(session.Selection() == before) << "undo selection";
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Redo})));
            expect(session.EditableSource(span.container_id) == std::optional{expected});
            expect(session.Selection() == after) << "redo selection";
            const auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            keyboard.Detach();
        }
    };
    "new math templates render through the production native engine after filling arguments"_test = [] {
        struct Case { std::u32string_view trigger; std::vector<std::u32string> values; };
        const auto catalog = Catalog();
        NativeMathSession renderer;
        auto cases = std::vector<Case>{
            {U"boldsymbol", {U"\\mu"}}, {U"mathbfit", {U"x"}}, {U"cancelto", {U"0", U"x"}},
            {U"ce", {}}, {U"pu", {}}, {U"dv", {U"y", U"x"}}, {U"pdv", {U"f", U"x"}},
            {U"xhookrightarrow", {U"n", U"a"}}, {U"widetilde", {U"ab"}},
            {U"bra", {U"x"}}, {U"ket", {U"y"}}, {U"mel", {U"a", U"H", U"b"}},
            {U"begin{array}", {}}, {U"begin{alignat}", {}}, {U"begin{alignedat}", {}},
            {U"begin{darray}", {}}, {U"mqty", {}}, {U"qty", {U"x"}},
            {U"lang", {}}, {U"rang", {}}};
        for (auto const& command : catalog) {
            if (command.trigger.starts_with(U"begin{") && command.trigger.ends_with(U"}"))
                cases.push_back({command.trigger, {}});
        }
        for (auto const& item : cases) {
            auto parsed = folia::parse_snippet_template(Find(catalog, item.trigger).snippet);
            auto stops = parsed.tab_stops;
            std::ranges::sort(stops, std::greater{}, [](auto const& stop) { return stop.range.start; });
            for (auto const& stop : stops) {
                if (stop.index && stop.index <= item.values.size())
                    parsed.text.replace(stop.range.start, stop.range.length(), item.values[stop.index - 1]);
                else if (stop.index && stop.range.empty() && item.trigger.starts_with(U"begin{"))
                    parsed.text.insert(stop.range.start, U"x");
            }
            const auto tex = folia::cps_to_utf8(parsed.text);
            const auto svg = renderer.Render(tex, true, 18, 1000);
            expect(svg.errorKind == MathJaxErrorKind::None) << tex << ": " << svg.error;
            expect(bool(svg)) << tex;
        }
    };
};
}

