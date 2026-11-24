#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.html.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 5> htmlCallbacks{&HtmlMethods::Data, &HtmlMethods::Href,
                                                    &HtmlMethods::Class, &HtmlMethods::Style,
                                                    &HtmlMethods::Id};
thread_local std::string *htmlError{};
thread_local std::u16string *htmlMessage{};
MmlNode &formatHtmlError(TeX &jax, TexError const &error) {
    if (htmlError)
        *htmlError = error.id();
    if (htmlMessage)
        *htmlMessage = error.message();
    return jax.formatError(error);
}
struct HtmlFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{htmlCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit HtmlFixture(bool aliases = false, bool selected = true) {
        HtmlConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected)
            options.packages->push_back({u"html", {}});
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatHtmlError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *oldError{htmlError};
            std::u16string *oldMessage{htmlMessage};
            ~Restore() {
                htmlError = oldError;
                htmlMessage = oldMessage;
            }
        } restore;
        htmlError = &error;
        htmlMessage = &message;
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
std::vector<MmlNode *> htmlNodes(MmlNode &root, std::u16string_view key) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.attributes().own(key))
            result.push_back(&node);
    });
    return result;
}
MmlNode *htmlOne(MmlNode &root, std::u16string_view key) {
    auto nodes = htmlNodes(root, key);
    expect(nodes.size() == 1_u);
    return nodes.size() == 1 ? nodes[0] : nullptr;
}
std::u16string codePoint(char32_t point) {
    if (point <= 0xffff)
        return std::u16string(1, static_cast<char16_t>(point));
    point -= 0x10000;
    return {static_cast<char16_t>(0xd800 + (point >> 10)),
            static_cast<char16_t>(0xdc00 + (point & 0x3ff))};
}
thread_local std::vector<NodeKind> *htmlRequests{};
MmlNode &observeHtml(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                     NodeDefinition definition, MmlNode *text) {
    if (htmlRequests)
        htmlRequests->push_back(kind);
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace
suite html = [] {
    "html original five command callbacks and explicit package selection"_test = [] {
        HtmlFixture f;
        auto map = f.maps.getMap(u"html_macros");
        expect(map != nullptr);
        const std::pair<std::u16string_view, MacroHandler> handlers[]{
            {u"data", &HtmlMethods::Data},
            {u"href", &HtmlMethods::Href},
            {u"class", &HtmlMethods::Class},
            {u"style", &HtmlMethods::Style},
            {u"cssId", &HtmlMethods::Id}};
        for (auto const &[name, handler] : handlers) {
            auto macro = std::get<Macro const *>(map->lookup(name));
            expect(std::get<MacroHandler>(macro->function) == handler);
        }
        HtmlFixture absent(false, false);
        absent.compile(u"\\href{a}{x}");
        expect(absent.error == "UndefinedControlSequence");
        expect(htmlOne(f.ok(u"\\href{a}{x}"), u"href") != nullptr);
    };
    "html argument attributes attach to single tokens fractions and explicit rows for multiple or empty content"_test =
        [] {
            HtmlFixture f;
            struct Case {
                std::u16string_view content;
                NodeKind kind;
                std::size_t children;
            };
            const Case cases[]{{u"x", NodeKind::mi, 1},
                               {u"xy", NodeKind::mrow, 2},
                               {u"x+y", NodeKind::mrow, 3},
                               {u"", NodeKind::mrow, 0},
                               {u"\\frac{a}{b}", NodeKind::mfrac, 2}};
            for (auto const &item : cases) {
                auto root = &f.ok(u"\\href{url?x=1&y=2#z}{" + std::u16string(item.content) + u"}");
                auto node = htmlOne(*root, u"href");
                if (!node)
                    continue;
                expect(node->kind() == item.kind);
                expect(node->children().size() == item.children);
                expect(attr(*node, u"href") == u"url?x=1&y=2#z");
            }
            auto node = htmlOne(f.ok(u"\\cssId{outer}{\\href{target}{xy}}"), u"id");
            if (node) {
                expect(node->kind() == NodeKind::mrow);
                expect(attr(*node, u"href") == u"target");
                expect(attr(*node, u"id") == u"outer");
            }
        };
    "html data preserves original key-value scalars braces escapes and repeated assignments"_test =
        [] {
            HtmlFixture f;
            auto node = htmlOne(f.ok(u"\\data{item={a,b=c},flag,falseval=false,raw=12,raw=13,trail="
                                     u"x\\ ,=empty}{xy}"),
                                u"data-item");
            if (!node)
                return;
            expect(node->kind() == NodeKind::mrow);
            expect(attr(*node, u"data-item") == u"a,b=c");
            expect(attr(*node, u"data-raw") == u"13");
            expect(attr(*node, u"data-trail") == u"x\\ ");
            expect(attr(*node, u"data-") == u"empty");
            auto yes = node->attributes().own(u"data-flag");
            auto no = node->attributes().own(u"data-falseval");
            expect(yes && std::holds_alternative<bool>(*yes) && std::get<bool>(*yes));
            expect(no && std::holds_alternative<bool>(*no) && !std::get<bool>(*no));
        };
    "html attribute names follow original code-unit and noncharacter rules"_test = [] {
        HtmlFixture f;
        const std::u16string ordinary[]{u"dash-name",
                                        u"under_score",
                                        u"colon:name",
                                        u"a<b",
                                        u"日本語",
                                        u"a\U0001F600b",
                                        std::u16string(1, char16_t{0xd800})};
        for (auto const &key : ordinary) {
            auto node = htmlOne(f.ok(u"\\data{" + key + u"=value}{x}"), u"data-" + key);
            if (node)
                expect(attr(*node, u"data-" + key) == u"value");
        }
        std::vector<std::u16string> invalid{u"a b", u"a\"b", u"a'b", u"a>b", u"a/b", u"a\tb"};
        for (auto c : {char16_t{0}, char16_t{0x1f}, char16_t{0x7f}, char16_t{0x9f}})
            invalid.push_back(u"a" + std::u16string(1, c) + u"b");
        for (char32_t c = 0xfdd0; c <= 0xfdef; ++c)
            invalid.push_back(codePoint(c));
        for (char32_t plane = 0; plane <= 16; ++plane) {
            invalid.push_back(codePoint((plane << 16) + 0xfffe));
            invalid.push_back(codePoint((plane << 16) + 0xffff));
        }
        for (auto const &key : invalid) {
            f.compile(u"\\data{" + key + u"=x}{y}");
            expect(f.error == "InvalidHTMLAttr") << f.error;
            expect(std::ranges::equal(f.message, u"Invalid HTML attribute: data-" + key));
        }
        f.ok(u"\\data{item=x}{y}");
    };
    "html class style and id combine in the exact original order including style length indexing"_test =
        [] {
            HtmlFixture f;
            auto node = htmlOne(f.ok(u"\\class{outer}{\\class{inner}{xy}}"), u"class");
            if (node)
                expect(attr(*node, u"class") == u"inner outer");
            node = htmlOne(f.ok(u"\\class{outer}{\\mmlToken{mi}[class=true]{x}}"), u"class");
            if (node)
                expect(attr(*node, u"class") == u"true outer");
            node = htmlOne(f.ok(u"\\cssId{outer}{\\cssId{inner}{x}}"), u"id");
            if (node)
                expect(attr(*node, u"id") == u"outer");
            struct Styles {
                std::u16string_view oldStyle, newStyle, result;
            };
            for (auto const &s :
                 {Styles{u"color:red", u"width:1em", u"color:red; width:1em"},
                  Styles{u"a;b", u"xy", u"a;b xy"}, Styles{u"x;", u"abc", u"x;; abc"},
                  Styles{u"x;", u"", u"x;; "}, Styles{u"", u"color:red", u"color:red"}}) {
                node = htmlOne(f.ok(u"\\style{" + std::u16string(s.newStyle) + u"}{\\style{" +
                                    std::u16string(s.oldStyle) + u"}{x}}"),
                               u"style");
                if (node)
                    expect(std::ranges::equal(attr(*node, u"style"), s.result));
            }
        };
    "html mrow construction uses configured creator once and keeps attributes on original row"_test =
        [] {
            HtmlFixture f;
            std::vector<NodeKind> requests;
            struct Restore {
                ~Restore() {
                    htmlRequests = nullptr;
                }
            } restore;
            htmlRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeHtml});
            auto node = htmlOne(f.ok(u"\\class{outer}{\\href{target}{xy}}"), u"class");
            expect(std::ranges::count(requests, NodeKind::mrow) == 1);
            if (node)
                expect(attr(*node, u"href") == u"target");
        };
    "html aliases preserve method identity and errors restore parser borrowing for following math"_test =
        [] {
            HtmlFixture f(true);
            f.ok(u"\\let\\saved\\href");
            auto node = htmlOne(f.ok(u"\\saved{target}{x}"), u"href");
            if (node)
                expect(attr(*node, u"href") == u"target");
            f.compile(u"\\data{bad key=x}{\\unknown}");
            expect(f.error == "UndefinedControlSequence");
            f.compile(u"\\data{bad key=x}{y}");
            expect(f.error == "InvalidHTMLAttr");
            for (auto command : {u"data", u"href", u"class", u"style", u"cssId"}) {
                f.compile(u"\\" + std::u16string(command) + u"{value}");
                expect(f.error == "MissingArgFor");
            }
            expect(throws<std::invalid_argument>(
                [&] { f.compile(u"\\style{color:red}{\\mmlToken{mi}[style=true]{x}}"); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"\\href{target}{y}");
        };
};