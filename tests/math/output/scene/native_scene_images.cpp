#include "support/folia_test.hpp"
import folia.math.output.scene.builder;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

GlyphRun image_test_glyph() {
    auto resource = GlyphResource::make(
        "image-resource-order",
        GlyphOutline{PathGeometry{{MoveTo{{0, 0}}, LineTo{{1, 1}}, ClosePath{}}}});
    return GlyphRun{{Glyph{resource, {"normal", "image-resource-order", 0x61}, {}, 1, {}}}};
}

suite native_scene_images = [] {
    "image values preserve authored dimensions and transform after Builder retirement"_test = [] {
        std::unique_ptr<Scene> saved;
        std::string href = "data:image/svg+xml,%3Csvg/%3E";
        std::string alt = "source alt";
        Transform transform{
            std::vector<TransformOperation>{Translation{0.25, -0.5}, Affine{1, 0, 0, -1, 0, 0}}};
        {
            Builder builder;
            builder.add_primitive(0, Primitive{Image{{2, 3}, href, alt, transform}});
            builder.add_primitive(0, Primitive{Image{{-1.5, -0.25}, href, alt, {}}});
            builder.add_primitive(0, Primitive{Image{{0, 0}, href, alt, {}}});
            href = "caller replaced href";
            alt = "caller replaced alt";
            saved = std::make_unique<Scene>(std::move(builder).freeze());
        }
        expect(saved != nullptr);
        expect(saved->root().entries.size() == 3_u);
        auto const *positive = std::get_if<Image>(&std::get<Primitive>(saved->root().entries[0]));
        auto const *signed_size =
            std::get_if<Image>(&std::get<Primitive>(saved->root().entries[1]));
        auto const *zero_size = std::get_if<Image>(&std::get<Primitive>(saved->root().entries[2]));
        expect(positive != nullptr);
        expect(signed_size != nullptr);
        expect(zero_size != nullptr);
        if (positive && signed_size && zero_size) {
            expect(positive->size == Size{2, 3});
            expect(signed_size->size == Size{-1.5, -0.25});
            expect(zero_size->size == Size{0, 0});
            expect(positive->href == "data:image/svg+xml,%3Csvg/%3E");
            expect(positive->alt == "source alt");
            expect(positive->transform == transform);
        }
    };

    "image snapshot rejects nonfinite dimensions and transforms"_test = [] {
        auto rejects = [](Image image) {
            Group root;
            root.entries.emplace_back(Primitive{std::move(image)});
            try {
                auto scene = Scene{std::move(root), {}, {}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto const infinity = std::numeric_limits<double>::infinity();
        auto const nan = std::numeric_limits<double>::quiet_NaN();
        expect(rejects(Image{{nan, 1}, {}, {}, {}}));
        expect(rejects(Image{{1, infinity}, {}, {}, {}}));
        expect(rejects(Image{
            {1, 1}, {}, {}, Transform{std::vector<TransformOperation>{Translation{infinity, 0}}}}));
    };

    "images are not resource-order glyph events"_test = [] {
        Builder builder;
        builder.add_primitive(0, Primitive{image_test_glyph()});
        builder.add_primitive(0, Primitive{Image{{1, 1}, "data:image/png;base64,AA==", "", {}}});
        builder.add_primitive(0, Primitive{image_test_glyph()});
        auto scene = std::move(builder).freeze();
        expect(std::get<GlyphRun>(std::get<Primitive>(scene.root().entries[0])).resource_order ==
               std::uint64_t{1});
        expect(std::holds_alternative<Image>(std::get<Primitive>(scene.root().entries[1])));
        expect(std::get<GlyphRun>(std::get<Primitive>(scene.root().entries[2])).resource_order ==
               std::uint64_t{2});
    };
};
} // namespace
