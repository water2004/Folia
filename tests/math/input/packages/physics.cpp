#include "math/support/tex_fixture.hpp"
#include "math/support/physics_fixture_data.hpp"
import folia.math.input.tex.physics.configuration;
import folia.math.input.tex.physics.mappings;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.parse_methods;
namespace {
using namespace std::literals;
struct PhysicsMethod {
    std::string_view name;
    MacroHandler handler;
};
constexpr std::array physicsMethods{
    PhysicsMethod{"Quantity", &PhysicsMethods::Quantity},
    PhysicsMethod{"Eval", &PhysicsMethods::Eval},
    PhysicsMethod{"Commutator", &PhysicsMethods::Commutator},
    PhysicsMethod{"VectorBold", &PhysicsMethods::VectorBold},
    PhysicsMethod{"StarMacro", &PhysicsMethods::StarMacro},
    PhysicsMethod{"OperatorApplication", &PhysicsMethods::OperatorApplication},
    PhysicsMethod{"VectorOperator", &PhysicsMethods::VectorOperator},
    PhysicsMethod{"Expression", &PhysicsMethods::Expression},
    PhysicsMethod{"Qqtext", &PhysicsMethods::Qqtext},
    PhysicsMethod{"Differential", &PhysicsMethods::Differential},
    PhysicsMethod{"Derivative", &PhysicsMethods::Derivative},
    PhysicsMethod{"Bra", &PhysicsMethods::Bra},
    PhysicsMethod{"Ket", &PhysicsMethods::Ket},
    PhysicsMethod{"BraKet", &PhysicsMethods::BraKet},
    PhysicsMethod{"KetBra", &PhysicsMethods::KetBra},
    PhysicsMethod{"Expectation", &PhysicsMethods::Expectation},
    PhysicsMethod{"MatrixElement", &PhysicsMethods::MatrixElement},
    PhysicsMethod{"MatrixQuantity", &PhysicsMethods::MatrixQuantity},
    PhysicsMethod{"IdentityMatrix", &PhysicsMethods::IdentityMatrix},
    PhysicsMethod{"XMatrix", &PhysicsMethods::XMatrix},
    PhysicsMethod{"PauliMatrix", &PhysicsMethods::PauliMatrix},
    PhysicsMethod{"DiagonalMatrix", &PhysicsMethods::DiagonalMatrix},
    PhysicsMethod{"AutoClose", &PhysicsMethods::AutoClose},
    PhysicsMethod{"Vnabla", &PhysicsMethods::Vnabla},
    PhysicsMethod{"DiffD", &PhysicsMethods::DiffD},
    PhysicsMethod{"Macro", PhysicsMethods::Macro},
    PhysicsMethod{"NamedFn", PhysicsMethods::NamedFn}};
constexpr auto physicsCallbacks = [] {
    std::array<MacroHandler, physicsMethods.size()> callbacks{};
    for (std::size_t i = 0; i < callbacks.size(); ++i)
        callbacks[i] = physicsMethods[i].handler;
    return callbacks;
}();
constexpr std::array<TokenInputHandler, 2> physicsTokens{&ParseMethods::mathchar0mo,
                                                         &ParseMethods::mathchar0mi};
thread_local std::string *physicsError{};
MmlNode &physicsFormatError(TeX &jax, TexError const &error) {
    if (physicsError)
        *physicsError = error.id();
    return jax.formatError(error);
}
struct PhysicsFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{physicsCallbacks, physicsTokens};
    Warnings warnings;
    std::unique_ptr<MmlDocument> document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit PhysicsFixture(bool italic = false, bool arrow = false, bool extra = false) {
        PhysicsConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (extra)
            options.packages->push_back({u"newcommand", {}});
        options.packages->push_back({u"physics", {}});
        PhysicsOptions selected;
        selected.italicdiff = italic;
        selected.arrowdel = arrow;
        options.physics = selected;
        options.maxMacros = 1000;
        options.formatError = &physicsFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{physicsError};
            ~Restore() {
                physicsError = old;
            }
        } restore;
        physicsError = &error;
        request.reset();
        document = std::make_unique<MmlDocument>();
        request = std::make_unique<TeXRequest>(*jax, document->factory());
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        return jax->compile(math, *document);
    }
    MmlNode &ok(std::u16string source) {
        auto &node = compile(std::move(source));
        expect(error.empty()) << error;
        expect(jax->parseOptions().request().parsers.empty());
        return node;
    }
};
std::u16string decimal(std::size_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::u16string number(double n) {
    if (n == 0)
        return u"0";
    std::array<char, 64> buffer;
    auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), n);
    return {buffer.data(), converted.ptr};
}
// Length-prefixed primitive records avoid a JSON dependency and preserve UTF-16 text exactly.
std::u16string physicsSnapshot(MmlNode const &root) {
    std::u16string out;
    auto add = [&](std::u16string_view value) {
        out += decimal(value.size()) + u":" + std::u16string(value);
    };
    auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(u"null");
            return;
        }
        auto name = nodeKindName(node->kind());
        add(std::u16string(name.begin(), name.end()));
        auto entries = node->attributes().ownEntries();
        std::ranges::sort(entries, {}, &AttributeSet::Entry::first);
        add(decimal(entries.size()));
        for (auto const &[key, value] : entries) {
            add(key);
            std::visit(
                [&](auto const &scalar) {
                    using T = std::remove_cvref_t<decltype(scalar)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(u"null");
                        add(u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(u"boolean");
                        add(scalar ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        add(u"number");
                        add(number(scalar));
                    } else {
                        add(u"string");
                        add(scalar);
                    }
                },
                value);
        }
        auto const &props = node->properties().tex;
        for (auto const *value : {&props.open, &props.close}) {
            add(value->has_value() ? u"string" : u"");
            add(value->value_or(u""));
        }
        add(props.explicit_tex_class ? u"number" : u"");
        add(props.explicit_tex_class ? number(static_cast<int>(*props.explicit_tex_class)) : u"");
        add(node->isToken() ? NodeUtil::getText(*node) : u"");
        add(decimal(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return out;
}
void physicsMatchesSource(MmlNode const &root, std::u16string_view expected) {
    auto actual = physicsSnapshot(root);
    auto mismatch = std::ranges::mismatch(actual, expected);
    expect(std::ranges::equal(actual, expected))
        << "Physics Source offset " << (mismatch.in1 - actual.begin()) << " lengths "
        << actual.size() << " " << expected.size();
}

void physicsParents(MmlNode &root) {
    root.walkTree([&](auto &node) {
        for (auto *child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
using PhysicsData = math_test::fixtures::PhysicsFixtureData<TexArgument, AttributeSet::Entry>;
using PhysicsTree = PhysicsData::PhysicsTree;
using PhysicsError = PhysicsData::PhysicsError;
using PhysicsRuntime = PhysicsData::PhysicsRuntime;
using PhysicsStep = PhysicsData::PhysicsStep;
using PhysicsDirect = PhysicsData::PhysicsDirect;
using PhysicsBinding = PhysicsData::PhysicsBinding;

MmlNode &runPhysicsDirect(PhysicsDirect const &entry, AutoOpen &item, TexParser &parser,
                          StackItemFactory &factory) {
    auto token = [&](std::u16string_view text, NodeKind kind = NodeKind::mi) -> MmlNode & {
        return parser.createToken(kind, text);
    };
    auto push = [&](MmlNode &node) {
        const std::array nodes{&node};
        item.Push(nodes);
    };
    if (entry.name == u"default"sv || entry.name == u"force-ignored"sv) {
        push(token(u"x"));
        auto &node = item.toMml(true, entry.name == u"force-ignored"sv);
        expect(!node.properties().tex.open);
        expect(!node.properties().tex.close);
        expect(!node.properties().tex.explicit_tex_class);
        return node;
    }
    if (entry.name == u"false-force"sv) {
        auto &node = token(u"x");
        node.properties().tex.open = u"o";
        node.properties().tex.close = u"c";
        node.properties().tex.explicit_tex_class = TexClass::bin;
        push(node);
        auto &out = item.toMml(false, true);
        expect(out.children().front() == &node);
        expect(std::ranges::equal(node.properties().tex.open.value(), u"o"sv));
        return out;
    }
    if (entry.name == u"smash-right"sv) {
        item.smash = true;
        item.right = u"y";
        push(token(u"x"));
        return item.toMml();
    }
    if (entry.name == u"pre-close"sv) {
        push(token(u"x"));
        auto close = factory.create(StackKind::close);
        dynamic_cast<CloseItem &>(*close).pre_autoclose = true;
        expect(item.checkItem(close).action == CheckAction::drop);
        expect(item.Size() == 1_u);
        return item.toMml(false);
    }
    if (entry.name == u"autoclose"sv || entry.name == u"ignore"sv) {
        item.ignore = entry.name == u"ignore"sv;
        push(token(u"x"));
        auto &closer = token(u")");
        auto frame = factory.create(StackKind::mml, MmlItemInit{closer});
        dynamic_cast<MmlItem &>(*frame).autoclose = true;
        auto result = item.checkItem(frame);
        expect(result.action == CheckAction::replay);
        if (item.ignore) {
            expect(result.replacements.empty());
            expect(item.Size() == 0_u);
            return item.toMml(false);
        }
        expect(result.replacements.size() == 1_u);
        return replayNode(result);
    }
    if (entry.name == u"nested-count"sv) {
        auto &open = token(u"(", NodeKind::mo);
        auto frame = factory.create(StackKind::mml, MmlItemInit{open});
        expect(item.checkItem(frame).action == CheckAction::drop);
        expect(item.openCount == 1.0);
        expect(!item.closing(u"]"));
        expect(item.openCount == 1.0);
        expect(!item.closing(u")"));
        expect(item.openCount == 0.0);
        expect(item.closing(u")"));
        expect(item.openCount == -1.0);
        expect(!item.closing(u")"));
        expect(item.openCount == -2.0);
        return item.toMml(false);
    }
    if (entry.name == u"stop"sv) {
        texError([&] { item.checkItem(factory.create(StackKind::stop)); }, "ExtraOrMissingDelims");
        return item.toMml(false);
    }
    throw std::logic_error("Unknown Source AutoOpen case");
}
struct PhysicsRequest {
    NodeKind kind;
    NodeDefinition definition;
    MmlNode *node;
};
thread_local std::vector<PhysicsRequest> *physicsRequests{};
thread_local bool physicsThrow{};
MmlNode &physicsNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                     NodeDefinition definition, MmlNode *text) {
    if (physicsThrow && kind == NodeKind::mi && text && NodeUtil::getText(*text) == u"q")
        throw std::runtime_error("physics creator probe");
    auto saved = definition;
    auto &node = NodeFactory::createNode(factory, kind, children, std::move(definition), text);
    if (physicsRequests)
        physicsRequests->push_back({kind, std::move(saved), &node});
    return node;
}
MmlNode &physicsToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                      std::optional<std::u16string_view> text) {
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
struct CountedAutoOpen : AutoOpen {
    int &live;
    CountedAutoOpen(StackItemFactory &factory, int &count) : AutoOpen(factory), live(count) {
        ++live;
    }
    ~CountedAutoOpen() override {
        --live;
    }
};
} // namespace
suite physics_package = [] {
    "physics original eleven maps bind every Source entry argument and character"_test = [] {
        PhysicsFixture f;
        auto config = f.registry.get(u"physics");
        expect(config->priority == 5.0);
        expect(config->handler.size() == 4_u);
        expect(!config->config());
        expect(!config->init());
        expect(config->items.size() == 1_u);
        expect(config->options.physics.has_value());
        expect(config->options.physics->italicdiff == false);
        expect(config->options.physics->arrowdel == false);
        std::vector<std::u16string> seen;
        for (auto const &binding : PhysicsData::physicsBindings()) {
            const std::string label(binding.token.begin(), binding.token.end());
            if (std::ranges::find(seen, binding.map) == seen.end())
                seen.emplace_back(binding.map);
            auto map = f.maps.getMap(binding.map);
            expect(map != nullptr);
            if (!map)
                continue;
            if (binding.character) {
                auto token = std::get<Token const *>(map->lookup(binding.token));
                expect(token != nullptr);
                if (!token)
                    continue;
                const bool same = token->character == binding.character;
                expect(same) << label << "character";
                const auto parser = binding.method == "mathchar0mo" ? &ParseMethods::mathchar0mo
                                                                    : &ParseMethods::mathchar0mi;
                expect(std::get<TokenInputHandler>(*map->parser()) == parser);
                if (binding.attributes.empty())
                    expect(!token->attributes || token->attributes->ownEntries().empty());
                else {
                    expect(token->attributes.has_value());
                    if (token->attributes) {
                        const bool attrs = token->attributes->ownEntries() == binding.attributes;
                        expect(attrs) << label << "attributes";
                    }
                }
            } else {
                auto macro = std::get<Macro const *>(map->lookup(binding.token));
                expect(macro != nullptr);
                if (!macro)
                    continue;
                if (binding.method == "Array") {
                    expect(std::get<EnvironmentBodyHandler>(macro->function) ==
                           PhysicsMethods::Array);
                    expect(std::get<EnvironmentHandler>(*map->parser()) ==
                           &ParseMethods::environment);
                } else {
                    auto method =
                        std::ranges::find(physicsMethods, binding.method, &PhysicsMethod::name);
                    expect(method != physicsMethods.end());
                    if (method != physicsMethods.end())
                        expect(std::get<MacroHandler>(macro->function) == method->handler)
                            << label << "callback";
                }
                const bool args = macro->arguments == binding.arguments;
                expect(args) << label << "arguments";
            }
        }
        expect(seen.size() == 11_u);
        expect(PhysicsData::physicsBindings().size() == 196_u);
        auto frame = f.jax->parseOptions().itemFactory->create(StackKind::auto_open);
        expect(dynamic_cast<AutoOpen *>(frame.get()) != nullptr);
        expect(frame->isOpen());
    };
    "physics all mathematical families and both options match real Source physical trees"_test =
        [] {
            for (auto const &entry : PhysicsData::physicsTrees()) {
                PhysicsFixture f(entry.italic, entry.arrow);
                auto &root = f.ok(std::u16string(entry.source));
                physicsMatchesSource(root, entry.snapshot);
                physicsParents(root);
                expect(&root.document() == f.document.get());
            }
        };
    "physics Source errors and finite matrix runtime boundary recover the next formula"_test = [] {
        for (auto const &entry : PhysicsData::physicsErrors()) {
            PhysicsFixture f(entry.italic, entry.arrow);
            f.compile(std::u16string(entry.source));
            expect(f.error == entry.id) << entry.id;
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            f.ok(u"\\qty(x)");
        }
        for (auto const &entry : PhysicsData::physicsRuntime()) {
            PhysicsFixture f;
            expect(entry.kind == "RangeError");
            expect(throws<std::length_error>([&] { f.compile(std::u16string(entry.source)); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            f.ok(u"\\qty(x)");
        }
    };
    "physics captured bra ket aliases and vector failures retain definition sequences"_test = [] {
        std::unique_ptr<PhysicsFixture> fixture;
        int group = -1;
        for (auto const &entry : PhysicsData::physicsSteps()) {
            if (group != entry.group) {
                fixture = std::make_unique<PhysicsFixture>(false, false, true);
                group = entry.group;
            }
            expect(entry.runtime.empty());
            auto &root = fixture->compile(std::u16string(entry.source));
            expect(fixture->error == entry.error);
            expect(fixture->jax->parseOptions().request().parsers.empty());
            physicsMatchesSource(root, entry.snapshot);
        }
    };
    "physics AutoOpen direct output flags markers and signed closing match Source"_test = [] {
        for (auto const &entry : PhysicsData::physicsDirect()) {
            PhysicsFixture f;
            f.ok(u"x");
            auto &options = f.jax->parseOptions();
            TexParser parser(u"", ParserEnvironmentSeed{}, options);
            auto frame = parser.itemFactory().create(StackKind::auto_open);
            auto &item = dynamic_cast<AutoOpen &>(*frame);
            item.open = u"(";
            item.close = u")";
            auto &node = runPhysicsDirect(entry, item, parser, parser.itemFactory());
            physicsMatchesSource(node, entry.snapshot);
            physicsParents(node);
            parser.mml();
            expect(options.request().parsers.empty());
        }
    };
    "physics configured creators see original fences before marker stripping"_test = [] {
        PhysicsFixture f;
        std::vector<PhysicsRequest> requests;
        struct Restore {
            std::vector<PhysicsRequest> *old{physicsRequests};
            ~Restore() {
                physicsRequests = old;
            }
        } restore;
        physicsRequests = &requests;
        f.jax->parseOptions().setNodeCreators(NodeCreators{.node = &physicsNode});
        auto &root = f.ok(u"\\qty(x)");
        bool found = false;
        for (auto const &request : requests)
            if (request.kind == NodeKind::mrow && request.definition.properties.tex.open == u"(") {
                found = true;
                expect(request.definition.properties.tex.close == u")");
                expect(request.definition.properties.tex.explicit_tex_class == TexClass::inner);
                expect(!request.node->properties().tex.open);
                expect(!request.node->properties().tex.close);
                expect(!request.node->properties().tex.explicit_tex_class);
            }
        expect(found);
        physicsParents(root);
    };
    "physics vector creator and font restore on success nested calls and thrown creator"_test = [] {
        PhysicsFixture f;
        f.ok(u"x");
        auto &options = f.jax->parseOptions();
        {
            auto &factory = options.request().nodeFactory;
            const auto creators = factory.creators();
            struct Restore {
                NodeFactory &factory;
                NodeCreators creators;
                bool fail{physicsThrow};
                ~Restore() {
                    factory.setCreators(creators);
                    physicsThrow = fail;
                }
            } restore{factory, creators};
            factory.setCreators(NodeCreators{.node = &physicsNode, .token = &physicsToken});
            TexEnvironment env;
            env.font = u"normal";
            TexParser parser(u"", ParserEnvironmentSeed{env}, options);
            auto invoke = [&](std::u16string source) {
                parser.setString(std::move(source));
                parser.setPosition(0);
                const std::span<TexArgument const> args;
                return PhysicsMethods::VectorBold(parser, MacroInvocation{u"\\vb", u"\\vb", args});
            };
            invoke(u"{a\\vb*{b}c}");
            expect(factory.creators().token.value() == &physicsToken);
            expect(parser.stack.env().font == u"normal");
            expect(!parser.stack.env().vector_font);
            expect(!parser.stack.env().vector_star);
            physicsThrow = true;
            expect(throws<std::runtime_error>([&] { invoke(u"{q}"); }));
            expect(factory.creators().token.value() == &physicsToken);
            expect(parser.stack.env().font == u"normal");
            expect(!parser.stack.env().vector_font);
            expect(!parser.stack.env().vector_star);
            physicsThrow = false;
            invoke(u"{z}");
            parser.mml();
            expect(options.request().parsers.empty());
        }
        f.ok(u"x");
        f.ok(u"\\vb{a}b");
    };
    "physics AutoOpen actual stack replay owns frames for repeated mixed scopes"_test = [] {
        PhysicsFixture f;
        f.ok(u"x");
        auto &options = f.jax->parseOptions();
        auto &factory = *options.itemFactory;
        int live = 0;
        factory.setNodeClass(StackKind::auto_open,
                             {[&](StackBuildArguments const &a) {
                                  return std::make_unique<CountedAutoOpen>(a.factory, live);
                              },
                              [](BaseItem const &a) {
                                  return dynamic_cast<CountedAutoOpen const *>(&a) != nullptr;
                              }});
        for (int i = 0; i < 32; ++i) {
            std::weak_ptr<BaseItem> weak;
            {
                TexParser parser(u"", ParserEnvironmentSeed{}, options);
                auto frame = factory.create(StackKind::auto_open);
                weak = frame;
                auto &item = dynamic_cast<AutoOpen &>(*frame);
                item.open = u"(";
                item.close = u")";
                Stack stack(factory, {}, false);
                stack.Push(frame);
                frame.reset();
                expect(!weak.expired());
                expect(live == 1);
                auto &node = options.request().nodeFactory.makeToken(NodeKind::mi, u"x");
                stack.Push(&node);
                if (i % 2 == 0) {
                    auto close = factory.create(StackKind::close);
                    dynamic_cast<CloseItem &>(*close).pre_autoclose = true;
                    stack.Push(close);
                    expect(live == 1);
                    auto closer = factory.create(StackKind::mml, MmlItemInit{node});
                    dynamic_cast<MmlItem &>(*closer).autoclose = true;
                    stack.Push(closer);
                    expect(weak.expired());
                    expect(live == 0);
                    stack.Push(factory.create(StackKind::stop));
                    expect(stack.Top()->Size() == 1_u);
                } else
                    texError([&] { stack.Push(factory.create(StackKind::stop)); },
                             "ExtraOrMissingDelims");
                parser.mml();
            }
            expect(weak.expired());
            expect(live == 0);
            expect(options.request().parsers.empty());
        }
        factory.setNodeClass(
            StackKind::auto_open,
            {[](StackBuildArguments const &a) { return std::make_unique<AutoOpen>(a.factory); },
             [](BaseItem const &a) { return dynamic_cast<AutoOpen const *>(&a) != nullptr; }});
    };
};
