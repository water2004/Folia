#pragma once

#include "math/support/tex_binding_fixtures.hpp"

namespace math_test::fixtures {

template <class TexArgument, class AttributeEntry>
struct PhysicsFixtureData {
    struct PhysicsTree {
        std::u16string name, source;
        bool italic, arrow;
        std::u16string snapshot;
    };
    struct PhysicsError {
        std::u16string source;
        bool italic, arrow;
        std::string id;
    };
    struct PhysicsRuntime {
        std::u16string source;
        std::string kind;
        std::u16string message;
    };
    struct PhysicsStep {
        int group;
        std::u16string source, snapshot;
        std::string error, runtime;
    };
    struct PhysicsDirect {
        std::u16string name, snapshot;
    };
    struct PhysicsBinding {
        std::u16string map, token;
        std::string method;
        std::vector<TexArgument> arguments;
        std::optional<std::u16string> character;
        std::vector<AttributeEntry> attributes;
    };

    static const auto& physicsTrees() {
        static const auto result = math_test::fixtures::records<PhysicsTree>(
            "math/fixtures/packages/physics/trees.tsv", 5, 192, [](const auto& r) {
                return PhysicsTree{folia::test::data::decode_utf16(r[0]),
                    folia::test::data::decode_utf16(r[1]),
                    math_test::fixtures::boolean(r[2]),
                    math_test::fixtures::boolean(r[3]),
                    folia::test::data::decode_utf16(r[4])};
            });
        return result;
    }

    static const auto& physicsErrors() {
        static const auto result = math_test::fixtures::records<PhysicsError>(
            "math/fixtures/packages/physics/errors.tsv", 4, 37, [](const auto& r) {
                return PhysicsError{folia::test::data::decode_utf16(r[0]),
                    math_test::fixtures::boolean(r[1]),
                    math_test::fixtures::boolean(r[2]),
                    math_test::fixtures::bytes(r[3])};
            });
        return result;
    }

    static const auto& physicsRuntime() {
        static const auto result = math_test::fixtures::records<PhysicsRuntime>(
            "math/fixtures/packages/physics/runtime.tsv", 3, 1, [](const auto& r) {
                return PhysicsRuntime{folia::test::data::decode_utf16(r[0]),
                    math_test::fixtures::bytes(r[1]),
                    folia::test::data::decode_utf16(r[2])};
            });
        return result;
    }

    static const auto& physicsSteps() {
        static const auto result = math_test::fixtures::records<PhysicsStep>(
            "math/fixtures/packages/physics/steps.tsv", 5, 13, [](const auto& r) {
                return PhysicsStep{math_test::fixtures::integer<int>(r[0]),
                    folia::test::data::decode_utf16(r[1]),
                    folia::test::data::decode_utf16(r[2]),
                    math_test::fixtures::bytes(r[3]),
                    math_test::fixtures::bytes(r[4])};
            });
        return result;
    }

    static const auto& physicsDirect() {
        static const auto result = math_test::fixtures::records<PhysicsDirect>(
            "math/fixtures/packages/physics/direct.tsv", 2, 9, [](const auto& r) {
                return PhysicsDirect{folia::test::data::decode_utf16(r[0]),
                    folia::test::data::decode_utf16(r[1])};
            });
        return result;
    }

    static const auto& physicsBindings() {
        static const auto result = [] {
            auto arguments = math_test::fixtures::bindingArguments<TexArgument>(
                "math/fixtures/packages/physics/arguments.tsv", 198, 196);
            auto attributes = math_test::fixtures::bindingAttributes<AttributeEntry>(
                "math/fixtures/packages/physics/attributes.tsv", 5, 196);
            std::size_t index{};
            return math_test::fixtures::records<PhysicsBinding>(
                "math/fixtures/packages/physics/bindings.tsv", 7, 196, [&](const auto& r) {
                    if (arguments[index].size() != math_test::fixtures::integer<std::size_t>(r[3]) ||
                        attributes[index].size() != math_test::fixtures::integer<std::size_t>(r[6]))
                        throw std::runtime_error("Physics binding fixture size changed");
                    PhysicsBinding value{folia::test::data::decode_utf16(r[0]),
                        folia::test::data::decode_utf16(r[1]), math_test::fixtures::bytes(r[2]),
                        std::move(arguments[index]), math_test::fixtures::boolean(r[4]) ?
                            std::optional{folia::test::data::decode_utf16(r[5])} : std::nullopt,
                        std::move(attributes[index])};
                    ++index;
                    return value;
                });
        }();
        return result;
    }
};

} // namespace math_test::fixtures
