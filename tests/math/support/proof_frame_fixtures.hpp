#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct ProofFrameTree {
    std::u16string name;
    std::u16string snapshot;
};

inline const auto& proofFrameTrees() {
    static const auto result = records<ProofFrameTree>(
        "math/fixtures/packages/bussproofs-items/trees.tsv", 2, 11, [](const auto& r) {
            return ProofFrameTree{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

struct ProofFrameError {
    std::u16string name;
    std::string id;
    std::u16string message;
};

inline const auto& proofFrameErrors() {
    static const auto result = records<ProofFrameError>(
        "math/fixtures/packages/bussproofs-items/errors.tsv", 3, 6, [](const auto& r) {
            return ProofFrameError{folia::test::data::decode_utf16(r[0]),
                bytes(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

} // namespace math_test::fixtures
