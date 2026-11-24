#include <optional>
#include <string>
#include <utility>
#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mathchoice;
import folia.math.core.mml_tree.mml_nodes.maction;
import folia.math.core.mml_tree.mml_nodes.mfenced;
import folia.math.core.mml_tree.mml_nodes.semantics;
import folia.math.core.mml_tree.mml_nodes.tex_atom;

namespace m = folia::math::mml;
using namespace boost::ut;

suite typed_mml_containers = [] {
    "math keeps source linebreak and display inheritance behavior"_test = [] {
        m::MmlDocument document;
        auto &math = static_cast<m::MmlMath &>(document.factory().make(m::NodeKind::math));
        expect(math.linebreakContainer());
        expect(math.linebreakAlign().empty());

        m::AttributeSet mathAttributes;
        mathAttributes.set(u"display", std::u16string{u"block"});
        auto &displayMath = static_cast<m::MmlMath &>(
            document.factory().make(m::NodeKind::math, std::move(mathAttributes)));
        displayMath.setInheritedAttributes({}, false, 0.0, false);
        const auto *displayStyle = displayMath.attributes().lookup(u"displaystyle");
        expect(displayStyle && std::get_if<bool>(displayStyle) && *std::get_if<bool>(displayStyle));

        const auto checkInheritance = [&](bool modeDisplay,
                                          std::optional<std::u16string> display,
                                          std::optional<bool> explicitDisplayStyle,
                                          bool expectedChildStyle) {
            m::MmlDocument caseDocument;
            m::AttributeSet attributes;
            if (modeDisplay)
                attributes.set(u"mode", std::u16string{u"display"});
            if (display)
                attributes.set(u"display", *display);
            if (explicitDisplayStyle)
                attributes.set(u"displaystyle", *explicitDisplayStyle);
            auto &caseMath = static_cast<m::MmlMath &>(
                caseDocument.factory().make(m::NodeKind::math, std::move(attributes)));
            auto &child = caseDocument.factory().make(m::NodeKind::mi);
            caseMath.appendChild(child);
            caseMath.setInheritedAttributes({}, false, 0.0, false);

            const auto *ownDisplay = caseMath.attributes().own(u"display");
            if (display) {
                const auto *value = ownDisplay ? std::get_if<std::u16string>(ownDisplay) : nullptr;
                expect(value && *value == *display);
            } else {
                expect(ownDisplay == nullptr);
            }
            const auto *inheritedDisplay = caseMath.attributes().inherited(u"display");
            if (modeDisplay) {
                const auto *value = inheritedDisplay
                                        ? std::get_if<std::u16string>(inheritedDisplay)
                                        : nullptr;
                expect(value && *value == u"block");
            } else {
                expect(inheritedDisplay == nullptr);
            }

            const auto *ownDisplayStyle = caseMath.attributes().own(u"displaystyle");
            if (explicitDisplayStyle) {
                const auto *value = ownDisplayStyle ? std::get_if<bool>(ownDisplayStyle) : nullptr;
                expect(value && *value == *explicitDisplayStyle);
            } else {
                expect(ownDisplayStyle == nullptr);
            }
            const auto *mathInheritedStyle = caseMath.attributes().inherited(u"displaystyle");
            const auto *mathStyle = mathInheritedStyle
                                        ? std::get_if<bool>(mathInheritedStyle)
                                        : nullptr;
            expect(mathStyle && *mathStyle == expectedChildStyle);
            const auto *childStyle = child.attributes().inherited(u"displaystyle");
            const auto *childStyleValue = childStyle ? std::get_if<bool>(childStyle) : nullptr;
            expect(childStyleValue && *childStyleValue == expectedChildStyle);
        };

        // Values follow the pinned Source math.ts rule: mode writes inherited
        // display=block, while an own display value remains visible and wins
        // for the child-style calculation. Explicit false still falls through
        // to display=block, so the child can inherit true while math owns false.
        checkInheritance(true, std::nullopt, std::nullopt, true);
        checkInheritance(true, std::u16string{u"inline"}, std::nullopt, false);
        checkInheritance(false, std::u16string{u"inline"}, false, false);
        checkInheritance(false, std::u16string{u"inline"}, true, true);
        checkInheritance(false, std::u16string{u"block"}, false, true);
        checkInheritance(false, std::u16string{u"block"}, true, true);
    };

    "nested_math_verification_replaces_inner_at_its_physical_inferred_parent"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &root = factory.make(m::NodeKind::mrow);
        auto &outer = static_cast<m::MmlMath &>(factory.make(m::NodeKind::math));
        auto &inner = static_cast<m::MmlMath &>(factory.make(m::NodeKind::math));
        root.appendChild(outer);
        outer.appendChild(inner);

        expect(root.children().size() == 1_u);
        expect(root.children()[0] == &outer);
        expect(outer.children().size() == 1_u);
        if (outer.children().empty()) return;
        auto *outerBody = outer.children().front();
        expect(outerBody->kind() == m::NodeKind::inferred_mrow);
        expect(outerBody->isInferred());
        expect(outerBody->children().size() == 1_u);
        expect(outerBody->children().front() == &inner);
        expect(inner.parent() == outerBody);

        m::VerifyOptions options;
        options.checkArity = false;
        options.checkAttributes = false;
        options.checkMathvariants = false;
        options.fullErrors = false;
        inner.verifyTree(options);

        expect(root.children().size() == 1_u);
        if (root.children().empty()) return;
        expect(root.children().front() == &outer);
        expect(outer.parent() == &root);
        expect(outer.children().size() == 1_u);
        if (outer.children().empty()) return;
        expect(outer.children().front() == outerBody);
        expect(outerBody->kind() == m::NodeKind::inferred_mrow);
        expect(outerBody->children().size() == 1_u);
        if (outerBody->children().empty()) return;
        auto *error = outerBody->children().front();
        expect(error != nullptr);
        if (!error) return;
        expect(error->kind() == m::NodeKind::merror);
        expect(error->parent() == outerBody);
        expect(inner.parent() == nullptr);

        const auto *message = error->attributes().own(u"data-mjx-message");
        expect(message && std::get_if<std::u16string>(message) &&
               *std::get_if<std::u16string>(message) == u"Improper nesting of math tags");
        const auto *title = error->attributes().own(u"title");
        expect(title && std::get_if<std::u16string>(title) &&
               *std::get_if<std::u16string>(title) == u"Improper nesting of math tags");
        expect(error->children().size() == 1_u);
        if (error->children().empty()) return;
        auto *errorBody = error->children().front();
        expect(errorBody->kind() == m::NodeKind::inferred_mrow);
        expect(errorBody->isInferred());
        expect(errorBody->parent() == error);
        expect(errorBody->children().size() == 1_u);
        if (errorBody->children().empty()) return;
        auto *messageNode = errorBody->children().front();
        expect(messageNode->kind() == m::NodeKind::mtext);
        expect(messageNode->parent() == errorBody);
        expect(messageNode->children().size() == 1_u);
        if (messageNode->children().empty()) return;
        auto *textNode = messageNode->children().front();
        expect(textNode->kind() == m::NodeKind::text);
        expect(textNode->parent() == messageNode);
        expect(textNode->text() == u"math");
    };

    "mathchoice selects branch by display and script level"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &choiceNode = factory.make(m::NodeKind::math_choice);
        auto &choice = static_cast<m::MathChoice &>(choiceNode);
        auto &a = factory.make(m::NodeKind::mi);
        auto &b = factory.make(m::NodeKind::mi);
        auto &c = factory.make(m::NodeKind::mi);
        auto &d = factory.make(m::NodeKind::mi);
        m::MmlNode *branches[]{&a, &b, &c, &d};
        choice.setChildren(branches);
        auto &root = factory.make(m::NodeKind::mrow);
        root.appendChild(choice);
        choice.setInheritedAttributes({}, false, 1.0, false);
        expect(root.children().size() == 1_u);
        expect(root.children()[0] == &c);
        expect(c.parent() == &root);

        auto &fractionalNode = factory.make(m::NodeKind::math_choice);
        auto &fractional = static_cast<m::MathChoice &>(fractionalNode);
        auto &f0 = factory.make(m::NodeKind::mi);
        auto &f1 = factory.make(m::NodeKind::mi);
        auto &f2 = factory.make(m::NodeKind::mi);
        auto &f3 = factory.make(m::NodeKind::mi);
        m::MmlNode *fractionalBranches[]{&f0, &f1, &f2, &f3};
        fractional.setChildren(fractionalBranches);
        auto &fractionalParent = factory.make(m::NodeKind::mrow);
        fractionalParent.appendChild(fractional);
        fractional.setInheritedAttributes({}, false, 0.5, false);
        expect(fractionalParent.children()[0]->kind() == m::NodeKind::mrow);
    };

    "mathchoice display and clamped script levels select the typed branches"_test = [] {
        const auto select = [](bool display, double level, std::size_t count) {
            m::MmlDocument document;
            auto &factory = document.factory();
            auto &choice = static_cast<m::MathChoice &>(factory.make(m::NodeKind::math_choice));
            std::array<m::MmlNode *, 4> branches{};
            for (auto &branch : branches)
                branch = &factory.make(m::NodeKind::mi);
            choice.setChildren(branches);
            auto &parent = factory.make(m::NodeKind::mrow);
            parent.appendChild(choice);
            choice.setInheritedAttributes({}, display, level, false);
            expect(parent.children().size() == 1_u);
            if (parent.children().empty())
                return;
            if (count < branches.size()) {
                expect(parent.children()[0] == branches[count]);
                expect(branches[count]->parent() == &parent);
            } else {
                expect(parent.children()[0]->kind() == m::NodeKind::mrow);
                expect(parent.children()[0]->parent() == &parent);
            }
        };

        select(true, 2.0, 0);   // display always selects branch zero
        select(false, -4.0, 1); // script level clamps to zero
        select(false, 2.0, 3);  // largest script level selects branch three
        select(false, 20.0, 3); // script level clamps to two

        m::MmlDocument emptyDocument;
        auto &emptyChoice = static_cast<m::MathChoice &>(
            emptyDocument.factory().make(m::NodeKind::math_choice));
        auto &emptyParent = emptyDocument.factory().make(m::NodeKind::mrow);
        emptyParent.appendChild(emptyChoice);
        emptyChoice.setInheritedAttributes({}, true, 0.0, false);
        expect(emptyParent.children().size() == 1_u);
        if (!emptyParent.children().empty()) {
            expect(emptyParent.children()[0]->kind() == m::NodeKind::mrow);
            expect(emptyParent.children()[0]->parent() == &emptyParent);
        }
    };

    "maction selection clamps, toggles and delegates selected projections"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &actionNode = factory.make(m::NodeKind::maction);
        auto &action = static_cast<m::MmlMaction &>(actionNode);
        auto &first = factory.make(m::NodeKind::mi);
        auto &second = factory.make(m::NodeKind::mo);
        m::MmlNode *children[]{&first, &second};
        action.setChildren(children);
        expect(&action.selected() == &first);
        action.nextToggleSelection();
        expect(&action.selected() == &second);
        action.nextToggleSelection();
        expect(&action.selected() == &first);
        auto &third = factory.make(m::NodeKind::mrow);
        m::MmlNode *threeChildren[]{&first, &second, &third};
        action.setChildren(threeChildren);
        action.attributes().set(u"selection", std::u16string{u"\u00A00x2"});
        expect(&action.selected() == &second);
        action.nextToggleSelection();
        expect(std::get<double>(*action.attributes().own(u"selection")) == 3.0);
        expect(&action.selected() == &third);

        action.attributes().set(u"selection", std::u16string{u"1e21"});
        expect(&action.selected() == &third);
        action.nextToggleSelection();
        expect(std::get<double>(*action.attributes().own(u"selection")) == 2.0);

        action.attributes().set(u"selection", 1.0e21);
        action.nextToggleSelection();
        expect(std::get<double>(*action.attributes().own(u"selection")) == 2.0);

        action.attributes().set(u"selection", 1.5);
        expect(action.selected().kind() == m::NodeKind::mrow);

        action.attributes().set(u"actiontype", std::u16string{u"tooltip"});
        action.setInheritedAttributes({}, true, 2.0, false);
        const auto *firstDisplay = first.attributes().lookup(u"displaystyle");
        const auto *secondDisplay = second.attributes().lookup(u"displaystyle");
        expect(firstDisplay && std::get_if<bool>(firstDisplay) && *std::get_if<bool>(firstDisplay));
        expect(secondDisplay && std::get_if<bool>(secondDisplay) &&
               !*std::get_if<bool>(secondDisplay));

        action.attributes().set(u"actiontype", std::u16string{u"statusline"});
        m::VerifyOptions verifyOptions;
        verifyOptions.checkArity = false;
        verifyOptions.checkAttributes = true;
        verifyOptions.checkMathvariants = false;
        verifyOptions.fixMmultiscripts = false;
        verifyOptions.fixMtables = false;
        action.verifyTree(verifyOptions);
        expect(action.attributes().own(u"selection") == nullptr);
    };

    "maction rejects non-string actiontype before child inheritance"_test = [] {
        const std::array<m::AttributeValue, 3> malformed{
            m::AttributeNull{}, false, 0.0};
        for (const auto &actionType : malformed) {
            m::MmlDocument document;
            m::AttributeSet attributes;
            attributes.set(u"actiontype", actionType);
            auto &first = document.factory().make(m::NodeKind::mi);
            auto &second = document.factory().make(m::NodeKind::mi);
            const std::array<m::MmlNode *, 2> children{&first, &second};
            auto &action = static_cast<m::MmlMaction &>(
                document.factory().make(m::NodeKind::maction, std::move(attributes), children));

            expect(throws<std::invalid_argument>([&] {
                action.setInheritedAttributes({}, true, 2.0, true);
            })) << "native typed adapter rejects a non-string actiontype";
            for (auto *child : children) {
                expect(child->attributes().inherited(u"displaystyle") == nullptr);
                expect(child->attributes().inherited(u"scriptlevel") == nullptr);
                expect(!child->properties().tex.prime_style.value_or(false));
            }
            action.attributes().set(u"actiontype", u"toggle");
            action.setInheritedAttributes({}, false, 4.0, false);
            for (auto *child : children) {
                const auto *display = child->attributes().inherited(u"displaystyle");
                const auto *level = child->attributes().inherited(u"scriptlevel");
                expect(display && std::get_if<bool>(display) && !*std::get_if<bool>(display));
                expect(level && std::get_if<double>(level) && *std::get_if<double>(level) == 4.0);
            }
        }
    };

    "maction tooltip lowercases strings and isolates tooltip child style"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        m::AttributeSet attributes;
        attributes.set(u"actiontype", u"TOOLTIP");
        auto &first = factory.make(m::NodeKind::mi);
        auto &second = factory.make(m::NodeKind::mi);
        const std::array<m::MmlNode *, 2> children{&first, &second};
        auto &action = static_cast<m::MmlMaction &>(
            factory.make(m::NodeKind::maction, std::move(attributes), children));
        action.setInheritedAttributes({}, true, 2.0, true);

        expect(first.properties().tex.prime_style.value_or(false));
        expect(!second.properties().tex.prime_style.value_or(false));
        const auto *firstDisplay = first.attributes().inherited(u"displaystyle");
        const auto *firstLevel = first.attributes().inherited(u"scriptlevel");
        const auto *secondDisplay = second.attributes().inherited(u"displaystyle");
        const auto *secondLevel = second.attributes().inherited(u"scriptlevel");
        expect(firstDisplay && std::get_if<bool>(firstDisplay) && *std::get_if<bool>(firstDisplay));
        expect(firstLevel && std::get_if<double>(firstLevel) && *std::get_if<double>(firstLevel) == 2.0);
        expect(secondDisplay && std::get_if<bool>(secondDisplay) && !*std::get_if<bool>(secondDisplay));
        expect(secondLevel && std::get_if<double>(secondLevel) && *std::get_if<double>(secondLevel) == 1.0);
    };

    "maction default and explicit toggle inherit the same style to each child"_test = [] {
        const auto checkToggle = [](bool explicitActionType, bool display, double level, bool prime) {
            m::MmlDocument document;
            auto &factory = document.factory();
            m::AttributeSet attributes;
            if (explicitActionType)
                attributes.set(u"actiontype", u"toggle");
            auto &first = factory.make(m::NodeKind::mi);
            auto &second = factory.make(m::NodeKind::mi);
            const std::array<m::MmlNode *, 2> children{&first, &second};
            auto &action = static_cast<m::MmlMaction &>(
                factory.make(m::NodeKind::maction, std::move(attributes), children));
            const auto *actionType = action.attributes().lookup(u"actiontype");
            expect(actionType && std::get_if<std::u16string>(actionType) &&
                   *std::get_if<std::u16string>(actionType) == u"toggle");
            action.setInheritedAttributes({}, display, level, prime);
            for (auto *child : children) {
                const auto *childDisplay = child->attributes().inherited(u"displaystyle");
                const auto *childLevel = child->attributes().inherited(u"scriptlevel");
                expect(childDisplay && std::get_if<bool>(childDisplay) &&
                       *std::get_if<bool>(childDisplay) == display);
                expect(childLevel && std::get_if<double>(childLevel) &&
                       *std::get_if<double>(childLevel) == level);
                expect(child->properties().tex.prime_style.value_or(false) == prime);
            }
        };
        checkToggle(false, true, 3.0, true);
        checkToggle(true, false, 4.0, false);
    };

    "semantics and annotation retain typed defaults and character data state"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &semantics = static_cast<m::MmlSemantics &>(factory.make(m::NodeKind::semantics));
        auto &annotation = static_cast<m::MmlAnnotation &>(factory.make(m::NodeKind::annotation));
        auto &payload = factory.make(m::NodeKind::mi);
        auto &annotationXml =
            static_cast<m::MmlAnnotationXML &>(factory.make(m::NodeKind::annotation_xml));
        m::MmlNode *payloadChildren[]{&payload};
        annotationXml.setChildren(payloadChildren);
        annotationXml.setInheritedAttributes({}, false, 3.0, false);
        expect(semantics.arity() == 1.0);
        expect(semantics.notParent());
        expect(annotation.properties().annotation.character_data.has_value());
        expect(*annotation.properties().annotation.character_data);
        expect(annotation.kind() == m::NodeKind::annotation);
        expect(annotationXml.kind() == m::NodeKind::annotation_xml);
        const auto *payloadLevel = payload.attributes().lookup(u"scriptlevel");
        expect(payloadLevel && std::get_if<double>(payloadLevel) &&
               *std::get_if<double>(payloadLevel) == 0.0);
    };

    "TeXAtom owns ORD intrinsic state and retains infinite arity"_test = [] {
        m::MmlDocument document;
        auto &atom = static_cast<m::MmlTeXAtom &>(document.factory().make(m::NodeKind::tex_atom));
        expect(atom.arity() == -1.0);
        expect(atom.notParent());
        expect(atom.state().working_tex_class == m::TexClass::ord);
    };

    "mfenced fake operators are auxiliary children with the same document parent"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &fenced = static_cast<m::MmlMfenced &>(factory.make(m::NodeKind::mfenced));
        auto &one = factory.make(m::NodeKind::mi);
        auto &two = factory.make(m::NodeKind::mi);
        m::MmlNode *fencedChildren[]{&one, &two};
        fenced.setChildren(fencedChildren);
        auto &root = factory.make(m::NodeKind::math);
        root.appendChild(fenced);
        fenced.setInheritedAttributes({}, true, 0.0, false);
        expect(fenced.children().size() == 2_u);
        expect(fenced.open != nullptr);
        expect(fenced.close != nullptr);
        expect(fenced.separators.size() == 1_u);
        expect(fenced.open->parent() == &fenced);
        expect(fenced.open->state().working_tex_class == m::TexClass::open);
        expect(fenced.open->attributes().lookup(u"fence") &&
               std::get_if<bool>(fenced.open->attributes().lookup(u"fence")) &&
               *std::get_if<bool>(fenced.open->attributes().lookup(u"fence")));
        expect(fenced.open->children().size() == 1_u);
        expect(fenced.open->children()[0]->kind() == m::NodeKind::text);
        expect(fenced.close->state().working_tex_class == m::TexClass::close);
        expect(fenced.state().working_tex_class == m::TexClass::inner);
    };
};
