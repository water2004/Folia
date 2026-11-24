#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mfenced;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mrow;
using namespace boost::ut;
namespace mml = folia::math::mml;

namespace {
mml::AttributeSet fencedAttributes() {
    mml::AttributeSet attributes;
    attributes.set(u"open", std::u16string{u"["});
    attributes.set(u"close", std::u16string{u"]"});
    attributes.set(u"separators", std::u16string{u";"});
    return attributes;
}

mml::MmlMfenced &makeFenced(mml::MmlDocument &document, std::span<mml::MmlNode *const> children) {
    auto &node = document.factory().make(mml::NodeKind::mfenced, fencedAttributes(), children);
    return dynamic_cast<mml::MmlMfenced &>(node);
}
} // namespace

suite math_mml_exact_clone_fenced = [] {
    "exact clone preserves distinct generated open close and separator nodes"_test = [] {
        mml::MmlDocument source;
        auto &left = source.factory().make(mml::NodeKind::mi);
        auto &right = source.factory().make(mml::NodeKind::mi);
        mml::MmlNode *children[]{&left, &right};
        auto &fenced = makeFenced(source, children);
        fenced.setInheritedAttributes({}, false, 0.0, false);
        expect(fenced.open != nullptr);
        expect(fenced.close != nullptr);
        expect(fenced.separators.size() == 1_u);
        expect(fenced.open != fenced.close);
        expect(fenced.open != fenced.separators[0]);
        expect(fenced.close != fenced.separators[0]);

        mml::MmlDocument destination;
        auto &clone = dynamic_cast<mml::MmlMfenced &>(destination.cloneSubtree(fenced));
        expect(clone.open != nullptr);
        expect(clone.close != nullptr);
        expect(clone.separators.size() == 1_u);
        expect(clone.open != clone.close);
        expect(clone.open != clone.separators[0]);
        expect(clone.close != clone.separators[0]);
        expect(clone.open->parent() == &clone);
        expect(clone.close->parent() == &clone);
        expect(clone.separators[0]->parent() == &clone);
    };

    "exact clone remaps generated Mo and Text nodes and survives source destruction"_test = [] {
        mml::MmlDocument destination;
        mml::MmlNode *cloneNode = nullptr;
        {
            mml::MmlDocument source;
            auto &left = source.factory().make(mml::NodeKind::mi);
            auto &right = source.factory().make(mml::NodeKind::mi);
            mml::MmlNode *children[]{&left, &right};
            auto &fenced = makeFenced(source, children);
            fenced.setInheritedAttributes({}, false, 0.0, false);
            expect(fenced.open != nullptr);
            expect(fenced.close != nullptr);
            expect(fenced.separators.size() == 1_u);

            // One source auxiliary appears in several fields. The operation map
            // must preserve that identity rather than cloning it more than once.
            fenced.close = fenced.open;
            fenced.separators[0] = fenced.open;
            fenced.separators.push_back(nullptr);
            fenced.separators.push_back(fenced.open);
            cloneNode = &destination.cloneSubtree(fenced);
        }

        auto &clone = dynamic_cast<mml::MmlMfenced &>(*cloneNode);
        expect(&clone.document() == &destination);
        expect(clone.children().size() == 2_u);
        expect(clone.open != nullptr);
        expect(clone.open == clone.close);
        expect(clone.separators.size() == 3_u);
        expect(clone.separators[0] == clone.open);
        expect(clone.separators[1] == nullptr);
        expect(clone.separators[2] == clone.open);
        expect(clone.open->parent() == &clone);
        expect(clone.open->kind() == mml::NodeKind::mo);
        expect(clone.open->children().size() == 1_u);
        expect(clone.open->children()[0]->kind() == mml::NodeKind::text);
        expect(dynamic_cast<mml::TextNode &>(*clone.open->children()[0]).getText() == u"[");
    };

    "exact clone reuses a normal child when an auxiliary field aliases it"_test = [] {
        mml::MmlDocument source;
        auto &token = source.factory().make(mml::NodeKind::mi);
        mml::MmlNode *children[]{&token};
        auto &fenced = makeFenced(source, children);
        auto &normalMo = source.factory().make(mml::NodeKind::mo);
        auto &text = source.factory().make(mml::NodeKind::text);
        text.setText(u"|");
        normalMo.appendChild(text);
        fenced.appendChild(normalMo);
        fenced.open = &normalMo;

        mml::MmlDocument destination;
        auto &clone = dynamic_cast<mml::MmlMfenced &>(destination.cloneSubtree(fenced));
        expect(clone.children().size() == 2_u);
        expect(clone.open == clone.children()[1]);
        expect(clone.open->parent() == &clone);
        expect(clone.open->kind() == mml::NodeKind::mo);
        expect(clone.open->children().size() == 1_u);
        expect(dynamic_cast<mml::TextNode &>(*clone.open->children()[0]).getText() == u"|");
    };

    "exact clone preserves absent generated references"_test = [] {
        mml::MmlDocument source;
        auto &raw = dynamic_cast<mml::MmlMfenced &>(source.factory().make(mml::NodeKind::mfenced));
        mml::MmlDocument destination;
        auto &clone = dynamic_cast<mml::MmlMfenced &>(destination.cloneSubtree(raw));
        expect(clone.open == nullptr);
        expect(clone.close == nullptr);
        expect(clone.separators.empty());
        expect(clone.children().empty());
    };

    "conflicting auxiliary clone parent rolls back destination and preserves source"_test = [] {
        mml::MmlDocument source;
        auto &row = source.factory().make(mml::NodeKind::mrow);
        auto &operatorNode = source.factory().make(mml::NodeKind::mo);
        row.appendChild(operatorNode);
        mml::MmlNode *children[]{&row};
        auto &fenced = makeFenced(source, children);
        fenced.open = &operatorNode;
        expect(row.parent() == &fenced);
        expect(operatorNode.parent() == &row);
        expect(fenced.open == &operatorNode);

        mml::MmlDocument destination;
        auto &existingRoot = destination.factory().make(mml::NodeKind::mrow);
        destination.setRoot(existingRoot);
        const auto originalSize = destination.size();
        bool rejected = false;
        try {
            (void)destination.cloneSubtree(fenced);
        } catch (std::invalid_argument const &) {
            rejected = true;
        }

        expect(rejected);
        expect(destination.size() == originalSize);
        expect(destination.root() == &existingRoot);
        expect(row.parent() == &fenced);
        expect(operatorNode.parent() == &row);
        expect(fenced.children().size() == 1_u);
        expect(fenced.children()[0] == &row);
        expect(fenced.open == &operatorNode);
    };
};
