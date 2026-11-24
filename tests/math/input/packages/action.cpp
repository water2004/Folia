#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.action.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 3> actionCallbacks{
    &ActionMethods::Toggle, &ActionMethods::Mathtip, ActionMethods::Macro};
thread_local std::string *actionError{};
thread_local std::u16string *actionMessage{};
MmlNode &formatActionError(TeX &jax, TexError const &error) {
    if (actionError)
        *actionError = error.id();
    if (actionMessage)
        *actionMessage = error.message();
    return jax.formatError(error);
}
struct ActionFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{actionCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit ActionFixture(bool aliases = false, bool selected = true) {
        ActionConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected)
            options.packages->push_back({u"action", {}});
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatActionError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *oldError{actionError};
            std::u16string *oldMessage{actionMessage};
            ~Restore() {
                actionError = oldError;
                actionMessage = oldMessage;
            }
        } restore;
        actionError = &error;
        actionMessage = &message;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
};
std::vector<MmlNode *> actionNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string actionText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
struct ActionRequest {
    std::vector<MmlNode *> children;
    NodeDefinition definition;
};
thread_local std::vector<ActionRequest> *actionRequests{};
MmlNode &observeAction(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                       NodeDefinition definition, MmlNode *text) {
    if (actionRequests && kind == NodeKind::maction)
        actionRequests->push_back({{children.begin(), children.end()}, definition});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace

suite action = [] {
    "action original callbacks texttip replacement and explicit package selection"_test = [] {
        ActionFixture f;
        expect(ActionMethods::Macro == &BaseMethods::Macro);
        const std::pair<std::u16string_view, MacroHandler> handlers[]{
            {u"toggle", &ActionMethods::Toggle},
            {u"mathtip", &ActionMethods::Mathtip},
            {u"texttip", ActionMethods::Macro}};
        auto map = f.maps.getMap(u"action-macros");
        for (auto const &[name, handler] : handlers) {
            auto macro = std::get<Macro const *>(map->lookup(name));
            expect(std::get<MacroHandler>(macro->function) == handler);
        }
        auto texttip = std::get<Macro const *>(map->lookup(u"texttip"));
        expect(texttip->arguments.size() == 2_u);
        if (texttip->arguments.size() == 2) {
            expect(std::get<std::u16string>(texttip->arguments[0]) == u"\\mathtip{#1}{\\text{#2}}");
            expect(std::get<double>(texttip->arguments[1]) == 2.0);
        }
        ActionFixture absent(false, false);
        absent.compile(u"\\mathtip{x}{tip}");
        expect(absent.error == "UndefinedControlSequence");
    };
    "action toggle branches retain order empty branches and exact braced or unbraced terminators"_test =
        [] {
            ActionFixture f;
            struct Case {
                std::u16string_view source;
                std::vector<std::u16string_view> children;
            };
            const Case cases[]{
                {u"\\toggle{a}{b}{c}\\endtoggle", {u"a", u"b", u"c"}},
                {u"\\toggle a b\\endtoggle", {u"a", u"b"}},
                {u"\\toggle{x}{\\endtoggle}", {u"x"}},
                {u"\\toggle{}{}{x}\\endtoggle", {u"", u"", u"x"}},
                // Source inheritance fills the arity-one node when no branch was supplied.
                {u"\\toggle\\endtoggle", {u""}},
                {u"\\toggle{\\endtoggle}", {u""}}};
            for (auto const &item : cases) {
                auto actions = actionNodes(f.ok(std::u16string(item.source)), NodeKind::maction);
                expect(actions.size() == 1_u);
                if (actions.empty())
                    continue;
                expect(attr(*actions[0], u"actiontype") == u"toggle");
                expect(actions[0]->children().size() == item.children.size());
                for (std::size_t i = 0;
                     i < std::min(actions[0]->children().size(), item.children.size()); ++i)
                    expect(std::ranges::equal(actionText(*actions[0]->children()[i]),
                                              item.children[i]));
            }
        };
    "action mathtip texttip and nested actions retain math and tooltip child order"_test = [] {
        ActionFixture f;
        struct Case {
            std::u16string_view source, math, tip;
        };
        for (auto const &item : {Case{u"\\mathtip{x+y}{z^2}", u"x+y", u"z2"},
                                 {u"\\mathtip{}{}", u"", u""},
                                 {u"\\texttip{x}{plain text}", u"x", u"plain text"}}) {
            auto actions = actionNodes(f.ok(std::u16string(item.source)), NodeKind::maction);
            expect(actions.size() == 1_u);
            if (actions.empty())
                continue;
            expect(attr(*actions[0], u"actiontype") == u"tooltip");
            expect(actions[0]->children().size() == 2_u);
            if (actions[0]->children().size() == 2) {
                expect(std::ranges::equal(actionText(*actions[0]->children()[0]), item.math));
                expect(std::ranges::equal(actionText(*actions[0]->children()[1]), item.tip));
            }
        }
        for (auto const &[source, first, second] :
             {std::tuple<std::u16string_view, std::u16string_view, std::u16string_view>{
                  u"\\toggle{\\mathtip{x}{y}}{\\texttip{z}{tip}}\\endtoggle", u"toggle",
                  u"tooltip"},
              {u"\\mathtip{\\toggle{x}{y}\\endtoggle}{\\mathtip{a}{b}}", u"tooltip", u"toggle"}}) {
            auto actions = actionNodes(f.ok(std::u16string(source)), NodeKind::maction);
            expect(actions.size() == 3_u);
            if (actions.size() == 3) {
                expect(std::ranges::equal(attr(*actions[0], u"actiontype"), first));
                expect(std::ranges::equal(attr(*actions[1], u"actiontype"), second));
                expect(attr(*actions[2], u"actiontype") == u"tooltip");
            }
        }
    };
    "action local toggle parsers inherit current font without leaking branch changes"_test = [] {
        ActionFixture f;
        auto tokens =
            actionNodes(f.ok(u"\\mathbf{\\toggle{\\mathit{x}}{y}\\endtoggle z}"), NodeKind::mi);
        expect(tokens.size() == 3_u);
        if (tokens.size() == 3) {
            expect(NodeUtil::getText(*tokens[0]) == u"x");
            expect(attr(*tokens[0], u"mathvariant") == u"-tex-mathit");
            expect(NodeUtil::getText(*tokens[1]) == u"y");
            expect(NodeUtil::getText(*tokens[2]) == u"z");
            expect(attr(*tokens[1], u"mathvariant") == u"bold");
            expect(attr(*tokens[2], u"mathvariant") == u"bold");
        }
        tokens = actionNodes(f.ok(u"\\mathbf{\\mathtip{x}{y}}"), NodeKind::mi);
        expect(tokens.size() == 2_u);
        for (auto token : tokens)
            expect(attr(*token, u"mathvariant") == u"bold");
    };
    "action configured creator receives original child borrows definitions and zero branch request"_test =
        [] {
            ActionFixture f;
            std::vector<ActionRequest> requests;
            struct Restore {
                ~Restore() {
                    actionRequests = nullptr;
                }
            } restore;
            actionRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeAction});
            auto actions =
                actionNodes(f.ok(u"\\toggle{x}{y}\\endtoggle+\\mathtip{a}{b}"), NodeKind::maction);
            expect(requests.size() == 2_u && actions.size() == 2_u);
            if (requests.size() == 2 && actions.size() == 2) {
                expect(attr(requests[0].definition.attributes, u"actiontype") == u"toggle");
                expect(attr(requests[1].definition.attributes, u"actiontype") == u"tooltip");
                for (std::size_t i = 0; i < 2; ++i) {
                    expect(requests[i].children.size() == 2_u &&
                           actions[i]->children().size() == 2_u);
                    if (requests[i].children.size() == 2 && actions[i]->children().size() == 2)
                        for (std::size_t j = 0; j < 2; ++j)
                            expect(requests[i].children[j] == actions[i]->children()[j]);
                }
            }
            f.ok(u"\\toggle\\endtoggle");
            expect(requests.size() == 3_u);
            if (requests.size() == 3)
                expect(requests[2].children.empty());
        };
    "action missing arguments exact terminator and child errors release parser borrows and recover"_test =
        [] {
            struct Case {
                std::u16string_view source;
                std::string_view error;
            };
            for (auto const &item :
                 {Case{u"\\toggle", "MissingArgFor"},
                  {u"\\toggle{x}", "MissingArgFor"},
                  {u"\\toggle{\\unknown}\\endtoggle", "UndefinedControlSequence"},
                  {u"\\toggle{\\endtoggle extra}\\endtoggle", "UndefinedControlSequence"},
                  {u"\\toggle{{x}\\endtoggle", "MissingCloseBrace"},
                  {u"\\mathtip", "MissingArgFor"},
                  {u"\\mathtip{x}", "MissingArgFor"},
                  {u"\\mathtip{\\unknown}{tip}", "UndefinedControlSequence"},
                  {u"\\mathtip{x}{\\unknown}", "UndefinedControlSequence"},
                  {u"\\texttip{x}", "MissingArgFor"},
                  {u"\\endtoggle", "UndefinedControlSequence"}}) {
                ActionFixture f;
                f.compile(std::u16string(item.source));
                expect(f.error == item.error);
                expect(actionNodes(f.ok(u"\\mathtip{x}{y}"), NodeKind::maction).size() == 1_u);
            }
        };
    "action formal let aliases retain compiled methods template arguments and input error name"_test =
        [] {
            ActionFixture f(true);
            f.ok(u"\\let\\savedToggle\\toggle\\let\\savedTip\\mathtip\\let\\savedText\\texttip");
            auto actions = actionNodes(
                f.ok(u"\\savedToggle{\\savedTip{x}{y}}{\\savedText{z}{tip}}\\endtoggle"),
                NodeKind::maction);
            expect(actions.size() == 3_u);
            if (actions.size() == 3) {
                expect(attr(*actions[0], u"actiontype") == u"toggle");
                expect(attr(*actions[1], u"actiontype") == u"tooltip");
                expect(attr(*actions[2], u"actiontype") == u"tooltip");
            }
            f.compile(u"\\savedToggle{x}");
            expect(f.error == "MissingArgFor");
            expect(f.message == u"Missing argument for \\savedToggle");
            f.ok(u"\\savedTip{x}{y}");
        };
};
