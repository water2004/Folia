#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.core.mml_tree.mml_nodes.msubsup;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.munderover;
import folia.math.core.mml_tree.operator_class;
import folia.math.core.mml_tree.operator_dictionary;
import folia.math.core.mml_tree.mo_patterns;

using namespace boost::ut;
using namespace folia::math::mml;

namespace {
MmlMo &makeMo(MmlDocument &document, std::u16string text) {
    auto &node = document.factory().make(NodeKind::mo);
    auto &textNode = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
    textNode.setText(text);
    node.appendChild(textNode);
    return dynamic_cast<MmlMo &>(node);
}
} // namespace

suite typed_mml_mo = [] {
    "mo raw and factory initialization preserve Source class-field phases"_test = [] {
        MmlDocument document;
        auto &rawNode = document.make(NodeKind::mo);
        auto *raw = dynamic_cast<MmlMo *>(&rawNode);
        expect(raw != nullptr);
        expect(raw->texClassMode() == MoTexClassMode::unset);
        expect(!raw->lspace.has_value());
        expect(!raw->rspace.has_value());
        expect(!raw->texClass().has_value());

        auto &initialized = makeMo(document, u"+");
        expect(initialized.texClassMode() == MoTexClassMode::cleared);
        expect(initialized.lspace == 5.0 / 18.0);
        expect(initialized.rspace == 5.0 / 18.0);
        expect(initialized.texClass() == TexClass::bin);
        // A getter resolves the dictionary record but Source does not run
        // checkOperatorTable here, so class-field spacing stays at 5/18.
        expect(initialized.lspace == 5.0 / 18.0);
        expect(initialized.rspace == 5.0 / 18.0);
        initialized.setTexClass(TexClass::rel);
        expect(initialized.texClassMode() == MoTexClassMode::explicit_value);
        expect(initialized.state().working_tex_class == TexClass::rel);
        initialized.setTexClass(std::nullopt);
        expect(initialized.texClassMode() == MoTexClassMode::cleared);
        expect(!initialized.state().working_tex_class.has_value());
        expect(initialized.texClass() == TexClass::bin);
    };

    "mo lookup uses ordered forms and typed operator records"_test = [] {
        MmlDocument document;
        auto &mo = makeMo(document, u"+");
        const auto forms = mo.getForms();
        expect(forms.size() == 3_u);
        expect(forms[0] == u"infix");
        auto *definition = operator_dict::findOperatorDefinition(forms[0], u"+");
        expect(definition != nullptr);
        expect(definition->texClass == TexClass::bin);
        expect(definition->left == 4.0);
        expect(definition->right == 4.0);
        const auto resolved = mo.texClass();
        expect(resolved == TexClass::bin);
        expect(mo.lspace == 5.0 / 18.0);
        expect(mo.rspace == 5.0 / 18.0);

        // The source lookup getter has no spacing side effects. Inheritance
        // dispatch invokes checkOperatorTable and applies the typed metrics.
        mo.setInheritedAttributes();
        expect(mo.lspace == definition->left / 18.0);
        expect(mo.rspace == definition->right / 18.0);

        mo.attributes().set(u"form", std::u16string(u"prefix"));
        const auto explicitForms = mo.getForms();
        expect(explicitForms.front() == u"infix");
        mo.setTexClass(std::nullopt);
        expect(mo.texClass() == TexClass::bin);
    };

    "mo explicit spacing attributes and defaults remain distinct"_test = [] {
        MmlDocument document;
        auto &node = document.factory().make(NodeKind::mo);
        auto &mo = dynamic_cast<MmlMo &>(node);
        expect(!mo.hasSpacingAttributes());
        mo.attributes().set(u"lspace", std::u16string(u"0em"));
        expect(mo.hasSpacingAttributes());
        const auto defaults = MmlMo::defaults();
        expect(std::get<std::u16string>(*defaults.defaultValue(u"form")) == u"infix");
        expect(std::get<bool>(*defaults.defaultValue(u"fence")) == false);
    };

    "mo pattern and prime remap services are closed typed data"_test = [] {
        expect(matchMoPattern(MmlMo::pseudoScripts(), u"'²"));
        expect(!matchMoPattern(MmlMo::pseudoScripts(), u"abc"));
        expect(matchMoPattern(MmlMo::opPattern(), u"abc"));
        expect(matchMoPattern(MmlMo::mathaccents(), u"^"));
        expect(MmlMo::remapPrimes().size() == 10_u);

        MmlDocument document;
        auto &mo = makeMo(document, u"''");
        mo.setInheritedAttributes();
        expect(mo.properties().tex.primes == u"′′");
        expect(mo.properties().tex.pseudo_script == true);

        auto &superscriptNode = document.factory().make(NodeKind::msup);
        auto &superscript = dynamic_cast<MmlMsubsup &>(superscriptNode);
        auto &base = document.factory().make(NodeKind::mi);
        auto &scriptedMo = makeMo(document, u"'²");
        superscript.appendChild(base);
        superscript.appendChild(scriptedMo);
        scriptedMo.setInheritedAttributes();
        expect(superscript.isKind(NodeKind::msubsup));
        expect(!superscript.isKind(NodeKind::msub));
        expect(scriptedMo.properties().tex.pseudo_script == false);
    };

    "shared TeX-class adjustment applies binary endpoint and relation rules"_test = [] {
        MmlDocument document;
        auto &rowNode = document.factory().make(NodeKind::mrow);
        auto &row = dynamic_cast<MmlMrow &>(rowNode);
        auto &left = makeMo(document, u"x");
        auto &op = makeMo(document, u"+");
        auto &right = makeMo(document, u"x");
        row.appendChild(left);
        row.appendChild(op);
        row.appendChild(right);
        left.setTexClass(TexClass::ord);
        op.setTexClass(TexClass::bin);
        expect(adjustOperatorTeXclass(op, &left) == &op);
        expect(op.prevClass() == TexClass::ord);
        expect(op.texClass() == TexClass::bin);

        op.setTexClass(TexClass::rel);
        left.setTexClass(TexClass::bin);
        expect(adjustOperatorTeXclass(op, &left) == &op);
        expect(op.prevClass() == TexClass::ord);
        expect(left.texClass() == TexClass::ord);
    };
    "mo forms retain row position and explicit form precedence"_test = [] {
        MmlDocument singletonDocument;
        auto &singletonRow = dynamic_cast<MmlMrow &>(
            singletonDocument.factory().make(NodeKind::mrow));
        auto &singleton = makeMo(singletonDocument, u"+");
        singletonRow.appendChild(singleton);
        expect(static_cast<bool>(singleton.getForms() ==
               std::vector<std::u16string>{u"infix", u"prefix", u"postfix"}));
        singleton.attributes().set(u"form", std::u16string(u"postfix"));
        singleton.setInheritedAttributes();
        const auto *selectedForm = singleton.attributes().inherited(u"form");
        expect(selectedForm != nullptr);
        const auto *selectedText = selectedForm ? std::get_if<std::u16string>(selectedForm) : nullptr;
        expect(selectedText != nullptr);
        if (selectedText)
            expect(static_cast<bool>(*selectedText == u"postfix"));

        MmlDocument document;
        auto &row = dynamic_cast<MmlMrow &>(document.factory().make(NodeKind::mrow));
        auto &left = makeMo(document, u"x");
        auto &middle = makeMo(document, u"+");
        auto &right = makeMo(document, u"y");
        row.appendChild(left);
        row.appendChild(middle);
        row.appendChild(right);
        expect(static_cast<bool>(left.getForms() ==
               std::vector<std::u16string>{u"prefix", u"infix", u"postfix"}));
        expect(static_cast<bool>(middle.getForms() ==
               std::vector<std::u16string>{u"infix", u"prefix", u"postfix"}));
        expect(static_cast<bool>(right.getForms() ==
               std::vector<std::u16string>{u"postfix", u"infix", u"prefix"}));
    };
    "mo forms select prefix and postfix at both ends of a two child row"_test = [] {
        MmlDocument document;
        auto &row = document.factory().make(NodeKind::mrow);
        auto &left = makeMo(document, u"(");
        auto &right = makeMo(document, u")");
        document.append(row, left);
        document.append(row, right);

        expect(static_cast<bool>(left.getForms() ==
               std::vector<std::u16string>{u"prefix", u"infix", u"postfix"}));
        expect(static_cast<bool>(right.getForms() ==
               std::vector<std::u16string>{u"postfix", u"infix", u"prefix"}));
    };

    "mo accent predicates follow script roles and typed attribute presence"_test = [] {
        MmlDocument document;
        auto &mover = dynamic_cast<MmlMover &>(document.factory().make(NodeKind::mover));
        auto &moverBase = document.factory().make(NodeKind::mi);
        auto &over = makeMo(document, u"^");
        mover.appendChild(moverBase);
        mover.appendChild(over);
        over.attributes().set(u"accent", true);
        expect(over.isAccent());
        over.attributes().set(u"accent", std::u16string(u"yes"));
        expect(over.isAccent());
        over.attributes().set(u"accent", false);
        expect(!over.isAccent());
        mover.attributes().set(u"accent", std::u16string(u"yes"));
        expect(!over.isAccent());
        over.attributes().set(u"accent", true);
        mover.attributes().set(u"accent", false);
        expect(!over.isAccent());
        mover.attributes().set(u"accent", std::u16string(u"yes"));
        expect(!over.isAccent());

        MmlDocument underDocument;
        auto &munder = dynamic_cast<MmlMunder &>(
            underDocument.factory().make(NodeKind::munder));
        auto &underBase = underDocument.factory().make(NodeKind::mi);
        auto &under = makeMo(underDocument, u"^");
        munder.appendChild(underBase);
        munder.appendChild(under);
        under.attributes().set(u"accent", true);
        expect(under.isAccent());
        munder.attributes().set(u"accentunder", true);
        expect(!under.isAccent());
        munder.attributes().set(u"accentunder", AttributeNull{});
        expect(!under.isAccent());

        MmlDocument multiUnderDocument;
        auto &multiUnderParent = dynamic_cast<MmlMunderover &>(
            multiUnderDocument.factory().make(NodeKind::munderover));
        auto &multiUnderBase = multiUnderDocument.factory().make(NodeKind::mi);
        auto &multiUnder = makeMo(multiUnderDocument, u"^");
        auto &multiOver = multiUnderDocument.factory().make(NodeKind::mi);
        multiUnderParent.appendChild(multiUnderBase);
        multiUnderParent.appendChild(multiUnder);
        multiUnderParent.appendChild(multiOver);
        multiUnder.attributes().set(u"accent", true);
        multiUnderParent.attributes().set(u"accent", true);
        expect(multiUnder.isAccent()) << "the under operator uses accentunder, not the parent's accent";
        multiUnderParent.attributes().set(u"accentunder", false);
        expect(!multiUnder.isAccent());

        MmlDocument multiOverDocument;
        auto &multiOverParent = dynamic_cast<MmlMunderover &>(
            multiOverDocument.factory().make(NodeKind::munderover));
        auto &multiOverBase = multiOverDocument.factory().make(NodeKind::mi);
        auto &multiUnderNode = multiOverDocument.factory().make(NodeKind::mi);
        auto &multiOverMo = makeMo(multiOverDocument, u"^");
        multiOverParent.appendChild(multiOverBase);
        multiOverParent.appendChild(multiUnderNode);
        multiOverParent.appendChild(multiOverMo);
        multiOverMo.attributes().set(u"accent", true);
        multiOverParent.attributes().set(u"accentunder", false);
        expect(multiOverMo.isAccent());
    };

    "mo math accent inheritance preserves under width and base rules"_test = [] {
        MmlDocument accentDocument;
        auto &accentParent = dynamic_cast<MmlMunderover &>(
            accentDocument.factory().make(NodeKind::munderover));
        auto &accentBase = accentDocument.factory().make(NodeKind::mi);
        auto &accent = makeMo(accentDocument, u"^");
        auto &accentOver = accentDocument.factory().make(NodeKind::mi);
        accentParent.appendChild(accentBase);
        accentParent.appendChild(accent);
        accentParent.appendChild(accentOver);
        accent.setInheritedAttributes();
        expect(accent.properties().tex.math_accent == true);

        MmlDocument widthDocument;
        auto &widthParent = dynamic_cast<MmlMunderover &>(
            widthDocument.factory().make(NodeKind::munderover));
        auto &widthBase = widthDocument.factory().make(NodeKind::mi);
        auto &widthAccent = makeMo(widthDocument, u"→");
        auto &widthOver = widthDocument.factory().make(NodeKind::mi);
        widthParent.appendChild(widthBase);
        widthParent.appendChild(widthAccent);
        widthParent.appendChild(widthOver);
        widthAccent.setInheritedAttributes();
        expect(widthAccent.properties().tex.math_accent == false);

        MmlDocument explicitDocument;
        auto &explicitParent = dynamic_cast<MmlMunderover &>(
            explicitDocument.factory().make(NodeKind::munderover));
        auto &explicitBase = explicitDocument.factory().make(NodeKind::mi);
        auto &explicitAccent = makeMo(explicitDocument, u"^");
        auto &explicitOver = explicitDocument.factory().make(NodeKind::mi);
        explicitParent.appendChild(explicitBase);
        explicitParent.appendChild(explicitAccent);
        explicitParent.appendChild(explicitOver);
        explicitAccent.properties().tex.math_accent = false;
        explicitAccent.setInheritedAttributes();
        expect(explicitAccent.properties().tex.math_accent == false);

        MmlDocument baseDocument;
        auto &baseParent = dynamic_cast<MmlMunderover &>(
            baseDocument.factory().make(NodeKind::munderover));
        auto &baseAccent = makeMo(baseDocument, u"^");
        auto &baseUnder = baseDocument.factory().make(NodeKind::mi);
        auto &baseOver = baseDocument.factory().make(NodeKind::mi);
        baseParent.appendChild(baseAccent);
        baseParent.appendChild(baseUnder);
        baseParent.appendChild(baseOver);
        baseAccent.setInheritedAttributes();
        expect(!baseAccent.properties().tex.math_accent.has_value());

        MmlDocument overDocument;
        auto &overParent = dynamic_cast<MmlMunderover &>(
            overDocument.factory().make(NodeKind::munderover));
        auto &overBase = overDocument.factory().make(NodeKind::mi);
        auto &overUnder = overDocument.factory().make(NodeKind::mi);
        auto &overAccent = makeMo(overDocument, u"^");
        overParent.appendChild(overBase);
        overParent.appendChild(overUnder);
        overParent.appendChild(overAccent);
        overAccent.setInheritedAttributes();
        // Pinned Source's isOver branch tests under.coreMO() against this mo.
        expect(!overAccent.properties().tex.math_accent.has_value());
    };
    // Migrated from the handwritten first case in reference_mo_order.cpp.
    // Preserve operator spacing through the phantom copy inside mpadded.
    "mo spacing survives mpadded phantom copy"_test = [] {
        MmlDocument document;
        const auto token = [&](NodeKind kind, std::u16string text) -> MmlNode & {
            auto &node = document.factory().make(kind);
            auto &textNode = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
            textNode.setText(text);
            node.appendChild(textNode);
            return node;
        };
        const auto group = [&](NodeKind kind, std::initializer_list<MmlNode *> children)
            -> MmlNode & {
            auto &node = document.factory().make(kind);
            for (auto *child : children) node.appendChild(*child);
            return node;
        };
        auto &x = token(NodeKind::mi, u"x");
        auto &plus = token(NodeKind::mo, u"+");
        auto &y = token(NodeKind::mi, u"y");
        auto &copy = group(NodeKind::inferred_mrow, {&x, &plus, &y});
        auto &phantom = group(NodeKind::mphantom, {&copy});
        auto &phantomRow = group(NodeKind::inferred_mrow, {&phantom});
        auto &padded = group(NodeKind::mpadded, {&phantomRow});
        padded.attributes().set(u"style", std::u16string(u"border: 0.1em solid; border-top: none"));
        padded.attributes().set(u"height", std::u16string(u".2em"));
        padded.attributes().set(u"depth", 0.0);
        auto &visibleX = token(NodeKind::mi, u"x");
        auto &visiblePlus = token(NodeKind::mo, u"+");
        auto &visibleY = token(NodeKind::mi, u"y");
        auto &visible = group(NodeKind::mrow, {&visibleX, &visiblePlus, &visibleY});
        auto &under = group(NodeKind::munder, {&visible, &padded});
        under.attributes().set(u"displaystyle", true);
        under.attributes().set(u"accentunder", true);
        under.setInheritedAttributes({}, true, 0, false);
        expect(under.setTeXclass(nullptr) == &under);
        expect(plus.prevClass() == TexClass::ord);
        expect(plus.texSpacing() == "mediummathspace");
        expect(y.prevClass() == TexClass::bin);
        expect(y.texSpacing() == "mediummathspace");
    };

};
