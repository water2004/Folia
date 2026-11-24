#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.menclose;
import folia.math.core.mml_tree.mml_nodes.merror;
import folia.math.core.mml_tree.mml_nodes.mfrac;
import folia.math.core.mml_tree.mml_nodes.mpadded;
import folia.math.core.mml_tree.mml_nodes.mphantom;
import folia.math.core.mml_tree.mml_nodes.mroot;
import folia.math.core.mml_tree.mml_nodes.msqrt;
import folia.math.core.mml_tree.mml_nodes.mstyle;

using namespace boost::ut;
using namespace folia::math::mml;

suite typed_mml_arithmetic = [] {
    "arithmetic and layout classes own native kind arity and defaults"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &frac = static_cast<MmlMfrac &>(factory.make(NodeKind::mfrac));
        auto &sqrt = static_cast<MmlMsqrt &>(factory.make(NodeKind::msqrt));
        auto &root = static_cast<MmlMroot &>(factory.make(NodeKind::mroot));
        auto &style = static_cast<MmlMstyle &>(factory.make(NodeKind::mstyle));
        auto &error = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        auto &padded = static_cast<MmlMpadded &>(factory.make(NodeKind::mpadded));
        auto &phantom = static_cast<MmlMphantom &>(factory.make(NodeKind::mphantom));
        auto &enclose = static_cast<MmlMenclose &>(factory.make(NodeKind::menclose));
        expect(frac.kind() == NodeKind::mfrac && frac.arity() == 2 && frac.linebreakContainer());
        expect(frac.linebreakAlign().empty());
        expect(sqrt.kind() == NodeKind::msqrt && sqrt.arity() == -1 && sqrt.linebreakContainer());
        expect(root.kind() == NodeKind::mroot && root.arity() == 2 && root.linebreakContainer());
        expect(style.kind() == NodeKind::mstyle && style.arity() == -1);
        expect(error.kind() == NodeKind::merror && error.arity() == -1 &&
               error.linebreakContainer());
        expect(padded.kind() == NodeKind::mpadded && padded.arity() == -1 &&
               padded.linebreakContainer());
        expect(phantom.kind() == NodeKind::mphantom && phantom.arity() == -1);
        expect(enclose.kind() == NodeKind::menclose && enclose.arity() == -1 &&
               enclose.linebreakContainer());
        expect(!frac.texClass().has_value() && sqrt.texClass() == TexClass::ord &&
               root.texClass() == TexClass::ord);
        expect(error.texClass() == TexClass::ord && phantom.texClass() == TexClass::ord &&
               enclose.texClass() == TexClass::ord);

        auto fraction_defaults = MmlMfrac::defaults();
        expect(std::get<std::u16string>(*fraction_defaults.lookup(u"linethickness")) == u"medium");
        expect(std::get<std::u16string>(*fraction_defaults.lookup(u"numalign")) == u"center");
        expect(std::get<bool>(*fraction_defaults.lookup(u"bevelled")) == false);
        auto root_defaults = MmlMroot::defaults();
        expect(std::get<std::u16string>(*root_defaults.lookup(u"data-vertical-align")) ==
               u"bottom");
        auto enclosure_defaults = MmlMenclose::defaults();
        expect(std::get<std::u16string>(*enclosure_defaults.lookup(u"notation")) == u"longdiv");
        auto padding_defaults = MmlMpadded::defaults();
        expect(std::get<std::u16string>(*padding_defaults.lookup(u"width")).empty());
        expect(std::get<double>(*padding_defaults.lookup(u"lspace")) == 0);
    };

    "fraction and root dispatch TeX classes and inherited attributes by role"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &frac = static_cast<MmlMfrac &>(factory.make(NodeKind::mfrac));
        auto &numerator = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        auto &denominator = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        frac.appendChild(numerator);
        frac.appendChild(denominator);
        frac.attributes().set(u"numalign", std::u16string(u"left"));
        frac.attributes().set(u"denomalign", std::u16string(u"right"));
        frac.setInheritedAttributes({}, false, 0, false);
        auto const *num_alignment = numerator.attributes().inherited(u"indentalignfirst");
        auto const *den_alignment = denominator.attributes().inherited(u"indentalignfirst");
        expect(num_alignment && std::get<std::u16string>(*num_alignment) == u"left");
        expect(den_alignment && std::get<std::u16string>(*den_alignment) == u"right");
        expect(frac.setTeXclass(nullptr) == &frac);

        auto &root = static_cast<MmlMroot &>(factory.make(NodeKind::mroot));
        auto &base = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        auto &degree = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        root.appendChild(base);
        root.appendChild(degree);
        expect(root.setTeXclass(nullptr) == &root);
    };

    "style and padded retain structural and semantic dispatch"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &style = static_cast<MmlMstyle &>(factory.make(NodeKind::mstyle));
        auto &body = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        expect(style.children().size() == 1 && style.children().front()->isInferred());
        expect(!style.notParent());
        style.appendChild(body);
        expect(style.notParent());
        auto &nested = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        body.appendChild(nested);
        style.attributes().set(u"scriptlevel", std::u16string(u"+2"));
        style.attributes().set(u"displaystyle", true);
        style.attributes().set(u"data-cramped", true);
        style.setInheritedAttributes({}, false, 1, false);
        auto const *inherited_level = body.attributes().inherited(u"scriptlevel");
        auto const *inherited_display = body.attributes().inherited(u"displaystyle");
        expect(inherited_level && std::get<double>(*inherited_level) == 3);
        expect(inherited_display && std::get<bool>(*inherited_display));

        auto &padded = static_cast<MmlMpadded &>(factory.make(NodeKind::mpadded));
        auto &padded_body = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
        padded.appendChild(padded_body);
        expect(padded.setTeXclass(nullptr) == &padded_body);
        padded.properties().tex.vertical_box = u"vbox";
        expect(padded.setTeXclass(nullptr) == &padded && padded.texClass() == TexClass::ord);
    };

    "style scriptlevel uses ECMAScript string and parseInt semantics"_test = [] {
        const auto inherited_level = [](AttributeValue value, double initial) {
            MmlDocument document;
            auto &factory = document.factory();
            auto &style = static_cast<MmlMstyle &>(factory.make(NodeKind::mstyle));
            auto &body = static_cast<MmlMerror &>(factory.make(NodeKind::merror));
            style.appendChild(body);
            style.attributes().set(u"scriptlevel", std::move(value));
            style.setInheritedAttributes({}, false, initial, false);
            auto const *result = body.attributes().inherited(u"scriptlevel");
            return result ? std::get<double>(*result) : std::numeric_limits<double>::quiet_NaN();
        };
        expect(inherited_level(std::u16string(u"0x10"), 3) == 16);
        expect(inherited_level(std::u16string(u"\u2003+2"), 3) == 5);
        expect(inherited_level(std::u16string(u"-2"), 3) == 1);
        expect(inherited_level(1e21, 3) == 1);
        expect(std::isnan(inherited_level(std::u16string(u"not-a-number"), 3)));
    };

    "fraction and radicals preserve child display level and prime roles"_test = [] {
        for (const auto kind : {NodeKind::mfrac, NodeKind::msqrt, NodeKind::mroot}) {
            for (const bool display : {false, true}) {
                for (const double level : {0.0, 2.0}) {
                    for (const bool prime : {false, true}) {
                        MmlDocument document;
                        auto &factory = document.factory();
                        auto &owner = factory.make(kind);
                        auto &first = factory.make(NodeKind::merror);
                        auto &second = factory.make(NodeKind::merror);
                        owner.appendChild(first);
                        if (kind != NodeKind::msqrt)
                            owner.appendChild(second);
                        owner.setInheritedAttributes({}, display, level, prime);

                        const auto expectStyle = [](MmlNode const &node, bool expected_display,
                                                    double expected_level, bool expected_prime) {
                            const auto *display_value = node.attributes().inherited(u"displaystyle");
                            const auto *level_value = node.attributes().inherited(u"scriptlevel");
                            expect(display_value && std::get_if<bool>(display_value) &&
                                   *std::get_if<bool>(display_value) == expected_display);
                            expect(level_value && std::get_if<double>(level_value) &&
                                   *std::get_if<double>(level_value) == expected_level);
                            expect(node.properties().tex.prime_style.value_or(false) ==
                                   expected_prime);
                        };
                        if (kind == NodeKind::mfrac) {
                            const double child_level = level + ((!display || level > 0) ? 1 : 0);
                            expectStyle(first, false, child_level, prime);
                            expectStyle(second, false, child_level, true);
                        } else if (kind == NodeKind::msqrt) {
                            expectStyle(first, display, level, true);
                            expect(owner.children().size() == 1_u);
                            expect(owner.children().front()->isInferred());
                        } else {
                            expectStyle(first, display, level, true);
                            expectStyle(second, false, level + 2, prime);
                        }
                    }
                }
            }
        }
    };

};
