#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
import folia.markdown.instrumentation;
import folia.markdown.utf;
namespace {
using namespace boost::ut;
using namespace winrt::Folia;
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
folia::Command Type(char32_t c) {
    auto command = folia::Command::InsertText(std::u32string(1, c));
    command.typed_input = true; return command;
}
suite math_bracket_editing = [] {
    "math Enter creates the body before ordinary typing and bracket pairing"_test = [] {
        for (auto prefix : {U"", U"> ", U"- ", U"- [ ] ", U"12. [x] ", U"> - [ ] "})
        for (auto opening : {U"$$", U"\\[", U"```math"}) {
            EditorSession session; session.SetText(L"");
            for (auto c : std::u32string_view{prefix}) expect(fatal(session.ExecuteCommand(Type(c))));
            for (auto c : std::u32string_view{opening}) expect(fatal(session.ExecuteCommand(Type(c))));
            folia::reset_core_operation_counters();
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::InsertNewline})));
            auto caret = session.Selection().active;
            const auto closing = std::u32string_view{opening} == U"$$" ? U"$$"
                : std::u32string_view{opening} == U"\\[" ? U"\\]" : U"```";
            const auto head = std::u32string{opening} + U"\n";
            const auto tail = U"\n" + std::u32string{closing};
            const auto blank = head + tail;
            const auto original = session.Selection();
            expect(session.EditableSource(caret.container_id) == std::optional{blank});
            expect(caret.source_offset == head.size());
            for (auto pair : {U"()", U"[]", U"{}"}) {
                expect(fatal(session.ExecuteCommand(Type(pair[0]))));
                expect(session.EditableSource(caret.container_id) == std::optional{head + pair + tail});
                expect(session.Selection().active.source_offset == head.size() + 1);
                expect(fatal(session.ExecuteCommand(Type(U'x'))));
                expect(session.EditableSource(caret.container_id)
                    == std::optional{head + pair[0] + U"x" + pair[1] + tail});
                expect(session.Selection().active.source_offset == head.size() + 2);
                expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
                expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::DeleteBackward})));
                expect(session.EditableSource(caret.container_id) == std::optional{blank});
                expect(session.Selection() == original);
                expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
                expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
                expect(session.EditableSource(caret.container_id) == std::optional{blank});
                expect(session.Selection() == original);
            }
            expect(fatal(session.ExecuteCommand(Type(U'x'))));
            expect(session.EditableSource(caret.container_id) == std::optional{head + U"x" + tail});
            expect(session.Selection().active.source_offset == head.size() + 1);
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Undo})));
            expect(fatal(session.ExecuteCommand({.kind = folia::CommandKind::Redo})));
            expect(session.EditableSource(caret.container_id) == std::optional{head + U"x" + tail});
            auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            EditorSession reloaded;
            auto saved = session.TextUtf8();
            reloaded.SetText(winrt::to_hstring(saved));
            auto frame = reloaded.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            std::optional<folia::TextSpan> span;
            for (auto const& block : frame.renderModel.blocks) if (auto found = Formula(block)) span = found;
            expect(span.has_value()) << "reload prefix=" << folia::cps_to_utf8(prefix)
                << " opening=" << folia::cps_to_utf8(opening) << " saved=" << saved;
            if (span) expect(reloaded.EditableSource(span->container_id) == std::optional{head + U"x" + tail})
                << "reload prefix=" << folia::cps_to_utf8(prefix) << " opening=" << folia::cps_to_utf8(opening)
                << " actual=" << folia::cps_to_utf8(*reloaded.EditableSource(span->container_id)) << " saved=" << saved;
        }
    };
    "math bracket typing uses syntax matching comments escapes and scalable delimiters"_test = [] {
        struct Case { std::wstring tex; std::size_t caret; char32_t typed; std::u32string expected; std::size_t after; };
        for (auto const& t : {
            Case{L"", 0, U'(', U"()", 1}, {L"x", 0, U'(', U"(x", 1},
            {L")", 0, U'(', U"()", 1}, {L"(x)", 2, U')', U"(x)", 3},
            {L"{x}", 1, U'[', U"{[x}", 2}, {L"{}", 1, U'(', U"{()}", 2},
            {L"\\", 1, U'(', U"\\(", 2}, {L"% hello", 7, U'(', U"% hello(", 8},
            {L"\\left", 5, U'(', U"\\left(\\right)", 6},
            {L"\\left(\\right)", 6, U')', U"\\left(\\right)", 13},
            {L"\\left(\\right)", 6, U'{', U"\\left({}\\right)", 7},
            {L"\\left(\\right )", 6, U'{', U"\\left({}\\right )", 7},
            {L"\\left\\langle\\right\\rangle", 12, U'{', U"\\left\\langle{}\\right\\rangle", 13}}) {
            EditorSession session;
            session.SetText(winrt::hstring(L"$$\n" + t.tex + L"\n$$\n"));
            auto span = Formula(session);
            auto caret = folia::TextPosition{span.container_id, span.source_range.start + t.caret, folia::TextAffinity::Downstream};
            session.SetSelection(caret, caret);
            auto before = session.EditableSource(caret.container_id);
            auto selection = session.Selection();
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand(Type(t.typed)));
            auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            auto actual = session.EditableSource(caret.container_id);
            if (actual != std::optional<std::u32string>{U"$$\n" + t.expected + U"\n$$"})
                std::cout << "typing actual=" << folia::cps_to_utf8(*actual) << " expected=" << folia::cps_to_utf8(t.expected) << '\n';
            expect(actual == std::optional<std::u32string>{U"$$\n" + t.expected + U"\n$$"}) << t.caret;
            expect(session.Selection().active.source_offset == span.source_range.start + t.after);
            if (session.EditableSource(caret.container_id) != before) {
                expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
                expect(session.EditableSource(caret.container_id) == before);
                expect(session.Selection() == selection);
                expect(session.ExecuteCommand({.kind = folia::CommandKind::Redo}));
                expect(session.Selection().active.source_offset == span.source_range.start + t.after);
            }
        }
    };
    "math matched bracket deletion retains content and is one reversible transaction"_test = [] {
        for (auto text : {L"()", L"(x)", L"{(x)}", L"\\left(x\\right)"})
        for (bool backward : {false, true}) {
            EditorSession session; session.SetText(winrt::hstring(std::wstring{L"$$\n"} + text + L"\n$$\n"));
            auto span = Formula(session); auto raw = *session.EditableSource(span.container_id);
            auto at = folia::TextPosition{span.container_id, raw.find(U'(') + (backward ? 1 : 0), folia::TextAffinity::Downstream};
            session.SetSelection(at, at); auto original = session.Selection();
            expect(session.ExecuteCommand({.kind = backward ? folia::CommandKind::DeleteBackward : folia::CommandKind::DeleteForward}));
            auto edited = *session.EditableSource(span.container_id);
            expect(edited.find(U'(') == std::u32string::npos && edited.find(U')') == std::u32string::npos);
            expect(edited.find(U"\\left") == std::u32string::npos && edited.find(U"\\right") == std::u32string::npos);
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.EditableSource(span.container_id) == std::optional<std::u32string>{raw});
            expect(session.Selection() == original);
        }
    };
    "math bracket rules cover nested containers and preserve literal insertion"_test = [] {
        for (auto source : {L"$x$", L"> $x$", L"- $x$", L"- [ ] $x$",
            L"| $x$ |\n| --- |", L"> [!NOTE]\n> $x$",
            L"> $$\n> x\n> $$", L"- before\n\n  $$\n  x\n  $$"}) {
            EditorSession session; session.SetText(source); auto span = Formula(session);
            auto caret = folia::TextPosition{span.container_id, span.source_range.end, folia::TextAffinity::Downstream};
            session.SetSelection(caret, caret);
            expect(session.ExecuteCommand(Type(U'(')));
            auto raw = *session.EditableSource(caret.container_id);
            expect(raw.substr(caret.source_offset, 2) == U"()");
            expect(session.Selection().active.source_offset == caret.source_offset + 1);
            expect(session.ExecuteCommand({.kind = folia::CommandKind::DeleteBackward}));
            expect(session.Selection().active.source_offset == caret.source_offset);
        }
        for (auto source : {L"text", L"\u0060code\u0060", L"$x$"}) {
            EditorSession session; session.SetText(source); auto caret = session.Selection().active;
            if (source == std::wstring_view{L"$x$"}) { auto span = Formula(session); caret = {span.container_id, span.source_range.end, folia::TextAffinity::Downstream}; }
            session.SetSelection(caret, caret);
            auto original = *session.EditableSource(caret.container_id);
            expect(session.ExecuteCommand(folia::Command::InsertText(U"(")));
            original.insert(caret.source_offset, U"(");
            expect(session.EditableSource(caret.container_id) == std::optional<std::u32string>{original});
        }
    };
    "math bracket surround retains directed selection and Unicode source on undo"_test = [] {
        for (bool reversed : {false, true}) {
            EditorSession session; session.SetText(L"$\U0001f600+x$");
            auto span = Formula(session);
            auto start = folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
            auto end = folia::TextPosition{span.container_id, span.source_range.end, folia::TextAffinity::Upstream};
            session.SetSelection(reversed ? end : start, reversed ? start : end);
            auto before = session.Selection();
            auto source = session.EditableSource(span.container_id);
            expect(session.ExecuteCommand(Type(U'(')));
            expect(session.EditableSource(span.container_id) == std::optional<std::u32string>{U"$(\U0001f600+x)$"});
            expect(session.Selection().anchor.source_offset == before.anchor.source_offset + 1);
            expect(session.Selection().active.source_offset == before.active.source_offset + 1);
            expect(session.ExecuteCommand({.kind = folia::CommandKind::Undo}));
            expect(session.EditableSource(span.container_id) == source);
            expect(session.Selection() == before);
        }
    };
};
}
