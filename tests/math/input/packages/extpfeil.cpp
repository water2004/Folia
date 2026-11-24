#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.extpfeil.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.begingroup.configuration;

namespace {
constexpr std::array<MacroHandler, 2> arrowCallbacks{&AmsMethods::xArrow,
                                                     &ExtpfeilMethods::NewExtArrow};
thread_local std::string *arrowError{};
thread_local std::u16string *arrowMessage{};
MmlNode &formatArrowError(TeX &jax, TexError const &error) {
    if (arrowError)
        *arrowError = error.id();
    if (arrowMessage)
        *arrowMessage = error.message();
    return jax.formatError(error);
}
struct ArrowRequest {
    std::u16string text;
    bool stretchy;
    std::optional<TexClass> texClass;
};
thread_local std::vector<ArrowRequest> *arrowRequests{};
MmlNode &observeArrow(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                      std::optional<std::u16string_view> text) {
    if (arrowRequests && kind == NodeKind::mo) {
        auto stretchy = definition.attributes.own(u"stretchy");
        arrowRequests->push_back(
            {text ? std::u16string(*text) : std::u16string{},
             stretchy && std::holds_alternative<bool>(*stretchy) && std::get<bool>(*stretchy),
             definition.properties.tex.explicit_tex_class});
    }
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
struct ArrowFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{arrowCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit ArrowFixture(bool groups = false) {
        ExtpfeilConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (groups) {
            BegingroupConfiguration::initializeModule(registry, maps);
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"begingroup", {}});
            options.packages->push_back({u"newcommand", {}});
        }
        options.packages->push_back({u"extpfeil", {}});
        options.formatError = &formatArrowError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *oldError;
            std::u16string *oldMessage;
            ~Restore() {
                arrowError = oldError;
                arrowMessage = oldMessage;
            }
        } restore{arrowError, arrowMessage};
        arrowError = &error;
        arrowMessage = &message;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty());
        return root;
    }
};
std::vector<MmlNode *> arrowNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
bool hasArrow(MmlNode &root, std::u16string_view glyph) {
    return std::ranges::any_of(arrowNodes(root, NodeKind::mo),
                               [&](auto node) { return NodeUtil::getText(*node) == glyph; });
}
} // namespace

suite extpfeil = [] {
    "extpfeil five original arrow maps keep code widths labels and base only package dependency"_test =
        [] {
            ArrowFixture f;
            expect(f.registry.get(u"ams") == nullptr);
            expect(f.registry.get(u"newcommand") == nullptr);
            expect(ExtpfeilMethods::xArrow == &AmsMethods::xArrow);
            struct Entry {
                std::u16string_view name, glyph;
                double code, left, right;
                std::u16string_view width, lspace;
            };
            for (auto const &entry :
                 {Entry{u"xtwoheadrightarrow", u"↠", 8608, 12, 16, u"+1.556em", u"0.667em"},
                  Entry{u"xtwoheadleftarrow", u"↞", 8606, 17, 13, u"+1.667em", u"0.944em"},
                  Entry{u"xmapsto", u"↦", 8614, 6, 7, u"+0.722em", u"0.333em"},
                  Entry{u"xlongequal", u"=", 61, 7, 7, u"+0.778em", u"0.389em"},
                  Entry{u"xtofrom", u"⇄", 8644, 12, 12, u"+1.333em", u"0.667em"}}) {
                auto value = f.maps.getMap(u"extpfeil")->lookup(entry.name);
                auto macro = std::get<Macro const *>(value);
                expect(std::get<MacroHandler>(macro->function) == &AmsMethods::xArrow);
                expect(std::get<double>(macro->arguments[0]) == entry.code);
                expect(std::get<double>(macro->arguments[1]) == entry.left);
                expect(std::get<double>(macro->arguments[2]) == entry.right);
                for (auto suffix : {u"{a}", u"[b]{a}", u"[]{}"}) {
                    auto &root = f.ok(u"\\" + std::u16string(entry.name) + suffix);
                    expect(hasArrow(root, entry.glyph));
                    auto padded = arrowNodes(root, NodeKind::mpadded);
                    const std::size_t count = std::u16string_view(suffix) == u"[b]{a}" ? 2 : 1;
                    expect(padded.size() == count);
                    for (auto pad : padded) {
                        expect(attr(*pad, u"width") == entry.width);
                        expect(attr(*pad, u"lspace") == entry.lspace);
                    }
                }
            }
            expect(f.maps.getMap(u"extpfeil")->contains(u"Newextarrow"));
        };
    "extpfeil custom decimal hex and single character commands persist across formulas and reset"_test =
        [] {
            ArrowFixture f;
            f.ok(u"\\Newextarrow{\\custom}{5,10}{8594}");
            expect(hasArrow(f.ok(u"\\custom[b]{a}"), u"→"));
            auto definition = f.jax->session().findCommand(u"custom");
            expect(definition != nullptr);
            if (definition) {
                auto alias = std::get_if<NativeMacroAlias>(&definition->body);
                expect(alias != nullptr);
                if (alias) {
                    expect(f.callbacks.macroHandler(alias->handler) == &AmsMethods::xArrow);
                    expect(alias->arguments.size() == 3_u);
                }
            }
            f.jax->reset(9);
            expect(hasArrow(f.ok(u"\\custom{a}"), u"→"));
            f.ok(u"\\Newextarrow{\\custom}{6,7}{0X21A6}");
            expect(hasArrow(f.ok(u"\\custom{a}"), u"↦"));
            f.ok(u"\\Newextarrow{\\!}{01,002}{0x2190}");
            expect(hasArrow(f.ok(u"\\!{a}"), u"←"));
            f.ok(u"\\Newextarrow{\\α}{1,2}{8594}");
            expect(hasArrow(f.ok(u"\\α{a}"), u"→"));
        };
    "extpfeil formal groups global definitions and let retain compiled callback snapshots"_test =
        [] {
            ArrowFixture f(true);
            f.ok(u"\\Newextarrow{\\custom}{1,2}{0x2192}");
            f.ok(u"\\begingroup\\Newextarrow{\\custom}{3,4}{0x2190}");
            expect(hasArrow(f.ok(u"\\custom{a}"), u"←"));
            expect(hasArrow(f.ok(u"\\endgroup\\custom{a}"), u"→"));
            f.ok(u"\\begingroup\\global\\Newextarrow{\\custom}{1,2}{0x21A6}\\endgroup");
            expect(hasArrow(f.ok(u"\\custom{a}"), u"↦"));
            f.ok(u"\\let\\saved\\custom\\Newextarrow{\\custom}{1,2}{0x2190}");
            expect(hasArrow(f.ok(u"\\saved{a}"), u"↦"));
            expect(hasArrow(f.ok(u"\\custom{a}"), u"←"));
            f.compile(u"\\let\\define\\Newextarrow\\define{bad}{bad}{bad}");
            expect(f.error == "NewextarrowArg1");
            expect(f.message == u"First argument to \\Newextarrow must be a control sequence name");
        };
    "extpfeil configured token creator receives original arrow attributes and code point"_test =
        [] {
            ArrowFixture f;
            std::vector<ArrowRequest> requests;
            arrowRequests = &requests;
            struct Restore {
                ~Restore() {
                    arrowRequests = nullptr;
                }
            } restore;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.token = &observeArrow});
            f.ok(u"\\Newextarrow{\\custom}{5,10}{8594}\\custom{a}");
            expect(requests.size() == 1_u);
            if (requests.size() == 1) {
                expect(requests[0].text == u"→");
                expect(requests[0].stretchy);
                expect(requests[0].texClass == TexClass::ord);
            }
        };
    "extpfeil reads required arguments before ordered validation and recovers next formula"_test =
        [] {
            struct Example {
                std::u16string_view source;
                std::string_view id;
            };
            for (auto const &example :
                 {Example{u"\\Newextarrow{bad}{bad}{bad}", "NewextarrowArg1"},
                  Example{u"\\Newextarrow{\\foo}{bad}{bad}", "NewextarrowArg2"},
                  Example{u"\\Newextarrow{\\foo}{1,2}{bad}", "NewextarrowArg3"},
                  Example{u"\\Newextarrow{bad}", "MissingArgFor"},
                  Example{u"\\Newextarrow{bad}{bad}", "MissingArgFor"},
                  Example{u"\\Newextarrow{\\foo}{1, 2}{8594}", "NewextarrowArg2"},
                  Example{u"\\Newextarrow{\\foo}{-1,2}{8594}", "NewextarrowArg2"},
                  Example{u"\\Newextarrow{\\foo}{1,2}{0xZZ}", "NewextarrowArg3"},
                  Example{u"\\Newextarrow{\\foo}{1,2}{-1}", "NewextarrowArg3"},
                  Example{u"\\Newextarrow{\\ab1}{1,2}{8594}", "NewextarrowArg1"},
                  Example{u"\\Newextarrow{\\😀}{1,2}{8594}", "NewextarrowArg1"}}) {
                ArrowFixture f;
                f.compile(std::u16string(example.source));
                expect(f.error == example.id);
                f.ok(u"a+b");
            }
        };
    "extpfeil invalid code point is caught on focused invocation and parser borrowing recovers"_test =
        [] {
            ArrowFixture f;
            f.ok(u"\\Newextarrow{\\invalid}{1,2}{1114112}");
            expect(throws<std::out_of_range>([&] { f.compile(u"\\invalid{a}"); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            expect(hasArrow(f.ok(u"\\xmapsto{a}"), u"↦"));
        };
};
