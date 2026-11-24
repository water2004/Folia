#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.bbox.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 1> boxCallbacks{&BboxMethods::BBox};
thread_local std::string *boxError{};
thread_local std::u16string *boxMessage{};
MmlNode &formatBoxError(TeX &jax, TexError const &error) {
    if (boxError)
        *boxError = error.id();
    if (boxMessage)
        *boxMessage = error.message();
    return jax.formatError(error);
}
struct BoxFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{boxCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit BoxFixture(bool aliases = false) {
        BboxConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"bbox", {}}};
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatBoxError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *previousError{boxError};
            std::u16string *previousMessage{boxMessage};
            ~Restore() {
                boxError = previousError;
                boxMessage = previousMessage;
            }
        } restore;
        boxError = &error;
        boxMessage = &message;
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
std::vector<MmlNode *> boxNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
struct BoxRequest {
    NodeKind kind;
    NodeDefinition definition;
};
thread_local std::vector<BoxRequest> *boxRequests{};
MmlNode &observeBox(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                    NodeDefinition definition, MmlNode *text) {
    if (boxRequests && (kind == NodeKind::mpadded || kind == NodeKind::mstyle))
        boxRequests->push_back({kind, definition});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace
suite bbox = [] {
    "bbox preserves original map and avoids extra wrappers for absent empty or comma options"_test =
        [] {
            BoxFixture f;
            expect(f.registry.get(u"bbox")->handler.size() == 4_u);
            auto entry = std::get<Macro const *>(f.maps.getMap(u"bbox")->lookup(u"bbox"));
            expect(std::get<MacroHandler>(entry->function) == &BboxMethods::BBox);
            for (auto source :
                 {u"\\bbox{x}", u"\\bbox[]{x}", u"\\bbox[,,,]{x}", u"\\bbox[\u00A0, \uFEFF]{x}"}) {
                auto &tree = f.ok(source);
                expect(boxNodes(tree, NodeKind::mpadded).empty());
                expect(boxNodes(tree, NodeKind::mstyle).empty());
                auto tokens = boxNodes(tree, NodeKind::mi);
                expect(tokens.size() == 1_u);
                if (!tokens.empty())
                    expect(NodeUtil::getText(*tokens[0]) == u"x");
            }
        };
    "bbox padding keeps spelling while doubled width follows original numeric formatting"_test =
        [] {
            BoxFixture f;
            for (auto unit : {u"pt", u"em", u"ex", u"mu", u"px", u"in", u"cm", u"mm"}) {
                const auto pad = u"001.50" + std::u16string(unit);
                auto nodes = boxNodes(f.ok(u"\\bbox[" + pad + u"]{x}"), NodeKind::mpadded);
                expect(nodes.size() == 1_u);
                if (nodes.empty())
                    continue;
                expect(std::ranges::equal(attr(*nodes[0], u"height"), u"+" + pad));
                expect(std::ranges::equal(attr(*nodes[0], u"depth"), u"+" + pad));
                expect(std::ranges::equal(attr(*nodes[0], u"lspace"), pad));
                expect(std::ranges::equal(attr(*nodes[0], u"width"), u"+3" + std::u16string(unit)));
            }
            for (auto const &[padding, width] :
                 {std::pair<std::u16string_view, std::u16string_view>{u".25em", u"+0.5em"},
                  {u"0em", u"+0em"},
                  {u"1.em", u"+2em"},
                  {u"00000000000000000001px", u"+2px"}}) {
                auto nodes = boxNodes(f.ok(u"\\bbox[" + std::u16string(padding) + u"]{x}"),
                                      NodeKind::mpadded);
                expect(nodes.size() == 1_u);
                if (!nodes.empty())
                    expect(attr(*nodes[0], u"width") == width);
            }
        };
    "bbox background and style choices wrap original parsed math in the original order"_test = [] {
        BoxFixture f;
        for (auto color : {u"Red", u"123", u"#aF3", u"#AbC123", u"2EM", u"1pc"}) {
            auto nodes =
                boxNodes(f.ok(u"\\bbox[" + std::u16string(color) + u"]{x}"), NodeKind::mstyle);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(attr(*nodes[0], u"mathbackground") == color);
        }
        for (auto style : {u"border:1px solid red", u"-moz-border: 1em", u"foo:"}) {
            auto nodes =
                boxNodes(f.ok(u"\\bbox[" + std::u16string(style) + u"]{x}"), NodeKind::mstyle);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(attr(*nodes[0], u"style") == style);
        }
        auto &tree =
            f.ok(u"\\bbox[\u00A0 red \uFEFF, .25em , border:1px solid blue]{\\frac{a}{b}}");
        auto styles = boxNodes(tree, NodeKind::mstyle);
        auto pads = boxNodes(tree, NodeKind::mpadded);
        expect(styles.size() == 1_u && pads.size() == 1_u);
        if (styles.size() == 1 && pads.size() == 1) {
            expect(attr(*styles[0], u"mathbackground") == u"red");
            expect(attr(*styles[0], u"style") == u"border:1px solid blue");
            expect(attr(*pads[0], u"width") == u"+0.5em");
            expect(pads[0]->parent() && pads[0]->parent()->parent() == styles[0]);
            expect(boxNodes(*pads[0], NodeKind::mfrac).size() == 1_u);
        }
    };
    "bbox configured creators receive padding before background and style wrappers"_test = [] {
        BoxFixture f;
        std::vector<BoxRequest> requests;
        struct Restore {
            ~Restore() {
                boxRequests = nullptr;
            }
        } restore;
        boxRequests = &requests;
        NodeCreators creators;
        creators.node = &observeBox;
        f.jax->parseOptions().request().nodeFactory.setCreators(creators);
        f.ok(u"\\bbox[red,.25em,border:1px solid blue]{x}");
        expect(requests.size() == 2_u);
        if (requests.size() == 2) {
            expect(requests[0].kind == NodeKind::mpadded);
            expect(requests[1].kind == NodeKind::mstyle);
            expect(attr(requests[0].definition.attributes, u"lspace") == u".25em");
            expect(attr(requests[1].definition.attributes, u"mathbackground") == u"red");
            expect(attr(requests[1].definition.attributes, u"style") == u"border:1px solid blue");
        }
    };
    "bbox errors preserve duplicate category invalid property and argument parsing order"_test =
        [] {
            BoxFixture f;
            struct Case {
                std::u16string_view options, message;
            };
            for (auto const &item :
                 {Case{u"1em,2em", u"Padding specified twice in \\bbox"},
                  Case{u"red,blue", u"Background specified twice in \\bbox"},
                  Case{u"border:red,foo:bar", u"Style specified twice in \\bbox"}}) {
                f.compile(u"\\bbox[" + std::u16string(item.options) + u"]{x}");
                expect(f.error == "MultipleBBoxProperty");
                expect(std::ranges::equal(f.message, item.message));
            }
            for (auto part :
                 {u"-1em", u"+1em", u"#12", u"#ggg", u"border :red", u"foo_bar:red", u"red blue"}) {
                f.compile(u"\\bbox[" + std::u16string(part) + u"]{x}");
                expect(f.error == "InvalidBBoxProperty");
                const auto message =
                    u"\"" + std::u16string(part) +
                    u"\" doesn't look like a color, a padding dimension, or a style";
                expect(std::ranges::equal(f.message, message));
                f.ok(u"\\bbox[red]{x}");
            }
            f.compile(u"\\bbox[-1em]{\\unknown}");
            expect(f.error == "UndefinedControlSequence");
            f.compile(u"\\bbox[-1em]");
            expect(f.error == "MissingArgFor");
        };
    "bbox let aliases retain original command identity and remain usable in following formulas"_test =
        [] {
            BoxFixture f(true);
            f.ok(u"\\let\\saved\\bbox");
            auto nodes = boxNodes(f.ok(u"\\saved[red,1em]{x}"), NodeKind::mpadded);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(attr(*nodes[0], u"width") == u"+2em");
            f.compile(u"\\saved[1em,2em]{x}");
            expect(f.error == "MultipleBBoxProperty");
            expect(f.message == u"Padding specified twice in \\bbox");
            f.ok(u"\\saved[blue]{x}");
        };
};
