#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mo;

using namespace boost::ut;
using namespace folia::math::mml;

namespace {
void attachText(MmlDocument &document, MmlMo &mo, std::u16string text) {
    auto &token = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
    token.setText(text);
    mo.appendChild(token);
}

void expectNativeState(MmlMo const &actual, MoTexClassMode mode, std::optional<TexClass> working,
                       std::optional<double> left, std::optional<double> right) {
    expect(actual.texClassMode() == mode);
    expect(actual.state().working_tex_class == working);
    expect(actual.lspace == left);
    expect(actual.rspace == right);
}
} // namespace

suite mml_exact_clone_mo = [] {
    "exact clone copies raw unset Mo state and same-document values"_test = [] {
        MmlDocument document;
        auto &source = dynamic_cast<MmlMo &>(document.make(NodeKind::mo));
        attachText(document, source, u"+");
        source.lspace = 1.25;
        source.rspace.reset();

        auto &clone = dynamic_cast<MmlMo &>(document.cloneSubtree(source));
        expectNativeState(clone, MoTexClassMode::unset, std::nullopt, 1.25, std::nullopt);
        expect(&clone != &source);
        expect(clone.children().size() == 1_u);
        expect(dynamic_cast<TextNode *>(clone.children().front())->getText() == u"+");
        source.lspace = -9.0;
        source.rspace = 8.0;
        source.setTexClass(TexClass::rel);
        expectNativeState(clone, MoTexClassMode::unset, std::nullopt, 1.25, std::nullopt);
    };

    "exact cross-document clones preserve cleared and explicit Mo state after source destruction"_test =
        [] {
            MmlDocument destination;
            MmlMo *clearedClone = nullptr;
            MmlMo *explicitClone = nullptr;
            {
                MmlDocument sourceDocument;
                auto &cleared = dynamic_cast<MmlMo &>(sourceDocument.factory().make(NodeKind::mo));
                attachText(sourceDocument, cleared, u"+");
                cleared.lspace.reset();
                cleared.rspace = -2.5;
                expectNativeState(cleared, MoTexClassMode::cleared, std::nullopt, std::nullopt,
                                  -2.5);
                clearedClone = &dynamic_cast<MmlMo &>(destination.cloneSubtree(cleared));

                auto &explicitValue =
                    dynamic_cast<MmlMo &>(sourceDocument.factory().make(NodeKind::mo));
                attachText(sourceDocument, explicitValue, u"+");
                explicitValue.setTexClass(TexClass::rel);
                explicitValue.lspace = 0.0;
                explicitValue.rspace = 3.125;
                expectNativeState(explicitValue, MoTexClassMode::explicit_value, TexClass::rel, 0.0,
                                  3.125);
                explicitClone = &dynamic_cast<MmlMo &>(destination.cloneSubtree(explicitValue));

                // The destination has snapshots of all Mo-owned values, not aliases.
                cleared.lspace = 12.0;
                cleared.rspace.reset();
                explicitValue.setTexClass(TexClass::bin);
                explicitValue.lspace.reset();
                explicitValue.rspace = -16.0;
            }

            expect(clearedClone != nullptr);
            expect(explicitClone != nullptr);
            expectNativeState(*clearedClone, MoTexClassMode::cleared, std::nullopt, std::nullopt,
                              -2.5);
            expectNativeState(*explicitClone, MoTexClassMode::explicit_value, TexClass::rel, 0.0,
                              3.125);
            expect(clearedClone->children().size() == 1_u);
            expect(explicitClone->children().size() == 1_u);
            expect(dynamic_cast<TextNode *>(clearedClone->children().front())->getText() == u"+");
            expect(dynamic_cast<TextNode *>(explicitClone->children().front())->getText() == u"+");
        };
};
