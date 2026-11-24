#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.base_configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.unicode.configuration;
import folia.math.util.js_number;
import folia.math.util.utf16;

namespace {
using namespace std::literals;
constexpr std::array<MacroHandler, 3> unicodeCallbacks{
    &UnicodeMethods::Unicode, &UnicodeMethods::RawUnicode, &UnicodeMethods::Char};
struct UnicodeCase {
    std::u16string_view name;
    std::u16string_view source;
    std::u16string_view snapshot;
    std::u16string_view error;
    std::u16string_view message;
};
struct UnicodeRegistryCase {
    std::u16string_view source;
    std::u16string_view snapshot;
    std::u16string_view error;
};
struct UnicodeRecoveryCase {
    int group;
    std::u16string_view source;
    std::u16string_view snapshot;
    std::u16string_view error;
    std::u16string_view message;
};
#include "math/fixtures/unicode.inc"

struct UnicodeObservedCreator {
    NodeKind kind;
    NodeDefinition definition;
    std::u16string text;
};
thread_local std::vector<UnicodeObservedCreator> *unicodeCapturedRequests{};
MmlNode &observeUnicodeToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                             std::optional<std::u16string_view> text) {
    auto saved = definition;
    auto &node = NodeFactory::createToken(factory, kind, std::move(definition), text);
    if (unicodeCapturedRequests && kind == NodeKind::mtext)
        unicodeCapturedRequests->push_back(
            {kind, std::move(saved), text ? std::u16string(*text) : std::u16string{}});
    return node;
}

struct UnicodeFailure {
    std::u16string id;
    std::u16string message;
};
thread_local UnicodeFailure *unicodeError{};
MmlNode &formatUnicodeError(TeX &jax, TexError const &error) {
    if (unicodeError)
        unicodeError->id.assign(error.id().begin(), error.id().end());
    if (unicodeError)
        unicodeError->message = error.message();
    return jax.formatError(error);
}

struct UnicodeFixture {
    std::unique_ptr<ConfigurationHandler> ownedRegistry;
    std::unique_ptr<MapHandler> ownedMaps;
    ConfigurationHandler *registry{};
    MapHandler *maps{};
    MacroCallbacks callbacks{unicodeCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;

    UnicodeFixture() {
        ownedRegistry = std::make_unique<ConfigurationHandler>();
        ownedMaps = std::make_unique<MapHandler>();
        registry = ownedRegistry.get();
        maps = ownedMaps.get();
        initialize();
    }
    UnicodeFixture(ConfigurationHandler &sharedRegistry, MapHandler &sharedMaps)
        : registry(&sharedRegistry), maps(&sharedMaps) {
        initialize();
    }

    void initialize() {
        BasePackage::initialize(*registry, *maps);
        NewcommandConfiguration::initializeModule(*registry, *maps);
        UnicodeConfiguration::initializeModule(*registry, *maps);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"newcommand", {}}, {u"unicode", {}}};
        options.maxMacros = 1000;
        options.formatError = &formatUnicodeError;
        jax = std::make_unique<TeX>(*registry, *maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }

    MmlNode &compile(std::u16string_view source, UnicodeFailure &failure) {
        failure = {};
        struct Restore {
            UnicodeFailure *previous{unicodeError};
            ~Restore() {
                unicodeError = previous;
            }
        } restore;
        unicodeError = &failure;
        folia::math::core::AbstractMathItem math(std::u16string(source), jax.get());
        return jax->compile(math, document);
    }
};

std::u16string decimal(std::size_t value) {
    const auto text = std::to_string(value);
    return {text.begin(), text.end()};
}
std::u16string number(double value) {
    const auto narrow = folia::math::js_number::toString(value);
    return {narrow.begin(), narrow.end()};
}
std::u16string snapshot(MmlNode const &root) {
    std::u16string output;
    auto add = [&](std::u16string_view value) {
        output += decimal(value.size());
        output += u":";
        output += value;
    };
    const auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(u"null");
            return;
        }
        const auto kind = nodeKindName(node->kind());
        add(std::u16string(kind.begin(), kind.end()));
        auto entries = node->attributes().ownEntries();
        std::ranges::sort(entries, {}, &AttributeSet::Entry::first);
        add(decimal(entries.size()));
        for (const auto &[key, value] : entries) {
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
        const auto unicode = node->properties().tex.unicode;
        add(unicode ? u"boolean" : u"");
        add(unicode ? (*unicode ? u"true" : u"false") : u"");
        add(node->isToken() ? NodeUtil::getText(*node) : u"");
        add(decimal(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (const auto *child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return output;
}

} // namespace

suite unicode_package = [] {
    "Unicode commands match pinned Source MML and errors"_test = [] {
        for (const auto &entry : unicodeCases) {
            UnicodeFixture fixture;
            UnicodeFailure failure;
            MmlNode *tree{};
            try {
                tree = &fixture.compile(entry.source, failure);
            } catch (std::range_error const &exception) {
                failure.id = u"RangeError";
                const auto message = std::string_view(exception.what());
                failure.message.assign(message.begin(), message.end());
            }
            expect(failure.id == entry.error) << folia::math::utf16::encode(entry.name);
            expect(failure.message == entry.message) << folia::math::utf16::encode(entry.name);
            if (!entry.snapshot.empty())
                expect(tree && snapshot(*tree) == entry.snapshot)
                    << folia::math::utf16::encode(entry.name);
            else
                expect(tree == nullptr) << folia::math::utf16::encode(entry.name);
            expect(fixture.jax->parseOptions().request().parsers.empty())
                << folia::math::utf16::encode(entry.name);
        }
    };

    "Unicode font cache follows registry and module lifetimes"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        UnicodeFailure failure;
        UnicodeFixture first(registry, maps);
        auto &set = first.compile(unicodeRegistryCases[0].source, failure);
        expect(failure.id == unicodeRegistryCases[0].error);
        expect(snapshot(set) == unicodeRegistryCases[0].snapshot);
        first.jax->reset();
        auto &read = first.compile(unicodeRegistryCases[1].source, failure);
        expect(failure.id == unicodeRegistryCases[1].error);
        expect(snapshot(read) == unicodeRegistryCases[1].snapshot);
        auto &replace = first.compile(unicodeRegistryCases[2].source, failure);
        expect(failure.id == unicodeRegistryCases[2].error);
        expect(snapshot(replace) == unicodeRegistryCases[2].snapshot);
        auto &readReplacement = first.compile(unicodeRegistryCases[3].source, failure);
        expect(failure.id == unicodeRegistryCases[3].error);
        expect(snapshot(readReplacement) == unicodeRegistryCases[3].snapshot);
        UnicodeFixture second(registry, maps);
        auto &shared = second.compile(unicodeRegistryCases[4].source, failure);
        expect(failure.id == unicodeRegistryCases[4].error);
        expect(snapshot(shared) == unicodeRegistryCases[4].snapshot);

        ConfigurationHandler independentRegistry;
        MapHandler independentMaps;
        UnicodeFixture independent(independentRegistry, independentMaps);
        auto &isolated = independent.compile(unicodeRegistryCases[5].source, failure);
        expect(failure.id == unicodeRegistryCases[5].error);
        expect(snapshot(isolated) == unicodeRegistryCases[5].snapshot);
    };

    "Unicode configured token creator receives its original request"_test = [] {
        UnicodeFixture fixture;
        const auto map = fixture.maps->getMap(u"unicode");
        expect(map != nullptr);
        if (!map)
            return;
        for (const auto &[name, callback] :
             std::array{std::pair{std::u16string_view(u"unicode"), &UnicodeMethods::Unicode},
                        std::pair{std::u16string_view(u"U"), &UnicodeMethods::RawUnicode},
                        std::pair{std::u16string_view(u"char"), &UnicodeMethods::Char}}) {
            const auto entry = map->lookup(name);
            const auto *macro = std::get_if<Macro const *>(&entry);
            const auto *handler =
                macro && *macro ? std::get_if<MacroHandler>(&(*macro)->function) : nullptr;
            expect(handler && *handler == callback) << folia::math::utf16::encode(name);
        }
        std::vector<UnicodeObservedCreator> requests;
        struct Restore {
            ~Restore() {
                unicodeCapturedRequests = nullptr;
            }
        } restore;
        unicodeCapturedRequests = &requests;
        fixture.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.token = &observeUnicodeToken});
        UnicodeFailure failure;
        auto &tree = fixture.compile(unicodeCreatorSource, failure);
        expect(failure.id.empty());
        expect(requests.size() == std::size(unicodeCreatorRequests));
        expect(snapshot(tree) == unicodeCreatorSnapshot);
        if (requests.size() == 1) {
            expect(requests[0].kind == NodeKind::mtext);
            expect(requests[0].text == unicodeCreatorRequests[0].text);
            expect(attr(requests[0].definition.attributes, u"fontfamily") ==
                   unicodeCreatorRequests[0].fontfamily);
            auto tokens = std::vector<MmlNode *>{};
            tree.walkTree([&](auto &node) {
                if (node.kind() == NodeKind::mtext)
                    tokens.push_back(&node);
            });
            expect(tokens.size() == 1_u);
            if (tokens.size() == 1)
                expect(tokens.front()->properties().tex.unicode == true);
        }
    };

    "Unicode errors recover in the same configured TeX instance"_test = [] {
        for (int group = 0; group < 3; ++group) {
            UnicodeFixture fixture;
            for (const auto &entry : unicodeRecoveryCases) {
                if (entry.group != group)
                    continue;
                UnicodeFailure failure;
                MmlNode *tree{};
                try {
                    tree = &fixture.compile(entry.source, failure);
                } catch (std::range_error const &exception) {
                    failure.id = u"RangeError";
                    const auto message = std::string_view(exception.what());
                    failure.message.assign(message.begin(), message.end());
                }
                expect(failure.id == entry.error) << folia::math::utf16::encode(entry.source);
                expect(failure.message == entry.message)
                    << folia::math::utf16::encode(entry.source);
                if (!entry.snapshot.empty())
                    expect(tree && snapshot(*tree) == entry.snapshot)
                        << folia::math::utf16::encode(entry.source);
                else
                    expect(tree == nullptr) << folia::math::utf16::encode(entry.source);
                expect(fixture.jax->parseOptions().request().parsers.empty())
                    << folia::math::utf16::encode(entry.source);
            }
        }
    };

    "Unicode command map releases with its loaded registry"_test = [] {
        std::weak_ptr<TokenMap> weak;
        {
            ConfigurationHandler registry;
            MapHandler maps;
            UnicodeConfiguration::initializeModule(registry, maps);
            weak = maps.getMap(u"unicode");
            expect(!weak.expired());
        }
        expect(weak.expired());
    };
};
