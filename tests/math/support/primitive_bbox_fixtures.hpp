#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

template <class BBoxData>
struct PrimitiveBBoxFixtureData {
    struct SourceBBoxSnapshot { std::array<double, 11> values; std::string pwidth; };
    enum class SourceBBoxOp { combine, append, update_from, rescale, empty };
    struct SourceBBoxStep { SourceBBoxSnapshot child; SourceBBoxOp operation; double x, y; };
    struct SourceBBoxSide { std::string name; int index; std::string dimension; };

private:
    static double number(std::string_view field) { return std::bit_cast<double>(binary64(field)); }
    static std::optional<double> optional(std::string_view present, std::string_view field) {
        if (boolean(present)) return number(field);
        if (!field.empty()) throw std::runtime_error("Absent BBox reference field has a value");
        return std::nullopt;
    }
    static SourceBBoxSnapshot snapshot(const auto& r) {
        SourceBBoxSnapshot result;
        for (std::size_t i{}; i < result.values.size(); ++i) result.values[i] = number(r[i]);
        result.pwidth = bytes(r[11]);
        return result;
    }
    static SourceBBoxOp operation(std::string_view value) {
        if (value == "combine") return SourceBBoxOp::combine;
        if (value == "append") return SourceBBoxOp::append;
        if (value == "update_from") return SourceBBoxOp::update_from;
        if (value == "rescale") return SourceBBoxOp::rescale;
        if (value == "empty") return SourceBBoxOp::empty;
        throw std::runtime_error("Unknown BBox reference operation");
    }

public:
    static const auto& sourceBBoxDefinitions() {
        static const auto result = records<BBoxData>(
            "math/fixtures/util/bbox/definitions.tsv", 6, 7, [](const auto& r) {
                return BBoxData{optional(r[0], r[1]), optional(r[2], r[3]), optional(r[4], r[5])};
            });
        return result;
    }
    static const auto& sourceBBoxExpected() {
        static const auto result = records<SourceBBoxSnapshot>(
            "math/fixtures/util/bbox/expected.tsv", 12, 46, [](const auto& r) { return snapshot(r); });
        return result;
    }
    static const auto& sourceBBoxSteps() {
        static const auto result = records<SourceBBoxStep>(
            "math/fixtures/util/bbox/steps.tsv", 15, 32, [](const auto& r) {
                return SourceBBoxStep{snapshot(r), operation(r[12]), number(r[13]), number(r[14])};
            });
        return result;
    }
    static const auto& sourceBBoxZeroSetup() {
        static const auto result = records<SourceBBoxSnapshot>(
            "math/fixtures/util/bbox/zero-setup.tsv", 12, 1, [](const auto& r) { return snapshot(r); });
        return result.front();
    }
    static const auto& sourceBBoxFullWidth() {
        static const auto result = bytes(rows("math/fixtures/util/bbox/full-width.tsv", 1, 1)[0][0]);
        return result;
    }
    static const auto& sourceBBoxSides() {
        static const auto result = records<SourceBBoxSide>(
            "math/fixtures/util/bbox/sides.tsv", 3, 4, [](const auto& r) {
                return SourceBBoxSide{bytes(r[0]), integer<int>(r[1]), bytes(r[2])};
            });
        return result;
    }
};

} // namespace math_test::fixtures
