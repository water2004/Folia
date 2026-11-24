#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.core.mml_tree.mml_nodes.mtd;
import folia.math.core.mml_tree.mml_nodes.mpadded;
import folia.math.core.mml_tree.mml_nodes.merror;
import folia.math.core.mml_tree.mml_nodes.mfrac;

namespace {
using namespace boost::ut;
namespace m = folia::math::mml;

suite native_mml_base = [] {
    "mml_raw_allocation_defers_typed_semantic_initialization"_test = [] {
        m::MmlDocument document;
        auto &raw = document.make(m::NodeKind::mi);
        expect(raw.state() == m::MmlIntrinsicState{})
            << "raw document allocation records ownership/kind only";
        expect(raw.properties() == m::NodeProperties{});
        expect(raw.attributes().ownEntries().empty())
            << "raw document allocation does not install Source factory defaults";

        auto &initialized = document.factory().make(m::NodeKind::mi);
        expect(initialized.state().working_tex_class == m::TexClass::ord)
            << "factory path runs initializeMml after adoption";
        auto &table = document.factory().make(m::NodeKind::mtable);
        expect(table.properties().table.use_height == true)
            << "typed properties initialize once on the factory path";
    };

    "mml_base_origin_exclusion_and_stop_rules"_test = [] {
        m::MmlDocument document;
        auto &padded = document.factory().make(m::NodeKind::mpadded);
        m::AttributeList blocked{{u"width", m::NodeKind::mstyle, std::u16string(u"5em")}};
        padded.setInheritedAttributes(blocked);
        expect(padded.attributes().inherited(u"width") == nullptr)
            << "mstyle width is excluded from mpadded";

        auto &cell = document.factory().make(m::NodeKind::mtd);
        auto &nested = document.factory().make(m::NodeKind::mtd);
        cell.appendChild(nested);
        m::AttributeList tableAttrs{
            {u"columnalign", m::NodeKind::mtable, std::u16string(u"right")}};
        cell.setInheritedAttributes(tableAttrs);
        expect(cell.attributes().inherited(u"columnalign") != nullptr)
            << "mtd receives its own column alignment";
        expect(nested.attributes().inherited(u"columnalign") == nullptr)
            << "mtd stops column alignment before descendants";
    };

    "mml_base_omitted_verify_is_noop_and_copy_omits_id"_test = [] {
        m::MmlDocument document;
        auto &row = document.factory().make(m::NodeKind::mrow);
        row.attributes().set(u"id", std::u16string(u"original"));
        row.attributes().set(u"data-marker", std::u16string(u"kept"));
        row.verifyTree();
        auto &copied = row.copy();
        expect(copied.attributes().own(u"id") == nullptr);
        expect(copied.attributes().own(u"data-marker") != nullptr);
    };

    "mml_base_spacing_table_finite_arity_and_inferred_rows"_test = [] {
        m::MmlDocument document;
        auto &previous = document.factory().make(m::NodeKind::mi);
        auto &current = document.factory().make(m::NodeKind::mo);
        previous.setTexClass(m::TexClass::ord);
        current.setTexClass(m::TexClass::rel);
        expect(current.setTeXclass(&previous) == &current);
        expect(current.texSpacing() == "thickmathspace");

        auto &error = document.factory().make(m::NodeKind::merror);
        auto &errorChild = document.factory().make(m::NodeKind::mi);
        error.appendChild(errorChild);
        expect(error.children().size() == 1);
        expect(error.children().front()->isInferred());
        expect(error.children().front()->children().size() == 1);
        expect(error.children().front()->children().front() == &errorChild);

        auto &fraction = document.factory().make(m::NodeKind::mfrac);
        auto &sourceRow = document.factory().make(m::NodeKind::inferred_mrow);
        auto &sourceChild = document.factory().make(m::NodeKind::mi);
        sourceRow.appendChild(sourceChild);
        const std::array<m::MmlNode *, 1> initial{&sourceRow};
        fraction.setChildren(initial);
        expect(fraction.children().size() == 1);
        expect(!fraction.children().front()->isInferred());
        expect(fraction.children().front()->children().front() == &sourceChild);
        fraction.setInheritedAttributes();
        expect(fraction.children().size() == 2) << "finite arity is repaired with mrows";
        for (auto const *child : fraction.children())
            expect(child->kind() == m::NodeKind::mrow)
                << "finite-arity repair creates mrow children";

        auto &emptyFraction = document.factory().make(m::NodeKind::mfrac);
        emptyFraction.setInheritedAttributes();
        expect(emptyFraction.children().size() == 2) << "empty fraction repairs both child slots";
        for (auto const *child : emptyFraction.children())
            expect(child->kind() == m::NodeKind::mrow)
                << "each missing fraction child is a native mrow";

        auto &inferred = document.factory().make(m::NodeKind::inferred_mrow);
        auto &child = document.factory().make(m::NodeKind::mi);
        inferred.appendChild(child);
        expect(inferred.children().size() == 1);
        expect(inferred.children().front() == &child);
        expect(child.parent() == &inferred);

        auto &arbitrary = document.factory().make(m::NodeKind::mrow);
        auto &sourceInferred = document.factory().make(m::NodeKind::inferred_mrow);
        auto &first = document.factory().make(m::NodeKind::mi);
        auto &second = document.factory().make(m::NodeKind::mi);
        sourceInferred.appendChild(first);
        sourceInferred.appendChild(second);
        arbitrary.appendChild(sourceInferred);
        expect(arbitrary.children().size() == 2);
        expect(sourceInferred.children().empty());
        expect(first.parent() == &arbitrary && second.parent() == &arbitrary);
    };
    "mml_base_tex_spacing_complete_class_matrix_and_script_suppression"_test = [] {
        constexpr std::array<m::TexClass, 8> classes{
            m::TexClass::ord, m::TexClass::op, m::TexClass::bin, m::TexClass::rel,
            m::TexClass::open, m::TexClass::close, m::TexClass::punct, m::TexClass::inner};
        constexpr std::array<std::array<int, 8>, 8> spacing{{
            {{0, -1, 2, 3, 0, 0, 0, 1}}, {{-1, -1, 0, 3, 0, 0, 0, 1}},
            {{2, 2, 0, 0, 2, 0, 0, 2}}, {{3, 3, 0, 0, 3, 0, 0, 3}},
            {{0, 0, 0, 0, 0, 0, 0, 0}}, {{0, -1, 2, 3, 0, 0, 0, 1}},
            {{1, 1, 0, 1, 1, 1, 1, 1}}, {{1, -1, 2, 3, 1, 0, 1, 1}}}};
        constexpr std::array<std::string_view, 4> lengths{
            "", "thinmathspace", "mediummathspace", "thickmathspace"};
        for (std::size_t previous = 0; previous < classes.size(); ++previous) {
            for (std::size_t currentClass = 0; currentClass < classes.size(); ++currentClass) {
                m::MmlDocument document;
                auto &current = document.factory().make(m::NodeKind::mo);
                current.setPrevClass(classes[previous]);
                current.setTexClass(classes[currentClass]);
                current.attributes().set(u"scriptlevel", 0.0);
                const int cell = spacing[previous][currentClass];
                expect(current.texSpacing() == lengths[static_cast<std::size_t>(std::abs(cell))]);

                current.setPrevLevel(1.0);
                expect(current.texSpacing() == (cell < 0
                    ? lengths[static_cast<std::size_t>(std::abs(cell))] : std::string_view{}));
                current.setPrevLevel(0.0);
                current.attributes().set(u"scriptlevel", 1.0);
                expect(current.texSpacing() == (cell < 0
                    ? lengths[static_cast<std::size_t>(std::abs(cell))] : std::string_view{}));
            }
        }
        m::MmlDocument document;
        auto &none = document.factory().make(m::NodeKind::mo);
        none.setPrevClass(m::TexClass::none);
        none.setTexClass(m::TexClass::rel);
        expect(none.texSpacing().empty());
        none.setPrevClass(m::TexClass::ord);
        none.setTexClass(m::TexClass::none);
        expect(none.texSpacing().empty());
    };

    "mml_base_variant_validation_preserves_source_truthiness_and_value_text"_test = [] {
        m::MmlDocument document;
        auto &row = document.factory().make(m::NodeKind::mrow);
        m::AttributeSet emptyVariant;
        emptyVariant.set(u"mathvariant", std::u16string{});
        auto &empty = document.factory().make(m::NodeKind::mi, std::move(emptyVariant));
        row.appendChild(empty);
        m::VerifyOptions options;
        options.checkAttributes = false;
        options.checkArity = false;
        options.checkMathvariants = true;
        empty.verifyTree(options);
        expect(row.children().front() == &empty) << "empty variant is falsy in Source";

        m::AttributeSet numericVariant;
        numericVariant.set(u"mathvariant", 5.0);
        auto &invalid = document.factory().make(m::NodeKind::mi, std::move(numericVariant));
        row.appendChild(invalid);
        invalid.verifyTree(options);
        expect(row.children().back()->isKind(m::NodeKind::merror));
        expect(row.children().back()->attributes().own(u"data-mjx-message") != nullptr);
        const auto *message = row.children().back()->attributes().own(u"data-mjx-message");
        expect(message && std::get<std::u16string>(*message) == u"Invalid mathvariant: 5");
    };
    "mml_numeric_attribute_conversion_uses_ecmascript_string_number"_test = [] {
        m::MmlDocument document;
        auto &current = document.factory().make(m::NodeKind::mi);

        m::AttributeSet unicodeWhitespaceHex;
        unicodeWhitespaceHex.set(u"scriptlevel", std::u16string(u"\u00a0 0x10 \u3000"));
        auto &hex = document.factory().make(m::NodeKind::mi, std::move(unicodeWhitespaceHex));
        current.setTeXclass(&hex);
        expect(current.prevLevel() == 16.0)
            << "number conversion accepts ECMAScript whitespace and hexadecimal strings";

        m::AttributeSet binary;
        binary.set(u"scriptlevel", std::u16string(u"0b101"));
        auto &binaryLevel = document.factory().make(m::NodeKind::mi, std::move(binary));
        current.setTeXclass(&binaryLevel);
        expect(current.prevLevel() == 5.0) << "number conversion accepts binary string literals";

        m::AttributeSet boolean;
        boolean.set(u"scriptlevel", true);
        auto &booleanLevel = document.factory().make(m::NodeKind::mi, std::move(boolean));
        current.setTeXclass(&booleanLevel);
        expect(current.prevLevel() == 1.0) << "boolean values retain numeric conversion semantics";
    };
    "mml_base_mathvariant_error_replacement_is_stable_on_reverification"_test = [] {
        m::MmlDocument document;
        auto &row = document.factory().make(m::NodeKind::mrow);
        m::AttributeSet invalidAttributes;
        invalidAttributes.set(u"mathvariant", 5.0);
        auto &invalid = document.factory().make(m::NodeKind::mi, std::move(invalidAttributes));
        row.appendChild(invalid);

        m::VerifyOptions options;
        options.checkArity = false;
        options.checkAttributes = false;
        options.checkMathvariants = true;
        invalid.verifyTree(options);
        expect(row.children().size() == 1_u);
        if (row.children().size() != 1)
            return;
        auto *error = row.children().front();
        expect(error != &invalid);
        expect(error->kind() == m::NodeKind::merror);
        expect(error->parent() == &row);
        expect(invalid.parent() == nullptr);
        expect(error->attributes().own(u"data-mjx-message") != nullptr);

        row.verifyTree(options);
        expect(row.children().size() == 1_u);
        if (row.children().size() != 1)
            return;
        expect(row.children().front() == error)
            << "reverification keeps the repaired error subtree stable";
        expect(error->parent() == &row);
    };
    "inheritAttributesFrom uses own mathsize over inherited and copies source style values without mutation"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &source = factory.make(m::NodeKind::mrow);
        auto &destination = factory.make(m::NodeKind::mrow);
        source.attributes().set(u"mathsize", u"own-size");
        source.attributes().setInherited(u"mathsize", u"inherited-size");
        source.attributes().set(u"displaystyle", true);
        source.attributes().set(u"scriptlevel", 3.0);
        source.properties().tex.prime_style = true;

        destination.inheritAttributesFrom(source);

        const auto *size = destination.attributes().inherited(u"mathsize");
        expect(size && std::get_if<std::u16string>(size) && *std::get_if<std::u16string>(size) == u"own-size");
        const auto *display = destination.attributes().inherited(u"displaystyle");
        expect(display && std::get_if<bool>(display) && *std::get_if<bool>(display));
        const auto *level = destination.attributes().inherited(u"scriptlevel");
        expect(level && std::get_if<double>(level) && *std::get_if<double>(level) == 3.0);
        expect(destination.properties().tex.prime_style.value_or(false));

        expect(std::get<std::u16string>(*source.attributes().own(u"mathsize")) == u"own-size");
        expect(std::get<std::u16string>(*source.attributes().inherited(u"mathsize")) == u"inherited-size");
        expect(std::get<bool>(*source.attributes().own(u"displaystyle")));
        expect(std::get<double>(*source.attributes().own(u"scriptlevel")) == 3.0);
        expect(source.properties().tex.prime_style.value_or(false));
    };

    "inheritAttributesFrom copies inherited-only mathsize and source style values without mutation"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &source = factory.make(m::NodeKind::mrow);
        auto &destination = factory.make(m::NodeKind::mrow);
        source.attributes().setInherited(u"mathsize", u"inherited-size");
        source.attributes().set(u"displaystyle", false);
        source.attributes().set(u"scriptlevel", 2.0);
        source.properties().tex.prime_style = false;

        destination.inheritAttributesFrom(source);

        const auto *size = destination.attributes().inherited(u"mathsize");
        expect(size && std::get_if<std::u16string>(size) && *std::get_if<std::u16string>(size) == u"inherited-size");
        const auto *display = destination.attributes().inherited(u"displaystyle");
        expect(display && std::get_if<bool>(display) && !*std::get_if<bool>(display));
        const auto *level = destination.attributes().inherited(u"scriptlevel");
        expect(level && std::get_if<double>(level) && *std::get_if<double>(level) == 2.0);
        expect(!destination.properties().tex.prime_style.value_or(false));

        expect(source.attributes().own(u"mathsize") == nullptr);
        expect(std::get<std::u16string>(*source.attributes().inherited(u"mathsize")) == u"inherited-size");
        expect(!std::get<bool>(*source.attributes().own(u"displaystyle")));
        expect(std::get<double>(*source.attributes().own(u"scriptlevel")) == 2.0);
        expect(!source.properties().tex.prime_style.value_or(false));
    };

    "inheritAttributesFrom does not copy default-only mathsize"_test = [] {
        m::MmlDocument document;
        auto &factory = document.factory();
        auto &source = factory.make(m::NodeKind::mrow);
        auto &destination = factory.make(m::NodeKind::mrow);
        source.attributes().setDefault(u"mathsize", u"default-size");
        source.attributes().set(u"displaystyle", true);
        source.attributes().set(u"scriptlevel", 1.0);
        source.properties().tex.prime_style = true;

        destination.inheritAttributesFrom(source);

        expect(source.attributes().own(u"mathsize") == nullptr);
        expect(source.attributes().inherited(u"mathsize") == nullptr);
        expect(source.attributes().defaultValue(u"mathsize") != nullptr);
        expect(destination.attributes().inherited(u"mathsize") == nullptr);
        const auto *display = destination.attributes().inherited(u"displaystyle");
        expect(display && std::get_if<bool>(display) && *std::get_if<bool>(display));
        const auto *level = destination.attributes().inherited(u"scriptlevel");
        expect(level && std::get_if<double>(level) && *std::get_if<double>(level) == 1.0);
        expect(destination.properties().tex.prime_style.value_or(false));

        expect(std::get<bool>(*source.attributes().own(u"displaystyle")));
        expect(std::get<double>(*source.attributes().own(u"scriptlevel")) == 1.0);
        expect(source.properties().tex.prime_style.value_or(false));
    };
};
} // namespace
