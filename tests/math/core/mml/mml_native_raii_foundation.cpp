#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
using namespace boost::ut;
namespace mml = folia::math::mml;
suite math_native_raii_mml_kind_vocabulary = [] {
    "all_pinned_mml_factory_kinds_round_trip_exact_source_names"_test = [] {
        expect(mml::allNodeKinds.size() == 43u);
        for (const auto kind : mml::allNodeKinds) {
            const auto name = mml::nodeKindName(kind);
            const auto decoded = mml::nodeKindFromName(name);
            expect(!name.empty());
            expect(decoded.has_value());
            if (decoded)
                expect(*decoded == kind);
        }
        expect(mml::nodeKindName(mml::NodeKind::inferred_mrow) == "inferredMrow");
        expect(mml::nodeKindName(mml::NodeKind::annotation_xml) == "annotation-xml");
        expect(mml::nodeKindName(mml::NodeKind::tex_atom) == "TeXAtom");
        expect(mml::nodeKindName(mml::NodeKind::math_choice) == "MathChoice");
        expect(mml::nodeKindName(mml::NodeKind::xml) == "XML");
        expect(!mml::nodeKindFromName("zero").has_value());
        expect(!mml::nodeKindFromName("mrow_like").has_value());
    };
};
suite math_native_raii_mml_foundation = [] {
    "arena_nodes_keep_addresses_for_document_lifetime"_test = [] {
        mml::MmlNode *address = nullptr;
        unsigned count = 0;
        {
            mml::MmlDocument d;
            auto &first = d.make(mml::NodeKind::math);
            first.setText(u"owned");
            address = &first;
            for (int i = 0; i < 2048; ++i)
                d.make(mml::NodeKind::mi);
            expect(address == &first);
            expect(d.size() == 2049u);
            expect(first.text() == u"owned");
            count = d.size();
        }
        expect(count == 2049u);
    };
    "links_reparent_insert_replace_remove_and_root_are_consistent"_test = [] {
        mml::MmlDocument d;
        auto &root = d.make(mml::NodeKind::math);
        auto &a = d.make(mml::NodeKind::mi);
        auto &b = d.make(mml::NodeKind::mn);
        auto &c = d.make(mml::NodeKind::mo);
        d.setRoot(root);
        d.append(root, a);
        d.append(root, b);
        d.insert(root, 1, c);
        expect(root.children().size() == 3u);
        expect(root.children()[0] == &a);
        expect(root.children()[1] == &c);
        expect(root.children()[2] == &b);
        expect(c.parent() == &root);
        auto &other = d.make(mml::NodeKind::mrow);
        d.append(other, c);
        expect(c.parent() == &other);
        expect(root.children().size() == 2u);
        d.replace(other, c, a);
        expect(other.children()[0] == &a);
        expect(a.parent() == &other);
        expect(c.parent() == nullptr);
        auto &sibling = d.make(mml::NodeKind::mi);
        d.append(root, sibling);
        d.append(root, c);
        d.replace(root, sibling, c);
        expect(root.children().size() == 2u);
        expect(root.children()[1] == &c);
        expect(c.parent() == &root);
        expect(sibling.parent() == nullptr);
        d.remove(a);
        expect(a.parent() == nullptr);
        expect(other.children().empty());
        d.setRoot(other);
        expect(d.root() == &other);
    };
    "cycles_self_parenting_and_foreign_document_references_are_rejected"_test = [] {
        mml::MmlDocument a, b;
        auto &x = a.make(mml::NodeKind::math);
        auto &y = a.make(mml::NodeKind::mrow);
        auto &z = b.make(mml::NodeKind::mi);
        a.append(x, y);
        bool cycle = false, self = false, foreign = false;
        try {
            a.append(y, x);
        } catch (...) {
            cycle = true;
        }
        try {
            a.append(x, x);
        } catch (...) {
            self = true;
        }
        try {
            a.append(x, z);
        } catch (...) {
            foreign = true;
        }
        auto &unaffected = a.make(mml::NodeKind::mrow);
        a.append(unaffected, x);
        bool badIndex = false, badReplace = false;
        try {
            a.insert(y, 99, x);
        } catch (...) {
            badIndex = true;
        }
        try {
            a.replace(y, x, unaffected);
        } catch (...) {
            badReplace = true;
        }
        expect(cycle);
        expect(self);
        expect(foreign);
        expect(badIndex);
        expect(badReplace);
        expect(x.parent() == &unaffected);
        expect(y.parent() == &x);
        expect(z.parent() == nullptr);
        expect(unaffected.children().size() == 1u);
    };
    "subtree_clone_copies_values_and_has_fresh_links"_test = [] {
        mml::MmlDocument d;
        auto &root = d.make(mml::NodeKind::math);
        auto &child = d.make(mml::NodeKind::mi);
        root.setText(u"root");
        child.setText(u"α𝛂");
        child.attributes().set(u"mathvariant", u"bold");
        d.append(root, child);
        mml::MmlDocument target;
        auto &copy = target.cloneSubtree(root);
        expect(&copy.document() == &target);
        expect(copy.kind() == root.kind());
        expect(copy.text().compare(root.text()) == 0);
        expect(copy.parent() == nullptr);
        expect(copy.children().size() == 1u);
        expect(copy.children()[0] != &child);
        expect(copy.children()[0]->parent() == &copy);
        expect(copy.children()[0]->text().compare(child.text()) == 0);
        expect(std::get<std::u16string>(*copy.children()[0]->attributes().own(u"mathvariant")) ==
               u"bold");
        copy.children()[0]->setText(u"changed");
        expect(child.text() == u"α𝛂");
        expect(child.parent() == &root);
        expect(d.size() == 2u);
        expect(target.size() == 2u);
    };
    "root_changes_detach_the_previous_root_and_unroot_moved_nodes"_test = [] {
        mml::MmlDocument d;
        auto &first = d.make(mml::NodeKind::math);
        auto &second = d.make(mml::NodeKind::mrow);
        auto &child = d.make(mml::NodeKind::mi);
        d.setRoot(first);
        d.append(first, child);
        d.setRoot(second);
        expect(d.root() == &second);
        expect(first.parent() == nullptr);
        expect(child.parent() == &first);
        d.append(second, first);
        expect(d.root() == &second);
        expect(first.parent() == &second);
        d.setRoot(first);
        expect(d.root() == &first);
        expect(first.parent() == nullptr);
        expect(second.children().empty());
    };
    "attribute_utf16_literals_remain_text_in_all_layers"_test = [] {
        mml::AttributeSet a;
        a.set(u"own", u"false");
        a.setInherited(u"inherited", u"");
        a.setDefault(u"default", u"bold");
        a.setGlobalDefault(u"global", u"normal");
        expect(std::get<std::u16string>(*a.own(u"own")) == u"false");
        expect(std::get<std::u16string>(*a.inherited(u"inherited")).empty());
        expect(std::get<std::u16string>(*a.defaultValue(u"default")) == u"bold");
        expect(std::get<std::u16string>(*a.globalDefault(u"global")) == u"normal");
        a.set(u"bool", false);
        expect(!std::get<bool>(*a.own(u"bool")));
    };
    "attributes_keep_order_and_lookup_own_then_inherited_then_default"_test = [] {
        mml::AttributeSet a;
        a.setDefault(u"display", true);
        a.setInherited(u"scriptlevel", 2.0);
        a.set(u"display", false);
        a.set(u"label", u"x");
        expect(a.lookup(u"display") != nullptr);
        expect(std::get<bool>(*a.lookup(u"display")) == false);
        expect(std::get<double>(*a.lookup(u"scriptlevel")) == 2.0);
        expect(a.lookup(u"missing") == nullptr);
        expect(a.ownEntries().size() == 2u);
        expect(a.ownEntries()[0].first == u"display");
        expect(a.ownEntries()[1].first == u"label");
        expect(a.erase(u"display"));
        expect(!a.erase(u"display"));
        expect(std::get<bool>(*a.lookup(u"display")) == true);
        a.set(u"nullable", mml::AttributeNull{});
        expect(std::holds_alternative<mml::AttributeNull>(*a.own(u"nullable")));
    };
};
