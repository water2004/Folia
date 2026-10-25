#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    suite inline_format_controls_tests = [] {
        "complete inline formatting remains visually stable at every source offset"_test = [] {
            struct Example { wchar_t const* source; std::u32string_view visible; };
            for (auto const& example : {
                Example{L"**abc** tail\n", U"abc tail"},
                Example{L"__abc__ tail\n", U"abc tail"},
                Example{L"_abc_ tail\n", U"abc tail"},
                Example{L"*abc* tail\n", U"abc tail"},
                Example{L"~~abc~~ tail\n", U"abc tail"},
                Example{L"`abc` tail\n", U"abc tail"},
                Example{L"**a\U0001F600b** tail\n", U"a\U0001F600b tail"},
                Example{L"a***b***c\n", U"abc"},
                Example{L"\\*abc\\*\n", U"*abc*"},
                Example{L"# **abc**\n", U"abc"},
                Example{L"**abc\n", U"**abc"},
                Example{L"~~abc\n", U"~~abc"},
            }) {
                EditorSession session;
                session.SetText(example.source);
                auto original = session.Text();
                auto owner = session.Selection().active.container_id;
                auto source = session.EditableSource(owner);
                expect(source.has_value());
                if (!source) continue;
                MathJaxRenderer math;
                SvgNormalizer normalizer;
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto const& block = frame.renderModel.blocks.front();
                for (std::size_t offset = 0; offset <= source->size(); ++offset) {
                    auto caret = folia::TextPosition{owner, offset, folia::TextAffinity::Downstream};
                    auto display = BuildDisplayInlineText(block.inline_items, caret,
                        {owner, source->size(), folia::TextAffinity::Upstream}, math, normalizer,
                        D2D1::ColorF(D2D1::ColorF::Black), 16, 500, false, false, false);
                    expect(std::u32string_view{display.text} == example.visible) << offset;
                    expect(DisplayPositionForSource(display.displayToSource, caret) < display.displayToSource.size());
                }
                expect(session.Text() == original);
                expect(math.Snapshot().entries == 0_u);
            }
        };

        "format controls reflect selection and use the existing reversible source edit"_test = [] {
            EditorSession session;
            session.SetText(L"__a\U0001F600b__ tail\n");
            auto original = session.Text();
            auto owner = session.Selection().active.container_id;
            auto caret = folia::TextPosition{owner, 3, folia::TextAffinity::Downstream};
            session.SetSelection(caret, caret);
            expect(session.ActiveInlineStyle().bold);
            expect(!session.ActiveInlineStyle().italic);
            folia::Command toggle;
            toggle.kind = folia::CommandKind::ToggleStrong;
            expect(session.ExecuteCommand(toggle));
            expect(!session.ActiveInlineStyle().bold);
            expect(session.EditableSource(owner) == std::optional<std::u32string>{U"a\U0001F600b tail"});
            folia::Command undo;
            undo.kind = folia::CommandKind::Undo;
            expect(session.ExecuteCommand(undo));
            expect(session.Text() == original);
            expect(session.Selection() == folia::TextSelection::caret(caret));
            expect(session.ActiveInlineStyle().bold);
            expect(session.EnterSourceMode());
            expect(!session.ActiveInlineStyle().bold);
            expect(session.Text() == original);
            expect(session.ExitSourceMode());
            expect(session.Text() == original);
        };
    };
}
