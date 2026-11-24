#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.amscd.configuration;

namespace {
thread_local std::string *cdError{};
MmlNode &formatCdError(TeX &jax, TexError const &error) {
    if (cdError)
        *cdError = error.id();
    return jax.formatError(error);
}
struct CdFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit CdFixture(AmscdOptions options = {}) {
        AmsCdConfiguration::initializeModule(registry, maps);
        TexOptionValues values;
        values.packages = std::vector<PackageSelection>{{u"base", {}}, {u"amscd", {}}};
        values.amscd = std::move(options);
        values.formatError = &formatCdError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, values);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *previous{cdError};
            ~Restore() {
                cdError = previous;
            }
        } restore;
        cdError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(true);
        auto &result = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return result;
    }
    MmlNode &ok(std::u16string source) {
        auto &result = compile(std::move(source));
        expect(error.empty()) << error;
        return result;
    }
};
std::vector<MmlNode *> cdNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string cdText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
std::string cdRows(MmlNode &root) {
    auto tables = cdNodes(root, NodeKind::mtable);
    if (tables.empty())
        return {};
    std::string result;
    for (auto row : tables[0]->children())
        result += std::to_string(row->children().size()) + "/";
    return result;
}
bool cdNumber(MmlNode const &node, std::u16string_view name, double value) {
    auto current = node.attributes().lookup(name);
    return current && std::holds_alternative<double>(*current) &&
           std::get<double>(*current) == value;
}
std::size_t cdStruts(MmlNode &root) {
    auto nodes = cdNodes(root, NodeKind::mpadded);
    return std::ranges::count_if(nodes, [](auto n) {
        return attr(*n, u"height") == u"8.5pt" && attr(*n, u"depth") == u"2pt";
    });
}
struct CdRequest {
    std::u16string text;
    NodeDefinition definition;
};
thread_local std::vector<CdRequest> *cdRequests{};
MmlNode &observeCdToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                        std::optional<std::u16string_view> text) {
    if (cdRequests && kind == NodeKind::mo)
        cdRequests->push_back({std::u16string(text.value_or(u"")), definition});
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
} // namespace

suite amscd = [] {
    "amscd configuration preserves three original maps defaults and existing ArrayItem"_test = [] {
        CdFixture f;
        auto config = f.registry.get(u"amscd");
        const HandlerConfig expected{{HandlerKind::character, {u"amscd_special"}},
                                     {HandlerKind::delimiter, {}},
                                     {HandlerKind::macro, {u"amscd_macros"}},
                                     {HandlerKind::environment, {u"amscd_environment"}}};
        expect(config->handler.size() == std::size(expected));
        for (auto const &handler : expected) {
            auto actual = std::ranges::find(config->handler, handler.kind, &HandlerMaps::kind);
            expect(actual != config->handler.end());
            if (actual != config->handler.end())
                expect(std::ranges::equal(actual->maps, handler.maps));
        }
        expect(config->items.empty());
        expect(config->options.amscd->harrowsize.value() == u"2.75em");
        expect(config->options.amscd->varrowsize.value() == u"1.75em");
        expect(!config->options.amscd->hideHorizontalLabels.value());
        for (auto name : {u"amscd_special", u"amscd_macros", u"amscd_environment"})
            expect(f.maps.getMap(name) != nullptr);
        auto tables = cdNodes(f.ok(u"\\begin{CD}A@>>>B\\end{CD}"), NodeKind::mtable);
        expect(tables.size() == 1_u);
        if (tables.size() == 1) {
            expect(attr(*tables[0], u"columnspacing") == u"5pt");
            expect(attr(*tables[0], u"rowspacing") == u"5pt");
            auto display = tables[0]->attributes().lookup(u"displaystyle");
            expect(display && std::holds_alternative<bool>(*display) && std::get<bool>(*display));
        }
    };
    "amscd original row parity fills missing cells and puts struts only in even first cells"_test =
        [] {
            struct Case {
                std::u16string_view source;
                std::string_view rows;
                std::size_t struts;
            };
            const Case cases[]{
                {u"\\begin{CD}A @>f>> B \\\\ @VgVV @AAhA \\\\ C @<<k< D\\end{CD}", "3/4/3/", 2},
                {u"\\begin{CD}@>>>@<<<\\\\@VVV@AAA\\\\@.\\end{CD}", "5/4/3/", 2},
                {u"\\begin{CD}@| @= @.\\end{CD}", "7/", 1},
                {u"\\begin{CD}@>>>\\end{CD}", "3/", 1},
                {u"\\begin{CD}@VaVbV @AaAbA\\end{CD}", "5/", 1}};
            CdFixture f;
            for (auto const &item : cases) {
                auto &tree = f.ok(std::u16string(item.source));
                expect(cdRows(tree) == item.rows);
                expect(cdStruts(tree) == item.struts);
            }
            auto &tree = f.ok(u"\\begin{CD}@| @= @.\\end{CD}");
            auto arrows = cdNodes(tree, NodeKind::mo);
            expect(arrows.size() == 2_u);
            if (arrows.size() == 2) {
                expect(NodeUtil::getText(*arrows[0]) == u"\u2225");
                expect(NodeUtil::getText(*arrows[1]) == u"=");
                expect(attr(*arrows[0], u"minsize") == u"1.75em");
                expect(attr(*arrows[1], u"minsize") == u"2.75em");
            }
        };
    "amscd horizontal labels retain upper lower padding and empty label kern"_test = [] {
        CdFixture f;
        struct Labels {
            std::u16string_view a, b;
        };
        for (auto direction : {u'>', u'<'}) {
            for (auto const &labels :
                 {Labels{u"", u""}, Labels{u"a", u""}, Labels{u"", u"b"}, Labels{u"a", u"b"}}) {
                const auto mark = std::u16string(1, direction);
                auto &tree = f.ok(u"\\begin{CD}A@" + mark + std::u16string(labels.a) + mark +
                                  std::u16string(labels.b) + mark + u"B\\end{CD}");
                const bool lower = !labels.b.empty();
                auto scripts = cdNodes(tree, lower ? NodeKind::munderover : NodeKind::mover);
                expect(scripts.size() == 1_u);
                if (scripts.size() != 1)
                    continue;
                auto children = scripts[0]->children();
                expect(children.size() == (lower ? std::size_t{3} : std::size_t{2}));
                auto &arrow = *children[0];
                expect(NodeUtil::getText(arrow) == (direction == u'>' ? u"\u2192" : u"\u2190"));
                expect(attr(arrow, u"minsize") == u"2.75em");
                auto &above = *children[lower ? 2 : 1];
                expect(above.kind() == NodeKind::mpadded);
                expect(attr(above, u"width") == u"+.67em");
                expect(attr(above, u"lspace") == u".33em");
                expect(attr(above, u"voffset") == u".1em");
                expect(cdText(above) == labels.a);
                auto kerns = cdNodes(above, NodeKind::mspace);
                expect(kerns.size() == (labels.a.empty() ? std::size_t{1} : std::size_t{0}));
                if (!kerns.empty())
                    expect(attr(*kerns[0], u"width") == u"2.75em");
                if (lower) {
                    auto &below = *children[1];
                    expect(attr(below, u"width") == u"+.67em");
                    expect(attr(below, u"lspace") == u".33em");
                    expect(attr(below.attributes(), u"voffset").empty());
                    expect(cdText(below) == labels.b);
                }
            }
        }
    };
    "amscd vertical labels use original raise vcenter llap and rlap layout"_test = [] {
        CdFixture f;
        struct Labels {
            std::u16string_view a, b;
        };
        for (auto direction : {u'V', u'A'}) {
            for (auto const &labels :
                 {Labels{u"", u""}, Labels{u"a", u""}, Labels{u"", u"b"}, Labels{u"a", u"b"}}) {
                auto mark = std::u16string(1, direction);
                auto &tree = f.ok(u"\\begin{CD}@" + mark + std::u16string(labels.a) + mark +
                                  std::u16string(labels.b) + mark + u"\\end{CD}");
                auto arrows = cdNodes(tree, NodeKind::mo);
                expect(arrows.size() == 1_u);
                if (arrows.size() != 1)
                    continue;
                auto &arrow = *arrows[0];
                const auto glyph = direction == u'V' ? u"\u2193" : u"\u2191";
                const auto expectedText =
                    std::u16string(labels.a) + glyph + std::u16string(labels.b);
                expect(std::ranges::equal(cdText(tree), expectedText));
                expect(attr(arrow, u"minsize") == u"1.75em");
                expect(cdNumber(arrow, u"lspace", 0));
                expect(cdNumber(arrow, u"rspace", 0));
                auto pads = cdNodes(tree, NodeKind::mpadded);
                expect(std::ranges::count_if(pads, [](auto n) {
                           return attr(*n, u"voffset") == u"+.125em" &&
                                  attr(*n, u"height") == u"+.125em" &&
                                  attr(*n, u"depth") == u"-.125em";
                       }) == static_cast<std::ptrdiff_t>(!labels.a.empty() + !labels.b.empty()));
                expect(std::ranges::count_if(pads, [](auto n) {
                           return cdNumber(*n, u"width", 0) && attr(*n, u"lspace") == u"-1width";
                       }) == static_cast<std::ptrdiff_t>(!labels.a.empty()));
            }
        }
    };
    "amscd captures dimensions on entry honors scoped overrides and resets each formula"_test = [] {
        AmscdOptions options;
        options.colspace = u"2pt";
        options.rowspace = u"3pt";
        options.harrowsize = u"6em";
        options.varrowsize = u"5em";
        CdFixture f(options);
        auto check = [&](std::u16string source, std::u16string_view width,
                         std::u16string_view height) {
            auto &tree = f.ok(std::move(source));
            auto arrows = cdNodes(tree, NodeKind::mo);
            expect(arrows.size() == 2_u);
            if (arrows.size() == 2) {
                expect(attr(*arrows[0], u"minsize") == width);
                expect(attr(*arrows[1], u"minsize") == height);
            }
            auto tables = cdNodes(tree, NodeKind::mtable);
            expect(tables.size() == 1_u);
            if (tables.size() == 1) {
                expect(attr(*tables[0], u"columnspacing") == u"2pt");
                expect(attr(*tables[0], u"rowspacing") == u"3pt");
            }
        };
        check(u"\\minCDarrowwidth{4em}\\minCDarrowheight{3em}\\begin{CD}"
              u"\\minCDarrowwidth{8em}\\minCDarrowheight{7em}@>>>\\\\@VVV\\end{CD}",
              u"4em", u"3em");
        check(u"\\begin{CD}@>>>\\\\@VVV\\end{CD}", u"6em", u"5em");
        auto &tree = f.ok(u"\\minCDarrowwidth{4em}\\begin{CD}{\\minCDarrowwidth{8em}"
                          u"\\begin{CD}@>>>\\end{CD}}@>>>\\end{CD}");
        auto arrows = cdNodes(tree, NodeKind::mo);
        expect(arrows.size() == 2_u);
        if (arrows.size() == 2) {
            expect(attr(*arrows[0], u"minsize") == u"8em");
            expect(attr(*arrows[1], u"minsize") == u"4em");
        }
    };
    "amscd configured creators receive original arrow attributes and hidden horizontal wrapper"_test =
        [] {
            AmscdOptions options;
            options.hideHorizontalLabels = true;
            CdFixture f(options);
            std::vector<CdRequest> requests;
            struct Restore {
                ~Restore() {
                    cdRequests = nullptr;
                }
            } restore;
            cdRequests = &requests;
            NodeCreators creators;
            creators.token = &observeCdToken;
            f.jax->parseOptions().request().nodeFactory.setCreators(creators);
            auto &tree = f.ok(u"\\begin{CD}A@>a>b>B\\\\@VaVbV\\end{CD}");
            expect(requests.size() == 2_u);
            if (requests.size() == 2) {
                expect(requests[0].text == u"\u2192");
                expect(requests[1].text == u"\u2193");
                expect(attr(requests[0].definition.attributes, u"minsize") == u"2.75em");
                expect(attr(requests[1].definition.attributes, u"minsize") == u"1.75em");
                for (auto const &request : requests) {
                    auto stretchy = request.definition.attributes.own(u"stretchy");
                    expect(stretchy && std::holds_alternative<bool>(*stretchy) &&
                           std::get<bool>(*stretchy));
                }
                auto symmetric = requests[1].definition.attributes.own(u"symmetric");
                expect(symmetric && std::holds_alternative<bool>(*symmetric) &&
                       std::get<bool>(*symmetric));
            }
            auto pads = cdNodes(tree, NodeKind::mpadded);
            expect(std::ranges::count_if(pads, [](auto n) {
                       return attr(*n, u"height") == u".67em" && cdNumber(*n, u"depth", 0);
                   }) == std::ptrdiff_t{1});
        };
    "amscd literal fallback generic arrays and original errors leave following input usable"_test =
        [] {
            CdFixture f;
            expect(cdText(f.ok(u"@ \\,x @\U0001F600 @")) == u"@x@\U0001F600@");
            auto &generic = f.ok(u"\\begin{array}{ccc}a@>u>>b\\end{array}");
            expect(cdRows(generic) == "3/");
            auto arrows = cdNodes(generic, NodeKind::mo);
            expect(arrows.size() == 1_u);
            if (arrows.size() == 1) {
                auto minimum = arrows[0]->attributes().own(u"minsize");
                expect(minimum && std::holds_alternative<AttributeNull>(*minimum));
            }
            const std::pair<std::u16string_view, std::string_view> cases[]{
                {u"@>>>", "Misplaced"},
                {u"\\begin{CD}@>f\\end{CD}", "TokenNotFoundForCommand"},
                {u"\\minCDarrowwidth{no}", "MissingDimOrUnits"},
                {u"\\minCDarrowheight{no}", "MissingDimOrUnits"},
                {u"\\begin{array}{ccc}a@>>>b\\end{array}", "MissingDimOrUnits"}};
            for (auto const &[source, error] : cases) {
                f.compile(std::u16string(source));
                expect(f.error == error) << f.error;
                expect(cdRows(f.ok(u"\\begin{CD}@>>>\\end{CD}")) == "3/");
            }
        };
};
