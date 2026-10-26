#include "pch.h"
#include "support/folia_test.hpp"
#include "media/TreeSitterHighlighter.h"
#include "editor/interaction/CodeLanguageChoices.h"
#include "editor/interaction/EditorCodeLanguageController.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    SyntaxHighlightKind KindAt(std::vector<SyntaxHighlightRange> const& ranges, std::size_t offset)
    {
        for (auto const& range : ranges)
            if (offset >= range.start && offset < range.start + range.length) return range.kind;
        return SyntaxHighlightKind::None;
    }
}

suite code_language_tests = [] {
    "code_language_control_refresh_and_detach_never_commit_a_draft"_test = [] {
        using namespace winrt::Microsoft::UI::Xaml;
        using namespace winrt::Microsoft::UI::Xaml::Controls;
        auto xaml = Hosting::WindowsXamlManager::InitializeForCurrentThread();
        Grid root;
        SwapChainPanel panel;
        Flyout flyout;
        TextBox box;
        ListView languages;
        root.Children().Append(panel);
        flyout.Content(box);
        EditorSession session;
        EditorSurfaceRenderer renderer;
        session.SetText(L"```cpp\nvalue\n```");
        auto owner = session.RenderModel().blocks.front().id;
        session.SetSelection(folia::TextSelection::caret({owner, 7, folia::TextAffinity::Downstream}));
        renderer.Initialize(panel);
        renderer.Resize(panel, 900, 600);
        renderer.Render(session.RenderFrame());
        auto raw = renderer.RawBlockFor(owner);
        expect(raw.has_value()) << "standalone code layout must register its raw block";
        if (raw)
        {
            expect(raw->languageEditable) << "fenced code must allow language editing";
            expect(raw->language == std::optional<std::string>{"cpp"}) << "raw block must retain language";
            expect(raw->headerRect.bottom > raw->headerRect.top) << "code header must reserve a stable row";
            expect(raw->headerRect.left >= raw->rect.left
                && raw->headerRect.right <= raw->rect.right
                && raw->headerRect.top >= raw->rect.top
                && raw->headerRect.bottom <= raw->rect.bottom)
                << "code rectangle must contain the language header";
            auto content = renderer.CaretBounds({owner, 8, folia::TextAffinity::Downstream});
            expect(content.has_value()) << "code body must expose source caret geometry";
            if (content)
            {
                expect(content->left > raw->rect.left
                    && content->right <= raw->rect.right
                    && content->bottom <= raw->rect.bottom)
                    << "code background must contain its padded content";
                expect(raw->headerRect.bottom <= content->top + 0.5f)
                    << "language header must end before the code body";
            }
        }
        std::size_t commands = 0;
        EditorCodeLanguageController controller;
        controller.Attach(session, renderer, panel, flyout, box, languages, [&](auto const&) { ++commands; return true; });
        controller.Refresh();
        expect(!flyout.IsOpen()) << "focus alone must not overlay a textbox on the code block";
        if (raw) {
            auto x = (raw->headerRect.left + raw->headerRect.right) * .5f;
            auto y = (raw->headerRect.top + raw->headerRect.bottom) * .5f;
            expect(renderer.CodeLanguageAt(x, y).has_value());
            expect(!renderer.CodeLanguageAt(raw->rect.right + 1, y));
            expect(!renderer.CodeLanguageAt(x, raw->headerRect.bottom + 2));
        }
        auto before = session.TextUtf8();
        box.Text(L"python");
        // A closed picker has no permanent editor surface or scrolling state.
        controller.Refresh();
        expect(!flyout.IsOpen());
        expect(commands == 0_u) << "refresh must not submit a language command";
        expect(session.TextUtf8() == before) << "draft must not change source";
        box.Text(L"rust");
        controller.Detach();
        expect(commands == 0_u) << "detach must not submit a language command";
        expect(box.Visibility() == Visibility::Collapsed) << "detach must hide selector";
        expect(!flyout.IsOpen()) << "detached picker must not intercept input";
        renderer.SetInvalidateCallback({});
    };

    "code_language_search_accepts_aliases_and_preserves_custom_tokens"_test = [] {
        expect(code_language::Filter("C++") == std::vector<std::string>{"cpp"});
        expect(code_language::Filter("PY") == std::vector<std::string>{"python"});
        expect(code_language::Filter("pwsh") == std::vector<std::string>{"powershell"});
        expect(code_language::Filter("does-not-exist").empty());
        expect(code_language::Filter("").size() == std::size(code_language::choices));
        expect(code_language::Parse("  MyLanguage\t") == std::optional<std::string>{"MyLanguage"});
        expect(code_language::Parse("\t ") == std::optional<std::string>{""});
        expect(!code_language::Parse("cpp metadata").has_value());
        expect(!code_language::Parse("cpp\npython").has_value());
        expect(!code_language::Parse("cpp`bad").has_value());
    };

    "code_highlight_calls_do_not_color_arguments_or_function_body"_test = [] {
        TreeSitterHighlighter highlighter;
        std::string code = "function greet(user) { const local = user; return print(local); }";
        auto ranges = highlighter.Highlight("javascript", code);
        expect(KindAt(ranges, code.find("greet")) == SyntaxHighlightKind::Function);
        expect(KindAt(ranges, code.find("print")) == SyntaxHighlightKind::Function);
        expect(KindAt(ranges, code.find("user")) == SyntaxHighlightKind::None);
        expect(KindAt(ranges, code.find("local")) == SyntaxHighlightKind::None);
        expect(KindAt(ranges, code.rfind("local")) == SyntaxHighlightKind::None);
        expect(KindAt(ranges, code.find("return")) == SyntaxHighlightKind::Keyword);
    };

    "code_highlight_preprocessor_preserves_nested_code_tokens"_test = [] {
        TreeSitterHighlighter highlighter;
        std::string code = "#ifdef DEBUG\nint value = 42; // note\n#endif\n";
        auto ranges = highlighter.Highlight("c++", code);
        expect(KindAt(ranges, code.find("#ifdef")) == SyntaxHighlightKind::Preprocessor);
        expect(KindAt(ranges, code.find("int")) == SyntaxHighlightKind::Type);
        expect(KindAt(ranges, code.find("42")) == SyntaxHighlightKind::Number);
        expect(KindAt(ranges, code.find("//")) == SyntaxHighlightKind::Comment);
        expect(KindAt(ranges, code.find("value")) == SyntaxHighlightKind::None);
    };

    "code_highlight_unicode_ranges_and_language_switch_are_stable"_test = [] {
        TreeSitterHighlighter highlighter;
        // UTF-8 multibyte content precedes another token; offsets are codepoints.
        std::string code = "const s = \"\xF0\x9F\x98\x80\"; const n = 42;";
        auto ranges = highlighter.Highlight("js", code);
        expect(KindAt(ranges, code.find("42") - 3) == SyntaxHighlightKind::Number);
        expect(highlighter.Highlight("text", code).empty());
        auto again = highlighter.Highlight("javascript", code);
        expect(again.size() == ranges.size());
        for (std::size_t index = 0; index < ranges.size(); ++index) {
            expect(again[index].start == ranges[index].start);
            expect(again[index].length == ranges[index].length);
            expect(again[index].kind == ranges[index].kind);
        }
    };

    "code_highlight_template_expressions_keep_code_colors"_test = [] {
        TreeSitterHighlighter highlighter;
        std::string code = "const message = `Hello ${format(user, 42)}`;";
        auto ranges = highlighter.Highlight("javascript", code);
        expect(KindAt(ranges, code.find("Hello")) == SyntaxHighlightKind::String);
        expect(KindAt(ranges, code.find("format")) == SyntaxHighlightKind::Function);
        expect(KindAt(ranges, code.find("user")) == SyntaxHighlightKind::None);
        expect(KindAt(ranges, code.find("42")) == SyntaxHighlightKind::Number);
    };
};
