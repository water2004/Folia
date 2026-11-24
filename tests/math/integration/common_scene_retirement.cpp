#include "math/support/common_scene_test.hpp"

suite common_scene_retirement_behavior = [] {
    "frozen_Scene_serializes_after_job_and_document_retirement_and_repeat_request"_test = [] {
        InputOwner owner;
        NoUnexpectedMeasurement measurement;
        constexpr std::array<std::string_view, 12> repeatedCases{
            "fraction-inline",
            "plain",
            "scene-inline-break-before",
            "scene-inline-break-after",
            "scene-table-array-row-rules",
            "scene-table-label-right",
            "scene-table-label-left-metric",
            "scene-enclose-radical-top",
            "scene-enclose-boundary-roundedbox-empty",
            "scene-enclose-notation-circle",
            "scene-enclose-notation-longdiv",
            "scene-enclose-notation-northeastsouthwestarrow"};
        for (auto name : repeatedCases) {
            const auto &source = sourceCase(name);
            auto first = buildScene(texInput(source), owner, measurement);
            const auto firstSvg = serialize(first, owner);
            auto second = buildScene(texInput(source), owner, measurement);
            const auto secondSvg = serialize(second, owner);
            const bool repeatedMatches = firstSvg == secondSvg;
            const auto expectedSvg = source_svg_paint::materialize(source.svg);
            const bool sourceMatches = firstSvg == expectedSvg;
            expect(bool(repeatedMatches))
                << "repeated request changed the completed Scene output for " << name;
            expect(bool(sourceMatches))
                << "retired-request Scene differs from independent Source SVG for " << name;
            reportFirstDifference(name, expectedSvg, firstSvg);
            std::cerr << "Common-to-Scene case=" << name << "-retirement result="
                      << (repeatedMatches && sourceMatches ? "success" : "mismatch") << '\n';
        }
    };
};
