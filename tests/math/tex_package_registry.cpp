#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "support/folia_test.hpp"

import folia.math;
import folia.math.input.tex.default_packages;
import folia.math.util.utf16;

using namespace boost::ut;
using namespace folia::math::tex;

namespace {
#include "fixtures/tex_package_registry_source.hpp"
#include "fixtures/texvc_svg_source.hpp"

struct Warnings final : WarningSink {
    std::vector<std::u16string> messages;
    void warn(std::u16string_view text) override { messages.emplace_back(text); }
};
void compareSvg(std::string_view name, std::string_view actual, std::string_view expected) {
    if (actual != expected) {
        std::size_t at = 0;
        while (at < actual.size() && at < expected.size() && actual[at] == expected[at]) ++at;
        const auto begin = at > 48 ? at - 48 : 0;
        std::cerr << name << " first differing SVG byte " << at << '\n'
                  << "expected: " << expected.substr(begin, 180) << '\n'
                  << "actual: " << actual.substr(begin, 180) << '\n';
    }
    expect(actual == expected) << name;
}
} // namespace

suite tex_package_conformance = [] {
    "production TeX package registry matches every upstream finite command map"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        DefaultPackages::initialize(registry, maps,
            std::make_shared<folia::math::handlers::html::HTMLDomStringsOptions>());
        MacroCallbacks callbacks{{}, {}};
        Warnings warnings;
        TexOptionValues options;
        options.packages = DefaultPackages::selections();
        expect(options.packages->size() == std::size(sourcePackages));
        TeX input(registry, maps, callbacks, warnings, options);
        expect(warnings.messages.empty());
        for (std::size_t i = 0; i < std::size(sourcePackages); ++i) {
            expect(registry.get(sourcePackages[i]) != nullptr);
            expect(options.packages->at(i).name == sourcePackages[i]);
        }
        for (auto const &source : sourceRegistrations) {
            const auto context = folia::math::utf16::encode(source.map) + ":" +
                                 folia::math::utf16::encode(source.token);
            auto map = maps.getMap(source.map);
            expect(map != nullptr) << context;
            if (!map) continue;
            expect(map->contains(source.token)) << context;
            auto handler = input.configuration().handlers.get(source.kind);
            expect(handler != nullptr) << context;
            if (!handler) continue;
            auto applicable = handler->applicable(source.token);
            expect(applicable != nullptr) << context;
            if (applicable) expect(applicable->name() == source.owner) << context;
            if (source.character) {
                auto value = map->lookup(source.token);
                auto token = std::get_if<Token const *>(&value);
                expect(token && *token && (*token)->character == source.text) << context;
            }
        }
    };

    "texvc aliases and preserved v4 commands match upstream SVG in both modes"_test = [] {
        folia::math::Renderer renderer;
        // The same session handles all cases, including explicit TeX definition
        // groups and restoration of aliases after those groups close.
        for (auto const &source : texvcSourceSvgs) {
            folia::math::RenderOptions options;
            options.display = source.display;
            auto result = renderer.render(source.tex, {}, options);
            compareSvg(source.name, result.svg, source.svg);
            expect(result.svg.find("data-mjx-error=") == std::string::npos) << source.name;
        }
    };

    "texvc batch uses the same package profile as individual requests"_test = [] {
        folia::math::Renderer renderer;
        std::vector<folia::math::RenderInput> inputs;
        for (auto const &source : texvcSourceSvgs) {
            folia::math::RenderOptions options;
            options.display = source.display;
            inputs.push_back({source.tex, {}, options, std::chrono::seconds(15)});
        }
        const auto results = renderer.renderBatch(inputs);
        expect(results.size() == inputs.size());
        for (std::size_t i = 0; i < results.size(); ++i) {
            auto const &source = texvcSourceSvgs[i];
            expect(!results[i].error && results[i].result.has_value()) << source.name;
            if (results[i].result)
                compareSvg(source.name, results[i].result->svg, source.svg);
        }
    };
};
