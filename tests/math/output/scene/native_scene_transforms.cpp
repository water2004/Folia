#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.scene.html_snapshot;
import folia.math.adaptors.lite_adaptor;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

suite native_scene_transforms = [] {
    "affine composition follows parent times local"_test = [] {
        auto const result = compose(Affine{1, 0, 0, 1, 2, 3}, Affine{0, 1, -1, 0, 4, 5});
        expect(result == Affine{0, 1, -1, 0, 6, 8});
        auto const point = Point{1, 2};
        expect(result.a * point.x + result.c * point.y + result.e == 4.0_d);
        expect(result.b * point.x + result.d * point.y + result.f == 9.0_d);
    };

    "ordered transforms preserve operations and derive SVG-order matrix"_test = [] {
        Transform identity;
        expect(identity.operations().empty());
        expect(identity.matrix() == Affine{});
        Transform transform{std::vector<TransformOperation>{
            Translation{3, 4}, Scaling{2, 3}, Rotation{90, {1, 2}}, Affine{1, 0, 0, 1, -2, 1}}};
        auto const operations = transform.operations();
        expect(operations.size() == 4_u);
        expect(std::holds_alternative<Translation>(operations[0]));
        expect(std::holds_alternative<Scaling>(operations[1]));
        expect(std::holds_alternative<Rotation>(operations[2]));
        expect(std::holds_alternative<Affine>(operations[3]));

        auto const translation =
            Transform{std::vector<TransformOperation>{Translation{3, 4}}}.matrix();
        auto const scaling = Transform{std::vector<TransformOperation>{Scaling{2, 3}}}.matrix();
        auto const rotation =
            Transform{std::vector<TransformOperation>{Rotation{90, {1, 2}}}}.matrix();
        auto expected = compose(Affine{}, translation);
        expected = compose(expected, scaling);
        expected = compose(expected, rotation);
        expected = compose(expected, Affine{1, 0, 0, 1, -2, 1});
        expect(transform.matrix() == expected);

        // SVG list order gives T*S, so the point is scaled then translated.
        Transform ordered{std::vector<TransformOperation>{Translation{3, 4}, Scaling{2, 3}}};
        auto const result = ordered.matrix();
        expect(result.a * 1 + result.c * 1 + result.e == 5.0_d);
        expect(result.b * 1 + result.d * 1 + result.f == 7.0_d);

        Transform reflection{std::vector<TransformOperation>{Scaling{-2, 1}}};
        auto const reflected = reflection.matrix();
        expect(reflected.a * 2 + reflected.c * 3 + reflected.e == -4.0_d);
        expect(reflected.b * 2 + reflected.d * 3 + reflected.f == 3.0_d);
    };

    "center rotation is counterclockwise in y-up scene coordinates"_test = [] {
        Transform rotation{std::vector<TransformOperation>{Rotation{90, {1, 2}}}};
        auto const matrix = rotation.matrix();
        Point const source{2, 2};
        auto const x = matrix.a * source.x + matrix.c * source.y + matrix.e;
        auto const y = matrix.b * source.x + matrix.d * source.y + matrix.f;
        expect(std::abs(x - 1) < 1e-12);
        expect(std::abs(y - 3) < 1e-12);
    };

    "affine convenience creates one explicit operation"_test = [] {
        Affine const affine{1, 0, 0, 1, 2, 3};
        Transform transform = affine;
        expect(transform.operations().size() == 1_u);
        expect(std::holds_alternative<Affine>(transform.operations().front()));
        expect(transform.matrix() == affine);
    };

    "scene rejects non-finite transform data at freeze"_test = [] {
        auto const infinity = std::numeric_limits<Unit>::infinity();
        auto rejects = [infinity](Transform transform) {
            Builder builder;
            (void)builder.add_group(0, std::move(transform));
            try {
                [[maybe_unused]] auto scene = std::move(builder).freeze();
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        expect(rejects(Transform{std::vector<TransformOperation>{Translation{infinity, 0}}}));
        expect(rejects(Transform{std::vector<TransformOperation>{Scaling{1, infinity}}}));
        expect(rejects(Transform{std::vector<TransformOperation>{Rotation{infinity, {0, 0}}}}));
        expect(rejects(Transform{std::vector<TransformOperation>{Rotation{0, {0, infinity}}}}));
        expect(rejects(Transform{Affine{infinity, 0, 0, 1, 0, 0}}));
    };


};
} // namespace
