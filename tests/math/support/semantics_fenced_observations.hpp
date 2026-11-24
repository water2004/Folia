#pragma once

#include "math/support/mathml_observations.hpp"



inline const auto& semanticsFencedObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/semantics-fenced/observations.tsv"};
        std::span<mathml_scene_test::Case const> sourceSemanticsCases;
        std::span<mathml_scene_test::Case const> sourceMfencedCases;
        Corpus() {
            input.field("sourceSemanticsCases", sourceSemanticsCases);
            if (sourceSemanticsCases.size() != 4)
                throw std::runtime_error("Incomplete semantics-fenced fixture: sourceSemanticsCases");
            input.field("sourceMfencedCases", sourceMfencedCases);
            if (sourceMfencedCases.size() != 7)
                throw std::runtime_error("Incomplete semantics-fenced fixture: sourceMfencedCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceSemanticsCases() { return semanticsFencedObservations().sourceSemanticsCases; }
inline auto sourceMfencedCases() { return semanticsFencedObservations().sourceMfencedCases; }
