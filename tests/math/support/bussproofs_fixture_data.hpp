#pragma once

#include "math/support/tex_binding_fixtures.hpp"

namespace math_test::fixtures {

template <class TexArgument>
struct BussproofsFixtureData {
    struct BussproofsTree {
        std::u16string name, source, snapshot;
    };
    struct BussproofsError {
        std::u16string source;
        std::string id;
    };
    struct BussproofsRuntime {
        std::u16string source;
        std::string kind;
        std::u16string message;
    };
    struct BussproofsStep {
        int group;
        std::u16string source, snapshot;
        std::string error, runtime;
        bool reset{};
    };
    struct BussproofsDirect {
        std::u16string name;
        bool present;
        std::u16string semantics;
    };
    struct BussproofsBinding {
        std::u16string name;
        int method;
        std::vector<TexArgument> arguments;
    };

    static const auto& bussproofsTrees() {
        static const auto result = math_test::fixtures::records<BussproofsTree>(
            "math/fixtures/packages/bussproofs/trees.tsv", 3, 63, [](const auto& r) {
                return BussproofsTree{folia::test::data::decode_utf16(r[0]),
                    folia::test::data::decode_utf16(r[1]),
                    folia::test::data::decode_utf16(r[2])};
            });
        return result;
    }

    static const auto& bussproofsErrors() {
        static const auto result = math_test::fixtures::records<BussproofsError>(
            "math/fixtures/packages/bussproofs/errors.tsv", 2, 29, [](const auto& r) {
                return BussproofsError{folia::test::data::decode_utf16(r[0]),
                    math_test::fixtures::bytes(r[1])};
            });
        return result;
    }

    static const auto& bussproofsRuntime() {
        static const auto result = math_test::fixtures::records<BussproofsRuntime>(
            "math/fixtures/packages/bussproofs/runtime.tsv", 3, 2, [](const auto& r) {
                return BussproofsRuntime{folia::test::data::decode_utf16(r[0]),
                    math_test::fixtures::bytes(r[1]),
                    folia::test::data::decode_utf16(r[2])};
            });
        return result;
    }

    static const auto& bussproofsSteps() {
        static const auto result = math_test::fixtures::records<BussproofsStep>(
            "math/fixtures/packages/bussproofs/steps.tsv", 6, 15, [](const auto& r) {
                return BussproofsStep{math_test::fixtures::integer<int>(r[0]),
                    folia::test::data::decode_utf16(r[1]),
                    folia::test::data::decode_utf16(r[2]),
                    math_test::fixtures::bytes(r[3]),
                    math_test::fixtures::bytes(r[4]),
                    math_test::fixtures::boolean(r[5])};
            });
        return result;
    }

    static const auto& bussproofsDirect() {
        static const auto result = math_test::fixtures::records<BussproofsDirect>(
            "math/fixtures/packages/bussproofs/direct.tsv", 3, 5, [](const auto& r) {
                return BussproofsDirect{folia::test::data::decode_utf16(r[0]),
                    math_test::fixtures::boolean(r[1]),
                    folia::test::data::decode_utf16(r[2])};
            });
        return result;
    }

    static const auto& bussproofsBindings() {
        static const auto result = [] {
            auto arguments = math_test::fixtures::bindingArguments<TexArgument>(
                "math/fixtures/packages/bussproofs/arguments.tsv", 37, 33);
            std::size_t index{};
            return math_test::fixtures::records<BussproofsBinding>(
                "math/fixtures/packages/bussproofs/bindings.tsv", 3, 33, [&](const auto& r) {
                    if (arguments[index].size() != math_test::fixtures::integer<std::size_t>(r[2]))
                        throw std::runtime_error("Bussproofs binding fixture size changed");
                    BussproofsBinding value{folia::test::data::decode_utf16(r[0]),
                        math_test::fixtures::integer<int>(r[1]), std::move(arguments[index])};
                    ++index;
                    return value;
                });
        }();
        return result;
    }
};

} // namespace math_test::fixtures
